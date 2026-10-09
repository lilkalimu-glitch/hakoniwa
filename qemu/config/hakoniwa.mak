# Geräte für Hakoniwa (QEMU wird mit --without-default-devices gebaut)

# Die Maschine "virt", auf der das Mini-Betriebssystem läuft
CONFIG_ARM_VIRT=y

# Nötig, weil "virt" ACPI_CXL auswählt: Ohne CXL fehlt beim Linken
# cxl_fmws_get_all_sorted() (QEMU 11). CXL braucht wiederum PXB.
CONFIG_PXB=y
CONFIG_CXL=y
