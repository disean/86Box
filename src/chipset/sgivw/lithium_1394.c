/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Lithium IEEE 1394 Controller emulation.
 *
 * Authors: Dmitry Borisov, <di.sean@protonmail.com>
 *
 *          Copyright 2024 Dmitry Borisov
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/pci.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/sgivw/lithium.h>
#include <86box/sgivw/lithium_1394_hw.h>

typedef struct li_1394_t {
    uint32_t regs[VW_LI_1394_REGS_SIZE / sizeof(uint32_t)];
    mem_mapping_t mmio_mapping;
} li_1394_t;

#ifdef ENABLE_LITHIUM_1394_LOG
int li_1394_do_log = ENABLE_LITHIUM_1394_LOG;

static void
lithium_1394_log(const char *fmt, ...)
{
    va_list ap;

    if (li_1394_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define lithium_1394_log(fmt, ...)
#endif

static void
lithium_1394_mmio_write32(uint32_t addr, uint32_t val, void* priv)
{
    li_1394_t *dev = priv;
    uint32_t write_bits_mask;

    assert((addr & 0x3) == 0);

    addr &= VW_LI_1394_IO_DECODE_MASK;

    lithium_1394_log("LI: 1394 [W32] [%lX] <-- %X\n", addr, val);

    addr /= sizeof(uint32_t);
    switch (addr) {
        case VW_LI_1394_REG_000:
        case VW_LI_1394_REG_040:
        case VW_LI_1394_REG_080:
        case VW_LI_1394_REG_0C0:
            write_bits_mask = 0x0000FFC0;
            break;
        case VW_LI_1394_REG_010:
        case VW_LI_1394_REG_050:
        case VW_LI_1394_REG_090:
        case VW_LI_1394_REG_0D0:
            write_bits_mask = 0x800001FF;
            break;
        case VW_LI_1394_REG_018:
        case VW_LI_1394_REG_058:
        case VW_LI_1394_REG_098:
        case VW_LI_1394_REG_0D8:
            write_bits_mask = 0x0000CFFF;
            break;
        case VW_LI_1394_REG_020:
        case VW_LI_1394_REG_028:
        case VW_LI_1394_REG_060:
        case VW_LI_1394_REG_068:
        case VW_LI_1394_REG_0A0:
        case VW_LI_1394_REG_0A8:
        case VW_LI_1394_REG_0E0:
        case VW_LI_1394_REG_0E8:
            write_bits_mask = 0xFFFFFFFF;
            break;
        case VW_LI_1394_REG_030:
        case VW_LI_1394_REG_070:
        case VW_LI_1394_REG_0B0:
        case VW_LI_1394_REG_0F0:
            write_bits_mask = 0xBFFFFFFF;
            break;
        case VW_LI_1394_REG_108:
            write_bits_mask = 0xFFFFF3B0;
            break;
        case VW_LI_1394_REG_110:
        case VW_LI_1394_REG_150:
        case VW_LI_1394_REG_190:
        case VW_LI_1394_REG_1D0:
            // TODO: Not correct
            write_bits_mask = 0xFFFFFFFF;
            break;
        case VW_LI_1394_REG_128:
        case VW_LI_1394_REG_130:
        case VW_LI_1394_REG_148:
        case VW_LI_1394_REG_168:
        case VW_LI_1394_REG_170:
        case VW_LI_1394_REG_188:
        case VW_LI_1394_REG_1A8:
        case VW_LI_1394_REG_1B0:
        case VW_LI_1394_REG_1C8:
        case VW_LI_1394_REG_1E8:
        case VW_LI_1394_REG_1F0:
            write_bits_mask = 0xFFFFFFFF;
            break;
        case VW_LI_1394_REG_200:
        case VW_LI_1394_REG_280:
        case VW_LI_1394_REG_300:
        case VW_LI_1394_REG_380:
        case VW_LI_1394_REG_400:
        case VW_LI_1394_REG_480:
        case VW_LI_1394_REG_500:
        case VW_LI_1394_REG_580:
        case VW_LI_1394_REG_600:
        case VW_LI_1394_REG_680:
        case VW_LI_1394_REG_700:
        case VW_LI_1394_REG_780:
            write_bits_mask = 0x00000001;
            break;

        default:
            write_bits_mask = 0;
            break;
    }

    val &= write_bits_mask;
    dev->regs[addr] = val | (dev->regs[addr] & ~write_bits_mask);
}

static uint32_t
lithium_1394_mmio_read32(uint32_t addr, void* priv)
{
    li_1394_t *dev = priv;
    uint32_t ret;

    assert((addr & 0x3) == 0);

    addr &= VW_LI_1394_IO_DECODE_MASK;
    addr /= sizeof(uint32_t);

    ret = dev->regs[addr];

    lithium_1394_log("LI: 1394 [R32] [%lX] --> %X\n", addr * sizeof(uint32_t), ret);
    return ret;
}

static void
lithium_1394_reset_hard(li_1394_t *dev)
{
    dev->regs[VW_LI_1394_REG_000] = 0x1000FFF2;
    dev->regs[VW_LI_1394_REG_018] = 0x04270000;
    dev->regs[VW_LI_1394_REG_030] = 0x80000000;
    dev->regs[VW_LI_1394_REG_038] = 0x80000000;

    dev->regs[VW_LI_1394_REG_040] = 0x1000FFF2;
    dev->regs[VW_LI_1394_REG_058] = 0x04270000;
    dev->regs[VW_LI_1394_REG_070] = 0x80000000;
    dev->regs[VW_LI_1394_REG_078] = 0x80000000;

    dev->regs[VW_LI_1394_REG_080] = 0x1000FFF2;
    dev->regs[VW_LI_1394_REG_098] = 0x04270000;
    dev->regs[VW_LI_1394_REG_0B0] = 0x80000000;
    dev->regs[VW_LI_1394_REG_0B8] = 0x80000000;

    dev->regs[VW_LI_1394_REG_0C0] = 0x1000FFF2;
    dev->regs[VW_LI_1394_REG_0D8] = 0x04270000;
    dev->regs[VW_LI_1394_REG_0F0] = 0x80000000;
    dev->regs[VW_LI_1394_REG_0F8] = 0x80000000;

    dev->regs[VW_LI_1394_REG_200] = 0x00000001;
    dev->regs[VW_LI_1394_REG_280] = 0x00000001;
    dev->regs[VW_LI_1394_REG_300] = 0x00000001;
    dev->regs[VW_LI_1394_REG_380] = 0x00000001;
    dev->regs[VW_LI_1394_REG_400] = 0x00000001;
    dev->regs[VW_LI_1394_REG_480] = 0x00000001;
    dev->regs[VW_LI_1394_REG_500] = 0x00000001;
    dev->regs[VW_LI_1394_REG_580] = 0x00000001;
    dev->regs[VW_LI_1394_REG_600] = 0x00000001;
    dev->regs[VW_LI_1394_REG_680] = 0x00000001;
    dev->regs[VW_LI_1394_REG_700] = 0x00000001;
    dev->regs[VW_LI_1394_REG_780] = 0x00000001;
}

static void
lithium_1394_close(void *priv)
{
    li_1394_t *dev = priv;

    free(dev);
}

static void *
lithium_1394_init(const device_t *devinfo)
{
    li_1394_t *dev = calloc(1, sizeof(li_1394_t));

    mem_mapping_add(&dev->mmio_mapping,
                    VW_LI_1394_IO_BASE,
                    VW_LI_1394_IO_SIZE,
                    NULL,
                    NULL,
                    lithium_1394_mmio_read32,
                    NULL,
                    NULL,
                    lithium_1394_mmio_write32,
                    NULL,
                    MEM_MAPPING_EXTERNAL,
                    dev);

    lithium_1394_reset_hard(dev);
}

const device_t lithium_1394_device = {
    .name          = "Lithium IEEE 1394 Controller",
    .internal_name = "li_1394",
    .flags         = 0,
    .local         = 0,
    .init          = lithium_1394_init,
    .close         = lithium_1394_close,
    .reset         = NULL,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
