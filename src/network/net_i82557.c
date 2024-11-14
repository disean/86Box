/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Intel 8255x Ethernet Controller family device emulation.
 *
 * Authors: Dmitry Borisov, <di.sean@protonmail.com>
 *
 *          Copyright 2024 Dmitry Borisov
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <wchar.h>

#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/pci.h>
#include <86box/random.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/dma.h>
#include <86box/device.h>
#include <86box/thread.h>
#include <86box/network.h>
#include <86box/net_eeprom_nmc93cxx.h>
#include <86box/bswap.h>
#include <86box/plat_fallthrough.h>
#include <86box/plat_unused.h>

#include <86box/net_i8255x.h>

#ifdef ENABLE_I8255X_LOG
int i8255x_do_log = ENABLE_I8255X_LOG;

static void
i8255x_log(const char *fmt, ...)
{
    va_list ap;

    if (i8255x_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define i8255x_log(fmt, ...)
#endif

static void
i8255x_mmio_write8(uint32_t addr, uint8_t val, void* priv)
{
    i8255x_log("I8255x [W08] [%lX] <-- %X\n", addr, val);
}

static void
i8255x_mmio_write16(uint32_t addr, uint16_t val, void* priv)
{
    i8255x_log("I8255x [W16] [%lX] <-- %X\n", addr, val);
}

static void
i8255x_mmio_write32(uint32_t addr, uint32_t val, void* priv)
{
    i8255x_log("I8255x [W32] [%lX] <-- %X\n", addr, val);
}

static uint8_t
i8255x_mmio_read8(uint32_t addr, void* priv)
{
    uint8_t ret;

    ret = 0;

    i8255x_log("I8255x [R08] [%lX] --> %X\n", addr, ret);
    return ret;
}

static uint16_t
i8255x_mmio_read16(uint32_t addr, void* priv)
{
    uint16_t ret;

    ret = 0;

    i8255x_log("I8255x [R16] [%lX] --> %X\n", addr, ret);
    return ret;
}

static uint32_t
i8255x_mmio_read32(uint32_t addr, void* priv)
{
    uint32_t ret;

    ret = 0;

    i8255x_log("I8255x [R32] [%lX] --> %X\n", addr, ret);
    return ret;
}

static void
i8255x_ioport_write32(uint16_t addr, uint32_t val, void *priv)
{
    i8255x_log("I8255x [WI32] [%lX] <-- %X\n", addr, val);
}

static void
i8255x_ioport_write16(uint16_t addr, uint16_t val, void *priv)
{
    nic_t *dev = priv;

    i8255x_log("I8255x [WI16] [%lX] <-- %X\n", addr, val);
}

static void
i8255x_ioport_write8(uint16_t addr, uint8_t val, void *priv)
{
    i8255x_log("I8255x [WI08] [%lX] <-- %X\n", addr, val);
}

static uint32_t
i8255x_ioport_read32(uint16_t addr, void* priv)
{
    uint32_t ret;

    ret = 0;

    i8255x_log("I8255x [RI32] [%lX] --> %X\n", addr, ret);
    return ret;
}

static uint16_t
i8255x_ioport_read16(uint16_t addr, void* priv)
{
    nic_t *dev = priv;
    uint16_t ret;

    ret = 0;

    i8255x_log("I8255x [RI16] [%lX] --> %X\n", addr, ret);
    return ret;
}

static uint8_t
i8255x_ioport_read8(uint16_t addr, void* priv)
{
    uint8_t ret;

    ret = 0;

    i8255x_log("I8255x [RI08] [%lX] --> %X\n", addr, ret);
    return ret;
}

static void
i8255x_pci_remap_mmio_mapping(nic_t *dev)
{
    uint32_t mmio_base;

    mmio_base = dev->pci_config[I8255X_PCI_CFG_BAR0_BYTE0];
    mmio_base |= dev->pci_config[I8255X_PCI_CFG_BAR0_BYTE1] << 8;
    mmio_base |= dev->pci_config[I8255X_PCI_CFG_BAR0_BYTE2] << 16;
    mmio_base |= dev->pci_config[I8255X_PCI_CFG_BAR0_BYTE3] << 24;
    mmio_base &= ~0x0F;

    i8255x_log("I8255x MMIO I/O Base %08lX\n", mmio_base);

    mem_mapping_set_addr(&dev->mmio_bar_mapping,
                         mmio_base,
                         I8255X_PCI_MMIO_BAR_SIZE);
}

static void
i8255x_pci_remap_flash_mapping(nic_t *dev)
{
    uint32_t flash_mmio_base;

    flash_mmio_base = dev->pci_config[I8255X_PCI_CFG_BAR2_BYTE0];
    flash_mmio_base |= dev->pci_config[I8255X_PCI_CFG_BAR2_BYTE1] << 8;
    flash_mmio_base |= dev->pci_config[I8255X_PCI_CFG_BAR2_BYTE2] << 16;
    flash_mmio_base |= dev->pci_config[I8255X_PCI_CFG_BAR2_BYTE3] << 24;
    flash_mmio_base &= ~0x0F;

    i8255x_log("I8255x Flash I/O Base %08lX\n", flash_mmio_base);

    mem_mapping_set_addr(&dev->flash_bar_mapping,
                         flash_mmio_base,
                         I8255X_PCI_FLASH_BAR_SIZE);
}

static void
i8255x_pci_remap_ioport_mapping(nic_t *dev, int do_enable)
{
    uint32_t ioport_base;

    ioport_base = dev->pci_config[I8255X_PCI_CFG_BAR1_BYTE0];
    ioport_base |= dev->pci_config[I8255X_PCI_CFG_BAR1_BYTE1] << 8;
    ioport_base |= dev->pci_config[I8255X_PCI_CFG_BAR1_BYTE2] << 16;
    ioport_base |= dev->pci_config[I8255X_PCI_CFG_BAR1_BYTE3] << 24;
    ioport_base &= ~0x03;

    if (!(dev->pci_config[PCI_REG_COMMAND_L] & PCI_COMMAND_L_IO))
        do_enable = 0;

    io_handler(do_enable,
               ioport_base,
               I8255X_PCI_IO_BAR_SIZE,
               i8255x_ioport_read8,
               i8255x_ioport_read16,
               i8255x_ioport_read32,
               i8255x_ioport_write8,
               i8255x_ioport_write16,
               i8255x_ioport_write32,
               dev);
}

static void
i8255x_pci_control(nic_t *dev)
{
    if (dev->pci_config[PCI_REG_COMMAND_L] & PCI_COMMAND_L_MEM)
    {
        mem_mapping_enable(&dev->mmio_bar_mapping);
        mem_mapping_enable(&dev->flash_bar_mapping);
    }
    else
    {
        mem_mapping_disable(&dev->mmio_bar_mapping);
        mem_mapping_disable(&dev->flash_bar_mapping);
    }

    if (dev->pci_config[PCI_REG_COMMAND_L] & PCI_COMMAND_L_IO)
        i8255x_pci_remap_ioport_mapping(dev, 1);
    else
        i8255x_pci_remap_ioport_mapping(dev, 0);
}

static void
i8255x_pci_write(UNUSED(int func), int addr, uint8_t val, void *priv)
{
    nic_t *dev = priv;
    uint8_t write_bits_mask;

    switch (addr) {
        case PCI_REG_COMMAND_L:
            write_bits_mask = 0x47;
            break;
        case PCI_REG_COMMAND_H:
            write_bits_mask = 0x01;
            break;

        case PCI_REG_LATENCY_TIMER:
            write_bits_mask = 0xFF;
            break;

        case I8255X_PCI_CFG_BAR0_BYTE1:
            write_bits_mask = 0xF0;
            break;
        case I8255X_PCI_CFG_BAR0_BYTE2:
            write_bits_mask = 0xFF;
            break;
        case I8255X_PCI_CFG_BAR0_BYTE3:
            write_bits_mask = 0xFF;
            break;

        case I8255X_PCI_CFG_BAR1_BYTE0:
            write_bits_mask = 0xE0;
            break;
        case I8255X_PCI_CFG_BAR1_BYTE1:
        case I8255X_PCI_CFG_BAR1_BYTE2:
        case I8255X_PCI_CFG_BAR1_BYTE3:
            write_bits_mask = 0xFF;
            break;

        case I8255X_PCI_CFG_BAR2_BYTE2:
            write_bits_mask = 0xF0;
            break;
        case I8255X_PCI_CFG_BAR2_BYTE3:
            write_bits_mask = 0xFF;
            break;

        case I8255X_PCI_CFG_ROM_BASE_BYTE0:
            write_bits_mask = 0x01;
            break;
        case I8255X_PCI_CFG_ROM_BASE_BYTE2:
            write_bits_mask = 0xF0;
            break;
        case I8255X_PCI_CFG_ROM_BASE_BYTE3:
            write_bits_mask = 0xFF;
            break;

        case I8255X_PCI_CFG_INT_LINE:
            write_bits_mask = 0xFF;
            break;

        default:
            write_bits_mask = 0;
            break;
    }

    i8255x_log("I8255x PCI [%2lX] <-- %X\n", addr, val);

    switch (addr) {
        case I8255X_PCI_CFG_BAR1_BYTE0:
        case I8255X_PCI_CFG_BAR1_BYTE1:
        case I8255X_PCI_CFG_BAR1_BYTE2:
        case I8255X_PCI_CFG_BAR1_BYTE3:
            i8255x_pci_remap_ioport_mapping(dev, 0);
            break;

        default:
            break;
    }

    val &= write_bits_mask;
    val |= dev->pci_config[addr] & ~write_bits_mask;
    dev->pci_config[addr] = val;

    switch (addr) {
        case PCI_REG_COMMAND_L:
            i8255x_pci_control(dev);
            break;

        case I8255X_PCI_CFG_BAR0_BYTE1:
        case I8255X_PCI_CFG_BAR0_BYTE2:
        case I8255X_PCI_CFG_BAR0_BYTE3:
            i8255x_pci_remap_mmio_mapping(dev);
            break;

        case I8255X_PCI_CFG_BAR1_BYTE0:
        case I8255X_PCI_CFG_BAR1_BYTE1:
        case I8255X_PCI_CFG_BAR1_BYTE2:
        case I8255X_PCI_CFG_BAR1_BYTE3:
            i8255x_pci_remap_ioport_mapping(dev, 1);
            break;

        case I8255X_PCI_CFG_BAR2_BYTE2:
        case I8255X_PCI_CFG_BAR2_BYTE3:
            i8255x_pci_remap_flash_mapping(dev);
            break;

        default:
            break;
    }
}

static uint8_t
i8255x_pci_read(UNUSED(int func), int addr, void *priv)
{
    nic_t *dev = priv;
    uint8_t ret;

    ret = dev->pci_config[addr];

    i8255x_log("I8255x PCI [%2lX] --> %X\n", addr, ret);

    return ret;
}

static void
i8255x_reset(void *priv)
{
    nic_t *dev = priv;

    dev->pci_config[PCI_REG_VENDOR_ID_L] = 0x86;
    dev->pci_config[PCI_REG_VENDOR_ID_H] = 0x80;

    dev->pci_config[PCI_REG_DEVICE_ID_L] = 0x29;
    dev->pci_config[PCI_REG_DEVICE_ID_H] = 0x12;

    dev->pci_config[PCI_REG_STATUS_L] = 0x80;
    dev->pci_config[PCI_REG_STATUS_H] = 0x02;

    dev->pci_config[PCI_REG_REVISION] = 0x02;
    dev->pci_config[PCI_REG_CLASS] = 0x02;

    dev->pci_config[I8255X_PCI_CFG_BAR0_BYTE0] = 0x08;

    dev->pci_config[I8255X_PCI_CFG_BAR1_BYTE0] = 0x01;

    dev->pci_config[I8255X_PCI_CFG_INT_PIN] = PCI_INTA;
    dev->pci_config[I8255X_PCI_CFG_MIN_GRANT] = 8;
    dev->pci_config[I8255X_PCI_CFG_MAX_LATENCY] = 56;
}

static void *
i8255x_init(UNUSED(const device_t *info))
{
    nic_t *dev = calloc(1, sizeof(nic_t));

    mem_mapping_add(&dev->mmio_bar_mapping,
                    0,
                    0,
                    i8255x_mmio_read8,
                    i8255x_mmio_read16,
                    i8255x_mmio_read32,
                    i8255x_mmio_write8,
                    i8255x_mmio_write16,
                    i8255x_mmio_write32,
                    NULL,
                    MEM_MAPPING_EXTERNAL,
                    dev);

    mem_mapping_add(&dev->flash_bar_mapping,
                    0,
                    0,
                    i8255x_mmio_read8,
                    i8255x_mmio_read16,
                    i8255x_mmio_read32,
                    i8255x_mmio_write8,
                    i8255x_mmio_write16,
                    i8255x_mmio_write32,
                    NULL,
                    MEM_MAPPING_EXTERNAL,
                    dev);

    pci_add_card(PCI_CARD_NETWORK,
                 i8255x_pci_read,
                 i8255x_pci_write,
                 dev,
                 &dev->pci_slot);

    i8255x_reset(dev);
}

static void
i8255x_close(void *priv)
{
    nic_t *dev = priv;

    free(dev);
}

static const device_config_t i8255x_config[] = {
    {
        .name = "mac",
        .description = "MAC Address",
        .type = CONFIG_MAC,
        .default_string = "",
        .default_int = -1
    },
    { .name = "", .description = "", .type = CONFIG_END }
};

const device_t intel_82557_device = {
    .name          = "Intel 82557 Fast Ethernet PCI Bus Controller",
    .internal_name = "intel_82557",
    .flags         = DEVICE_PCI,
    .local         = 0,
    .init          = i8255x_init,
    .close         = i8255x_close,
    .reset         = i8255x_reset,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = i8255x_config
};
