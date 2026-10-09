#include "dtb.h"
#include "lib.h"

// Aufbau eines Device Trees: ein Kopf mit Offsets, danach eine Folge von
// 4-Byte-Markierungen (Token). Alle Zahlen sind "big endian" gespeichert.
#define FDT_MAGIC      0xd00dfeedu
#define FDT_BEGIN_NODE 1u
#define FDT_END_NODE   2u
#define FDT_PROP       3u
#define FDT_NOP        4u
#define FDT_END        9u

// Byteweise lesen: Ohne MMU sind nur korrekt ausgerichtete Zugriffe erlaubt.
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t read_cells(const uint8_t *p, uint32_t cells)
{
    uint64_t value = 0;
    for (uint32_t i = 0; i < cells; i++) {
        value = (value << 32) | be32(p + 4 * i);
    }
    return value;
}

static uint32_t align4(uint32_t n)
{
    return (n + 3u) & ~3u;
}

void dtb_read(uintptr_t address, struct dtb_info *info)
{
    memset(info, 0, sizeof(*info));
    const uint8_t *base = (const uint8_t *)address;

    if (be32(base) != FDT_MAGIC) {
        return;
    }

    uint32_t total_size = be32(base + 4);
    uint32_t off_struct = be32(base + 8);
    uint32_t off_strings = be32(base + 12);
    if (total_size < 40 || total_size > 0x100000 || off_struct >= total_size ||
        off_strings >= total_size) {
        return;
    }

    const uint8_t *p = base + off_struct;
    const uint8_t *end = base + total_size;
    uint32_t address_cells = 2;
    uint32_t size_cells = 1;
    int depth = 0;
    int memory_depth = 0;
    int cpus_depth = 0;

    while (p + 4 <= end) {
        uint32_t token = be32(p);
        p += 4;

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            uint32_t length = (uint32_t)strlen(name);
            p += align4(length + 1);
            depth++;
            info->node_count++;
            if (depth == 2 && strncmp(name, "memory", 6) == 0) {
                memory_depth = depth;
            }
            if (depth == 2 && strcmp(name, "cpus") == 0) {
                cpus_depth = depth;
            }
            if (cpus_depth && depth == cpus_depth + 1 && strncmp(name, "cpu@", 4) == 0) {
                info->cpu_count++;
            }
        } else if (token == FDT_END_NODE) {
            if (depth == memory_depth) {
                memory_depth = 0;
            }
            if (depth == cpus_depth) {
                cpus_depth = 0;
            }
            depth--;
        } else if (token == FDT_PROP) {
            if (p + 8 > end) {
                return;
            }
            uint32_t length = be32(p);
            uint32_t name_offset = be32(p + 4);
            p += 8;
            const uint8_t *value = p;
            p += align4(length);
            if (p > end || off_strings + name_offset >= total_size) {
                return;
            }
            const char *name = (const char *)(base + off_strings + name_offset);

            if (depth == 1) {
                if (strcmp(name, "#address-cells") == 0 && length == 4) {
                    address_cells = be32(value);
                } else if (strcmp(name, "#size-cells") == 0 && length == 4) {
                    size_cells = be32(value);
                } else if (strcmp(name, "compatible") == 0) {
                    info->compatible = (const char *)value;
                }
            }

            if (memory_depth && depth == memory_depth && strcmp(name, "reg") == 0 &&
                address_cells <= 2 && size_cells <= 2) {
                uint32_t entry = 4 * (address_cells + size_cells);
                for (uint32_t off = 0; entry && off + entry <= length; off += entry) {
                    uint64_t start = read_cells(value + off, address_cells);
                    uint64_t size = read_cells(value + off + 4 * address_cells, size_cells);
                    if (info->ram_size == 0) {
                        info->ram_base = start;
                    }
                    info->ram_size += size;
                }
            }
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            break;
        } else {
            return;  // unbekanntes Token: Device Tree beschädigt
        }
    }

    info->total_size = total_size;
    info->found = 1;
}
