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
 */
#include <stdarg.h>
#include <stdint.h>
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
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/keyboard.h>
#include <86box/flash.h>
#include <86box/sio.h>
#include <86box/hwm.h>
#include <86box/spd.h>
#include <86box/video.h>
#include <86box/clock.h>
#include <86box/timer.h>
#include <86box/serial.h>
#include <86box/thread.h>
#include <86box/network.h>
#include "cpu.h"
#include <86box/machine.h>

int
machine_at_sgivw320_init(const machine_t *model)
{
    int ret;

    ret = bios_load_linear("roms/machines/sgivw/prom1005.bin",
                           0x00080000, 512 * 1024, 0x200);
    if (bios_only || !ret)
        return ret;

    machine_at_common_init_ex(model, 2);

    // TODO: The INT numbers are not accurate

    /* Access type 1 only */
    pci_init(PCI_CONFIG_TYPE_1);

    /* Lithium B bus */
    pci_register_bus_slot(0, 0x04, PCI_CARD_SOUTHBRIDGE, 0, 0, 0, 4); // PIIX4E (FW82371EB)
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORMAL,      1, 2, 3, 4); // 32-bit 3.3V option slot #1 // TODO: Verify 0x00

    /* Lithium A bus */
    pci_register_bus_slot(1, 0x03, PCI_CARD_NETWORK,     4, 0, 0, 0); // On-Board Intel 82557
    pci_register_bus_slot(1, 0x00, PCI_CARD_NORMAL,      1, 2, 3, 4); // 64-bit 3.3V option slot #1
    pci_register_bus_slot(1, 0x01, PCI_CARD_NORMAL,      1, 2, 3, 4); // 64-bit 3.3V option slot #2 // TODO: Verify 0x01

    device_add(&piix4e_device);
    device_add(&pc87307_device);
    device_add(&cobalt_chipset_device);
    device_add(&m29f400t_flash_device);
    device_add(&intel_82557_device);

    return ret;
}
