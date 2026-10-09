#!/usr/bin/env bash
# Baut qemu-system-aarch64 für Android mit dem Android-NDK.
#
# Aufruf:  qemu/build-android.sh <abi>
#          abi = arm64-v8a (Handy) oder x86_64 (Test-Emulator)
#
# Benötigt: ANDROID_NDK (Pfad zum NDK) oder ANDROID_HOME + NDK_VERSION,
#           QEMU_VERSION, GLIB_VERSION, außerdem meson, ninja, pkg-config, python3.
# Ergebnis: out/qemu-<abi>/libqemu.so (ein ausführbares Programm, nur so benannt,
#           damit Android es mit der App installiert).
set -euo pipefail

ABI="${1:?ABI fehlt (arm64-v8a oder x86_64)}"
API="${ANDROID_API:-31}"
QEMU_VERSION="${QEMU_VERSION:?QEMU_VERSION fehlt}"
GLIB_VERSION="${GLIB_VERSION:?GLIB_VERSION fehlt}"
# Absichtlich nicht $ANDROID_NDK: Auf GitHub zeigt das auf ein anderes, vorinstalliertes NDK.
NDK="${HAKONIWA_NDK:-${ANDROID_HOME:?}/ndk/${NDK_VERSION:?}}"

case "$ABI" in
  arm64-v8a) TRIPLE=aarch64-linux-android; CPU=aarch64 ;;
  x86_64)    TRIPLE=x86_64-linux-android;  CPU=x86_64 ;;
  *) echo "Unbekannte ABI: $ABI" >&2; exit 1 ;;
esac

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/build/android-$ABI"
PREFIX="$WORK/prefix"
OUT="$ROOT/out/qemu-$ABI"
DOWNLOADS="$ROOT/build/downloads"
JOBS="$(nproc)"

rm -rf "$WORK"
mkdir -p "$WORK" "$PREFIX/lib/pkgconfig" "$OUT" "$DOWNLOADS"

TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"

# Thread-lokale Variablen werden "emuliert" (wie bei Termux): Das läuft auf jeder
# Android-Version und lässt sich deshalb auch in der Termux-Docker-Umgebung
# (Android 9) auf einem ARM-Rechner testen.
export CC="$TOOLCHAIN/bin/${TRIPLE}${API}-clang"
export CXX="$TOOLCHAIN/bin/${TRIPLE}${API}-clang++"
export AR="$TOOLCHAIN/bin/llvm-ar"
export NM="$TOOLCHAIN/bin/llvm-nm"
export RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
export STRIP="$TOOLCHAIN/bin/llvm-strip"
export OBJCOPY="$TOOLCHAIN/bin/llvm-objcopy"
export READELF="$TOOLCHAIN/bin/llvm-readelf"
export LD="$TOOLCHAIN/bin/ld.lld"

step() { echo; echo "===== $* ====="; }

download() {  # download <url> <datei>
  if [ ! -s "$DOWNLOADS/$2" ]; then
    curl -fsSL --retry 5 --retry-delay 5 -o "$DOWNLOADS/$2.tmp" "$1"
    mv "$DOWNLOADS/$2.tmp" "$DOWNLOADS/$2"
  fi
}

step "Werkzeuge"
"$CC" --version | head -1
meson --version
ninja --version

# --- pkg-config: nur unsere eigenen Bibliotheken, immer statisch -------------
cat > "$WORK/pkg-config" <<EOF
#!/bin/sh
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH=""
exec pkg-config --static "\$@"
EOF
chmod +x "$WORK/pkg-config"
export PKG_CONFIG="$WORK/pkg-config"

# zlib liegt schon im NDK
cat > "$PREFIX/lib/pkgconfig/zlib.pc" <<EOF
Name: zlib
Description: zlib aus dem Android-NDK
Version: 1.3.1
Libs: -lz
Cflags:
EOF

