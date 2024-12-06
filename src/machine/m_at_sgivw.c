/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of SGI 320/540.
 *
 * Authors: Dmitry Borisov, <di.sean@protonmail.com>
 *
 *          Copyright 2024 Dmitry Borisov
 *
 * TODO: pci_register_bus_slot() The INT numbers are not accurate
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/mem.h>
#include <86box/io.h>
#include <86box/rom.h>
#include <86box/pci.h>
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/keyboard.h>
#include <86box/flash.h>
#include <86box/sio.h>
#include <86box/timer.h>
#include <86box/thread.h>
#include <86box/network.h>
#include "cpu.h"
#include <86box/machine.h>

#define VW_GPI_DEFAULT                0x0000607E

#define VW_GPI_JP_PASSWORD_ENABLED    0x00000000
#define VW_GPI_JP_PASSWORD_DISABLED   0x00008000

#define VW_GPI_CPU_BUS_66MHZ          0x00000000
#define VW_GPI_CPU_BUS_100MHZ         0x00010000

#define VW_GPI_MODEL_320              0x00000000
#define VW_GPI_MODEL_540              0x00040000

static const device_config_t sgi320_config[] = {
    // clang-format off
    {
        .name = "prom_upgrade",
        .description = "PROM Version",
        .type = CONFIG_BIOS,
        .default_string = "1005",
        .default_int = 0,
        .file_filter = "",
        .spinner = { 0 },
        .bios = {
            { .name = "1.0002", .internal_name = "0002", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0002.bin", "" } },
            { .name = "1.0004", .internal_name = "0004", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0004.bin", "" } },
            { .name = "1.0005", .internal_name = "0005", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0005.bin", "" } },
            { .name = "1.0006", .internal_name = "0006", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0006.bin", "" } },
            { .name = "1.1005", .internal_name = "1005", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom1005.bin", "" } },
            { .files_no = 0 }
        },
    },
    {
        .name = "board_rev",
        .description = "Motherboard Revision",
        .type = CONFIG_SELECTION,
        .default_string = "",
        .default_int = 0x11,
        .file_filter = "",
        .spinner = { 0 },
        .selection = {
            { .description = "006A", .value = 0x0C },
            { .description = "006D", .value = 0x0F },
            { .description = "006F", .value = 0x11 },
            { .description = "006H", .value = 0x13 },
            { .description = "006J", .value = 0x15 },
            { .description = "006K", .value = 0x16 },
            { .description = ""                    }
        },
    },
    {
        .name = "jp1",
        .description = "Password Jumper",
        .type = CONFIG_INT,
        .default_string = "",
        .default_int = 0,
        .file_filter = "",
        .spinner = { 0 },
        .selection = {
            { .description = "Disabled", .value = 0 },
            { .description = "Enabled",  .value = 1 },
            { .description = ""                     }
        },
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

static const device_config_t sgi540_config[] = {
    // clang-format off
    {
        .name = "prom_upgrade",
        .description = "PROM Version",
        .type = CONFIG_BIOS,
        .default_string = "1005",
        .default_int = 0,
        .file_filter = "",
        .spinner = { 0 },
        .bios = {
            /* The VW 540 only started shipping with 1.0004 and higher PROM versions */
            { .name = "1.0004", .internal_name = "0004", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0004.bin", "" } },
            { .name = "1.0005", .internal_name = "0005", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0005.bin", "" } },
            { .name = "1.0006", .internal_name = "0006", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom0006.bin", "" } },
            { .name = "1.1005", .internal_name = "1005", .bios_type = BIOS_NORMAL,
              .files_no = 1, .local = 0, .size = 513 * 1024, .files = { "roms/machines/sgivw/prom1005.bin", "" } },
            { .files_no = 0 }
        },
    },
    {
        .name = "jp1",
        .description = "Password Jumper",
        .type = CONFIG_INT,
        .default_string = "",
        .default_int = 0,
        .file_filter = "",
        .spinner = { 0 },
        .selection = {
            { .description = "Disabled", .value = 0 },
            { .description = "Enabled",  .value = 1 },
            { .description = ""                     }
        },
    },
    { .name = "", .description = "", .type = CONFIG_END }
    // clang-format on
};

const device_t sgivw320_device = {
    .name          = "Visual Workstation",
    .internal_name = "sgivw_config",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = sgi320_config
};

const device_t sgivw540_device = {
    .name          = "Visual Workstation",
    .internal_name = "sgivw_config",
    .flags         = 0,
    .local         = 0,
    .init          = NULL,
    .close         = NULL,
    .reset         = NULL,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = sgi540_config
};

static void
machine_sgivw_gpio_init(bool is_320)
{
    uint32_t piix_gpio, sio_board_rev_gpio;

    piix_gpio = VW_GPI_DEFAULT;

    if (is_320)
        piix_gpio |= VW_GPI_MODEL_320;
    else
        piix_gpio |= VW_GPI_MODEL_540;

    if (device_get_config_int("jp1"))
        piix_gpio |= VW_GPI_JP_PASSWORD_ENABLED;
    else
        piix_gpio |= VW_GPI_JP_PASSWORD_DISABLED;

    if (cpu_busspeed >= 100000000)
        piix_gpio |= VW_GPI_CPU_BUS_100MHZ;
    else
        piix_gpio |= VW_GPI_CPU_BUS_66MHZ;

    machine_set_gpio_acpi_default(piix_gpio);

    if (is_320)
        sio_board_rev_gpio = device_get_config_int("board_rev");
    else
        sio_board_rev_gpio = 2; // TODO

    machine_set_gpio_default(sio_board_rev_gpio);
}

static int
machine_at_sgivw_common_init(const machine_t *model, bool is_320)
{
    const char* prom_path;
    int ret;

    device_context(model->device);
    prom_path = device_get_bios_file(model->device, device_get_config_bios("prom_upgrade"), 0);

    ret = bios_load_linear(prom_path, 0x00080000, 512 * 1024, 0x200);
    if (bios_only || !ret)
        return ret;

    machine_sgivw_gpio_init(is_320);

    machine_at_common_init_ex(model, 2);

    /* Access type 1 only */
    pci_init(PCI_CONFIG_TYPE_1);

    /* Lithium B bus #0 */
    pci_register_bus_slot(0, 0x04, PCI_CARD_SOUTHBRIDGE, 0, 0, 0, 4); // PIIX4E (FW82371EB)

    /* Lithium A bus #1 */
    pci_register_bus_slot(1, 0x03, PCI_CARD_NETWORK,     4, 0, 0, 0); // On-Board Intel 82557

    device_add(&piix4e_device);
    device_add(&pc87307_device);
    device_add(&cobalt_chipset_device);
    device_add(&m29f400t_flash_device);
    device_add(&intel_82557_device);

    return ret;
}

int
machine_at_sgivw320_init(const machine_t *model)
{
    int ret;

    ret = machine_at_sgivw_common_init(model, true);
    if (bios_only || !ret)
        return ret;

    /* Lithium B bus #0 */
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORMAL, 1, 2, 3, 4); // 32-bit 3.3V option slot #1

    /* Lithium A bus #1 */
    pci_register_bus_slot(1, 0x00, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 3.3V option slot #2
    pci_register_bus_slot(1, 0x01, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 3.3V option slot #3

    return ret;
}

int
machine_at_sgivw540_init(const machine_t *model)
{
    int ret;

    ret = machine_at_sgivw_common_init(model, false);
    if (bios_only || !ret)
        return ret;

    /* Lithium B bus #0 */
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 5V option slot #1
    pci_register_bus_slot(0, 0x01, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 5V option slot #2
    pci_register_bus_slot(0, 0x02, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 5V option slot #3
    pci_register_bus_slot(0, 0x03, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 5V option slot #4

    /* Lithium A bus #1 */
    pci_register_bus_slot(1, 0x00, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 3.3V option slot #5
    pci_register_bus_slot(1, 0x01, PCI_CARD_NORMAL, 1, 2, 3, 4); // 64-bit 3.3V option slot #6
    pci_register_bus_slot(1, 0x02, PCI_CARD_SCSI,   1, 2, 3, 4); // On-Board Qlogic 1080 SCSI

    return ret;
}
