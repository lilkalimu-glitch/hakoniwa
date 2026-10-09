/*
 * Ergänzungen für Android (bionic), die QEMU erwartet.
 */
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/types.h>

/*
 * QEMU benutzt shm_open() nur, um sofort danach shm_unlink() aufzurufen und
 * mit dem Dateideskriptor weiterzuarbeiten. Dafür ist memfd gleichwertig.
 */
int shm_open(const char *name, int oflag, mode_t mode)
{
    (void)mode;
    if (!(oflag & O_CREAT)) {
        errno = ENOSYS;
        return -1;
    }
    while (*name == '/') {
        name++;
    }
    return memfd_create(name, MFD_CLOEXEC);
}

int shm_unlink(const char *name)
{
    (void)name;
    return 0;
}

#ifndef M_BIONIC_SET_HEAP_TAGGING_LEVEL
#define M_BIONIC_SET_HEAP_TAGGING_LEVEL (-204)
#endif

/*
 * Android 11+ markiert auf arm64 Heap-Zeiger im obersten Byte ("tagged pointers").
 * Damit kommt QEMU nicht zurecht, deshalb wird das vor main() abgeschaltet
 * (wie im QEMU-Paket von Termux).
 */
__attribute__((constructor)) static void hakoniwa_disable_heap_tagging(void)
{
#if defined(__aarch64__)
    mallopt(M_BIONIC_SET_HEAP_TAGGING_LEVEL, 0 /* M_HEAP_TAGGING_LEVEL_NONE */);
#endif
}
