/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Intel 82557 device emulation.
 *
 * Authors: Dmitry Borisov, <di.sean@protonmail.com>
 *
 *          Copyright 2024 Dmitry Borisov
 */
#pragma once

#define I8255X_PCI_CFG_BAR0_BYTE0               0x10
#define I8255X_PCI_CFG_BAR0_BYTE1               0x11
#define I8255X_PCI_CFG_BAR0_BYTE2               0x12
#define I8255X_PCI_CFG_BAR0_BYTE3               0x13

#define I8255X_PCI_CFG_BAR1_BYTE0               0x14
#define I8255X_PCI_CFG_BAR1_BYTE1               0x15
#define I8255X_PCI_CFG_BAR1_BYTE2               0x16
#define I8255X_PCI_CFG_BAR1_BYTE3               0x17

#define I8255X_PCI_CFG_BAR2_BYTE0               0x18
#define I8255X_PCI_CFG_BAR2_BYTE1               0x19
#define I8255X_PCI_CFG_BAR2_BYTE2               0x1A
#define I8255X_PCI_CFG_BAR2_BYTE3               0x1B

#define I8255X_PCI_CFG_BAR3_BYTE0               0x1C
#define I8255X_PCI_CFG_BAR3_BYTE1               0x1D
#define I8255X_PCI_CFG_BAR3_BYTE2               0x1E
#define I8255X_PCI_CFG_BAR3_BYTE3               0x1F

#define I8255X_PCI_CFG_BAR4_BYTE0               0x20
#define I8255X_PCI_CFG_BAR4_BYTE1               0x21
#define I8255X_PCI_CFG_BAR4_BYTE2               0x22
#define I8255X_PCI_CFG_BAR4_BYTE3               0x23

#define I8255X_PCI_CFG_BAR5_BYTE0               0x24
#define I8255X_PCI_CFG_BAR5_BYTE1               0x25
#define I8255X_PCI_CFG_BAR5_BYTE2               0x26
#define I8255X_PCI_CFG_BAR5_BYTE3               0x27

#define I8255X_PCI_CFG_CIS_PTR_BYTE0            0x28
#define I8255X_PCI_CFG_CIS_PTR_BYTE1            0x29
#define I8255X_PCI_CFG_CIS_PTR_BYTE2            0x2A
#define I8255X_PCI_CFG_CIS_PTR_BYTE3            0x2B

#define I8255X_PCI_CFG_SUB_VEN_ID_LOW           0x2C
#define I8255X_PCI_CFG_SUB_VEN_ID_HIGH          0x2D

#define I8255X_PCI_CFG_SUBSYSTEM_ID_LOW         0x2E
#define I8255X_PCI_CFG_SUBSYSTEM_ID_HIGH        0x2F

#define I8255X_PCI_CFG_ROM_BASE_BYTE0           0x30
#define I8255X_PCI_CFG_ROM_BASE_BYTE1           0x31
#define I8255X_PCI_CFG_ROM_BASE_BYTE2           0x32
#define I8255X_PCI_CFG_ROM_BASE_BYTE3           0x33

#define I8255X_PCI_CFG_CAPS_PTR                 0x34
#define I8255X_PCI_CFG_INT_LINE                 0x3C
#define I8255X_PCI_CFG_INT_PIN                  0x3D
#define I8255X_PCI_CFG_MIN_GRANT                0x3E
#define I8255X_PCI_CFG_MAX_LATENCY              0x3F

#define I8255X_PCI_MMIO_BAR_SIZE   0x1000
#define I8255X_PCI_IO_BAR_SIZE     0x20
#define I8255X_PCI_FLASH_BAR_SIZE  0x100000

#define I8255X_EEPROM_WORDS  64

#define I8255X_IO_DECODE_MASK      (I8255X_PCI_IO_BAR_SIZE - 1)

#define I8255X_PHY_ADDRESS  1

#define I8255X_REG_SCB_STATUS        0x00
#define I8255X_REG_SCB_COMMAND       0x02
#define I8255X_REG_PORT              0x08
#define I8255X_REG_EEPROM_CONTROL    0x0E
#define I8255X_REG_MDI_CONTROL       0x10
#define I8255X_REG_RX_DMA_BYTE_COUNT 0x14
#define I8255X_REG_FLOW_CONTROL      0x18
#define I8255X_REG_PMDR              0x1B
#define I8255X_REG_GENERAL_CTRL      0x1C
#define I8255X_REG_GENERAL_STATUS    0x1D

#define I8255X_EEPROM_SK    0x0001
#define I8255X_EEPROM_CS    0x0002
#define I8255X_EEPROM_DI    0x0004
#define I8255X_EEPROM_DO    0x0008

#define I8255X_PORT_SELF_TEST    0x0000001

typedef struct nic_t {
    mem_mapping_t mmio_bar_mapping;
    mem_mapping_t flash_bar_mapping;
    nmc93cxx_eeprom_t *eeprom;
    uint8_t pci_slot;
    uint8_t pci_config[256];
    uint8_t io_regs[I8255X_PCI_IO_BAR_SIZE];
    uint16_t mii_regs[32];
    uint32_t mii_read_latch;
    uint8_t eeprom_data[I8255X_EEPROM_WORDS * 2];
} nic_t;