# --- glib (statisch) ----------------------------------------------------------
step "glib $GLIB_VERSION herunterladen"
GLIB_MINOR="${GLIB_VERSION%.*}"
download "https://download.gnome.org/sources/glib/$GLIB_MINOR/glib-$GLIB_VERSION.tar.xz" "glib-$GLIB_VERSION.tar.xz"
download "https://download.gnome.org/sources/glib/$GLIB_MINOR/glib-$GLIB_VERSION.sha256sum" "glib-$GLIB_VERSION.sha256sum"
(cd "$DOWNLOADS" && grep " glib-$GLIB_VERSION.tar.xz\$" "glib-$GLIB_VERSION.sha256sum" | sha256sum -c -)
tar -C "$WORK" -xf "$DOWNLOADS/glib-$GLIB_VERSION.tar.xz"

cat > "$WORK/cross-glib.ini" <<EOF
[host_machine]
system = 'android'
cpu_family = '$CPU'
cpu = '$CPU'
endian = 'little'

[properties]
sys_root = '$TOOLCHAIN/sysroot'

[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
nm = '$NM'
ranlib = '$RANLIB'
pkg-config = '$WORK/pkg-config'

[built-in options]
c_args = ['-fPIC', '-femulated-tls']
cpp_args = ['-fPIC', '-femulated-tls']
EOF

step "glib bauen"
meson setup "$WORK/glib-build" "$WORK/glib-$GLIB_VERSION" \
  --cross-file "$WORK/cross-glib.ini" \
  --prefix "$PREFIX" --libdir lib \
  --buildtype release --default-library static --wrap-mode default \
  -Dtests=false -Dinstalled_tests=false -Dnls=disabled -Dintrospection=disabled \
  -Dlibmount=disabled -Dselinux=disabled -Dxattr=false -Dman-pages=disabled \
  -Ddocumentation=false -Ddtrace=disabled -Dsystemtap=disabled -Dsysprof=disabled \
  -Dglib_debug=disabled -Dlibelf=disabled -Dbsymbolic_functions=false
meson compile -C "$WORK/glib-build"
meson install -C "$WORK/glib-build" --quiet

# Alle statischen Hilfsbibliotheken (pcre2, intl, ...) in den Prefix kopieren
find "$WORK/glib-build" -name '*.a' -exec cp --update=none {} "$PREFIX/lib/" \;
ls -la "$PREFIX/lib"/*.a

GLIB_LIBS="-lglib-2.0"
for lib in pcre2-8 intl; do
  if [ -f "$PREFIX/lib/lib$lib.a" ]; then
    GLIB_LIBS="$GLIB_LIBS -l$lib"
  fi
done
cat > "$PREFIX/lib/pkgconfig/glib-2.0.pc" <<EOF
prefix=$PREFIX
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: GLib
Description: C Utility Library (statisch, für Android)
Version: $GLIB_VERSION
Libs: -L\${libdir} $GLIB_LIBS -llog -lm
Cflags: -I\${includedir}/glib-2.0 -I\${libdir}/glib-2.0/include
EOF
"$PKG_CONFIG" --libs --cflags glib-2.0

# --- Android-Ergänzungen ------------------------------------------------------
step "Android-Ergänzungen"
EXTRA_OBJS="$WORK/compat.o"
"$CC" -O2 -fPIC -Wall -c "$ROOT/qemu/android/compat.c" -o "$WORK/compat.o"
# QEMU verlangt librt, wenn es shm_open nicht findet. Bei Android steckt alles in
# libc, deshalb reicht eine leere Bibliothek.
echo 'static int hakoniwa_leer __attribute__((unused));' > "$WORK/leer.c"
"$CC" -c "$WORK/leer.c" -o "$WORK/leer.o"
"$AR" rcs "$PREFIX/lib/librt.a" "$WORK/leer.o"
if [ "$CPU" = aarch64 ]; then
  # setjmp/longjmp ohne die Prüfungen von Android 12+ (siehe qemu/android/README.md)
  mkdir -p "$WORK/setjmp/private"
  for f in "$ROOT"/qemu/android/setjmp-aarch64/private-*.h; do
    name="$(basename "$f")"
    cp "$f" "$WORK/setjmp/private/${name#private-}"
  done
  "$CC" -I"$WORK/setjmp" -c "$ROOT/qemu/android/setjmp-aarch64/setjmp.S" -o "$WORK/setjmp.o"
  EXTRA_OBJS="$EXTRA_OBJS $WORK/setjmp.o"
fi

# --- QEMU ---------------------------------------------------------------------
step "QEMU $QEMU_VERSION herunterladen"
download "https://download.qemu.org/qemu-$QEMU_VERSION.tar.xz" "qemu-$QEMU_VERSION.tar.xz"
if [ -n "${QEMU_SHA256:-}" ]; then
  echo "$QEMU_SHA256  $DOWNLOADS/qemu-$QEMU_VERSION.tar.xz" | sha256sum -c -
fi
tar -C "$WORK" -xf "$DOWNLOADS/qemu-$QEMU_VERSION.tar.xz"
SRC="$WORK/qemu-$QEMU_VERSION"

step "QEMU an Android anpassen"
# Wie bei Termux: signalfd nicht benutzen, kein ivshmem
python3 - "$SRC/meson.build" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path).read()
new, n = re.subn(
    r"config_host_data\.set\('CONFIG_SIGNALFD', cc\.links\(osdep_prefix \+ '''.*?'''\)\)\n",
    "", text, count=1, flags=re.S)
if n != 1:
    sys.exit("signalfd-Prüfung nicht gefunden")
new, n = re.subn(r"^have_ivshmem = .*$", "have_ivshmem = false", new, count=1, flags=re.M)
if n != 1:
    sys.exit("have_ivshmem nicht gefunden")
open(path, "w").write(new)
print("meson.build angepasst")
PY
# Eigene Geräteliste (nur die Maschine "virt")
cp "$ROOT/qemu/config/hakoniwa.mak" "$SRC/configs/devices/aarch64-softmmu/hakoniwa.mak"
# Ohne CXL fehlt in QEMU 11 sonst diese Funktion beim Linken
cat >> "$SRC/hw/cxl/cxl-host-stubs.c" <<'EOF'

/* Hakoniwa: Ersatz, falls CXL abgeschaltet ist */
GSList *cxl_fmws_get_all_sorted(void)
{
    return NULL;
}
EOF

step "QEMU konfigurieren"
mkdir -p "$SRC/build"
cd "$SRC/build"
../configure \
  --cross-prefix="${TRIPLE}-" \
  --cc="$CC" --cxx="$CXX" --host-cc=gcc \
  --cpu="$CPU" \
  --target-list=aarch64-softmmu \
  --without-default-features \
  --without-default-devices --with-devices-aarch64=hakoniwa \
  --disable-docs --disable-tools --disable-guest-agent --disable-user \
  --disable-werror --enable-fdt=internal --disable-debug-info \
  --with-coroutine=sigaltstack --disable-qom-cast-debug \
  --enable-trace-backends=nop \
  --extra-cflags="-femulated-tls -include $ROOT/qemu/android/compat.h" \
  --extra-cxxflags="-femulated-tls" \
  --extra-ldflags="-L$PREFIX/lib $EXTRA_OBJS -Wl,-z,max-page-size=16384 -llog"

step "QEMU bauen"
make -j"$JOBS" qemu-system-aarch64

step "Ergebnis"
"$STRIP" --strip-unneeded -o "$OUT/libqemu.so" qemu-system-aarch64
chmod 755 "$OUT/libqemu.so"
ls -la "$OUT/libqemu.so"
"$READELF" -h "$OUT/libqemu.so" | grep -E "Type|Machine"
"$READELF" -d "$OUT/libqemu.so" | grep -E "NEEDED|FLAGS"
"$READELF" -l "$OUT/libqemu.so" | grep -E "LOAD|INTERP|TLS" -A1 | head -24
echo "Thread-lokale Variablen über emutls:"
"$NM" -D --undefined-only "$OUT/libqemu.so" | grep -c emutls || true
"$NM" "$OUT/libqemu.so" 2>/dev/null | grep -c "__emutls_v\." || true
echo "Undefinierte setjmp-Symbole (sollten bei arm64 fehlen):"
"$NM" -D --undefined-only "$OUT/libqemu.so" | grep -i jmp || echo "  keine"
echo "QEMU $QEMU_VERSION für $ABI fertig."
