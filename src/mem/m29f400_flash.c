/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the M29F400-compatible flash devices.
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
#include <assert.h>
#include <wchar.h>

#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include <86box/nvr.h>
#include <86box/plat.h>

#define CMD_CHIP_ERASE_CONFIRM       0x10
#define CMD_BLOCK_ERASE_CONFIRM      0x30
#define CMD_ERASE_RESUME             CMD_BLOCK_ERASE_CONFIRM
#define CMD_SETUP_ERASE              0x80
#define CMD_AUTO_SELECT              0x90
#define CMD_PROGRAM                  0xA0
#define CMD_ERASE_SUSPEND            0xB0
#define CMD_READ_ARRAY               0xF0

#define STATUS_ALT_TOGGLE            0x04 /* DQ2 */
#define STATUS_ERASE_TIMEOUT_EXPIRED 0x08 /* DQ3 */
#define STATUS_ERROR                 0x20 /* DQ5 */
#define STATUS_TOGGLE                0x40 /* DQ6 */
#define STATUS_DATA_POLLING          0x80 /* DQ7 */

#define FLASH_BLOCK_NOT_PROTECTED    0x00
#define FLASH_BLOCK_PROTECTED        0x01

/* A0-A14 */
#define FLASH_CODED_CYCLE_ADDRESS_MASK   0x7FFF

/* A12-A17 */
#define FLASH_BLOCK_ERASE_ADDRESS_MASK   0x3F000

/* A0-A1 */
#define FLASH_AUTOSELECT_ADDRESS_MASK           0x3

/* Read the manufacturer ID. A0 = 0, A1 = 0 */
#define FLASH_AUTOSELECT_ADDR_MANUFACTURER_ID   0x0

/* Read the model ID. A0 = 1, A1 = 0 */
#define FLASH_AUTOSELECT_ADDR_MODEL_ID          0x1

#define FLASH_BLOCK_ERASE_ACCEPT_TIMEOUT  50.0
#define FLASH_PROGRAM_TIME                100.0
#define FLASH_ERASE_TIME_NO_DATA          150.0
#define FLASH_BLOCK_ERASE_TIME            1000000.0 /* 1.0 sec */
#define FLASH_CHIP_ERASE_TIME             4300000.0 /* 4.3 sec */

#define FLASH_MAX_BLOCKS   11

/* 512 kB */
#define M29F400T_FLASH_SIZE     (512 * 1024)

#define M29F400T_FLASH_IO_BASE_LOW   0x00080000
#define M29F400T_FLASH_IO_BASE_HIGH  0xFFF80000

/* ST */
#define M29F400T_MANUFACTURER_ID   0x20

/* Integrated Silicon Solution (ISSI) */
#define M29F400T_MODEL_ID          0x00D5

typedef enum BusCycleState {
    CYCLE_INVALID,
    CYCLE_CHECK_55,
    CYCLE_CHECK_AA,
    CYCLE_CHECK_FIRST_CMD,
    CYCLE_CHECK_SECOND_CMD,
    CYCLE_ENTER_PROGRAM,
} BusCycleState;

typedef enum DeviceMode {
    M_READ_ARRAY = 0, // Also used for Erase Suspend
    M_AUTO_SELECT,
    M_PROGRAM,
    M_BLOCK_ERASE,
    M_CHIP_ERASE,
    M_MAX,
} DeviceMode;

typedef struct flash_block_t
{
    uint32_t number;
    uint32_t start_addr;
    uint32_t end_addr;
    uint8_t protection_status;
} flash_block_t;

typedef struct flash_t
{
    DeviceMode mode;

    bool in_16_bit_mode;

    uint8_t bus_cycle;
    uint8_t cmd_cycle;
    uint8_t status_reg;
    uint8_t manufacturer_id;
    uint16_t model_id;

    uint32_t addr_decode_mask;
    uint32_t addr_select_shift;
    uint32_t addr_AAAA_phys;
    uint32_t addr_5555_phys;
    uint32_t blocks_to_erase_bitmap;

    flash_block_t block[FLASH_MAX_BLOCKS];

    uint8_t array[M29F400T_FLASH_SIZE];

    pc_timer_t erase_accept_timeout_timer;
    pc_timer_t cmd_complete_timer;

    mem_mapping_t flash_mapping_low;
    mem_mapping_t flash_mapping_high;

    char flash_path[1024];
} flash_t;

#ifdef ENABLE_M29F400_LOG
int m29f400_do_log = ENABLE_M29F400_LOG;

static void
m29f400_log(const char *fmt, ...)
{
    va_list ap;

    if (m29f400_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define m29f400_log(fmt, ...)
#endif

static inline void
m29f400_set_mode(flash_t *dev, DeviceMode mode)
{
#ifdef ENABLE_M29F400_LOG
    static const char *mode_names[] = {
        "Read Array",
        "Auto Select",
        "Program",
        "Block Erase",
        "Chip Erase",
    };
    const char *name;

    if (mode < M_MAX) {
        name = mode_names[mode];
    } else {
        name = "<Unknown>";
    }

    if (mode != dev->mode) {
        m29f400_log("FLASH: Set %s mode\n", name);
    }
#endif

    dev->mode = mode;
}

static flash_block_t*
m29f400_address_to_block(flash_t *dev, uint32_t addr)
{
    flash_block_t *block;
    uint32_t i;

    for (i = 0; i < FLASH_MAX_BLOCKS; ++i) {
        block = &dev->block[i];

        if ((addr >= block->start_addr) && (addr <= block->end_addr))
            return block;
    }

    /* Should not happen */
    assert(false);
    return NULL;
}

static void
m29f400_reset_cmd_sequence(flash_t *dev)
{
    dev->bus_cycle = 0;
    dev->cmd_cycle = 0;
}

static void
m29f400_reset_cmd(flash_t *dev)
{
    m29f400_set_mode(dev, M_READ_ARRAY);
    m29f400_reset_cmd_sequence(dev);

    dev->status_reg = 0;
    dev->blocks_to_erase_bitmap = 0;

    /* Terminate the block erase timeout */
    timer_stop(&dev->erase_accept_timeout_timer);
}

static void
m29f400_cmd_complete_timer_callback(void *priv)
{
    flash_t *dev = priv;

    m29f400_log("FLASH: Command completed with status %02X\n", dev->status_reg);

    /* The memory will return to the Read Mode, unless an error has occurred */
    if (dev->status_reg & STATUS_ERROR) {
        return;
    }
    m29f400_reset_cmd(dev);
}

static bool
m29f400_erase_blocks(flash_t *dev, int pattern)
{
    bool was_erased = false;
    uint32_t i;

    for (i = 0; i < FLASH_MAX_BLOCKS; ++i) {
        flash_block_t *block = &dev->block[i];
        void *block_start;
        size_t block_length;

        if (!(dev->blocks_to_erase_bitmap & (1 << block->number))) {
            continue;
        }

        /* Protected block: The data remains unchanged, no error is given */
        if (block->protection_status == FLASH_BLOCK_PROTECTED) {
            continue;
        }

        m29f400_log("FLASH: Erase block #%lu %lX-%lX\n",
                    block->number,
                    block->start_addr,
                    block->end_addr);

        block_start = dev->array + block->start_addr;
        block_length = (block->end_addr - block->start_addr) + 1;

        memset(block_start, pattern, block_length);
        was_erased = true;
    }

    return was_erased;
}

static void
m29f400_erase_begin_timer_callback(void *priv)
{
    flash_t *dev = priv;
    double period;
    bool was_erased = false;

    /* This status bit is the same for Block Erase and Chip Erase */
    dev->status_reg |= STATUS_ERASE_TIMEOUT_EXPIRED;

    /* Finally, erase the blocks (fill with 0xFF) */
    was_erased = m29f400_erase_blocks(dev, 0xFF);

    /*
     * If all of the selected blocks are protected,
     * the operation will terminate within about 100us.
     */
    if (!was_erased) {
        period = FLASH_ERASE_TIME_NO_DATA;
    } else if (dev->mode == M_BLOCK_ERASE) {
        period = FLASH_BLOCK_ERASE_TIME;
    } else {
        period = FLASH_CHIP_ERASE_TIME;
    }
    timer_on_auto(&dev->cmd_complete_timer, period);
}

static void
m29f400_check_for_erasure_abort(flash_t *dev)
{
    /*
     * Erase Suspend: A Read/Reset command will definitively abort erasure
     * and result in invalid data in the blocks being erased.
     */
    if (dev->blocks_to_erase_bitmap != 0) {
        m29f400_log("FLASH: Block Erase abort %08lX\n", dev->blocks_to_erase_bitmap);

        /*
         * Simulate the effect of erasure being interrupted.
         * The software, therefore, has to check the change of memory array values again.
         */
        m29f400_erase_blocks(dev, 0xCC);
    }
}

static uint8_t
m29f400_status_register_read(flash_t *dev, bool is_read_from_erasing_block)
{
    switch (dev->mode) {
        case M_READ_ARRAY: {
            /* Erase Suspend */
            assert(is_read_from_erasing_block);

            dev->status_reg ^= STATUS_ALT_TOGGLE;
            break;
        }

        case M_PROGRAM: {
            dev->status_reg ^= STATUS_TOGGLE;
            break;
        }

        case M_BLOCK_ERASE: {
            dev->status_reg ^= STATUS_TOGGLE;

            if (is_read_from_erasing_block)
                dev->status_reg ^= STATUS_ALT_TOGGLE;
            break;
        }

        case M_CHIP_ERASE: {
            dev->status_reg ^= (STATUS_TOGGLE | STATUS_ALT_TOGGLE);
            break;
        }

        default:
            break;
    }

    return dev->status_reg;
}

static void
m29f400_accept_cmd(flash_t *dev, uint32_t addr, uint16_t val)
{
    flash_block_t *block;

    /* Single cycle commands (write to any address inside the device) */
    switch (val) {
        case CMD_READ_ARRAY: {
            m29f400_check_for_erasure_abort(dev);
            m29f400_reset_cmd(dev);
            return;
        }

        case CMD_ERASE_SUSPEND: {
            if (dev->mode != M_BLOCK_ERASE) {
                break;
            }

            m29f400_log("FLASH: Erase suspend\n");

            /* Suspend the erase operation */
            timer_stop(&dev->erase_accept_timeout_timer);
            timer_stop(&dev->cmd_complete_timer);

            dev->status_reg |= STATUS_DATA_POLLING | STATUS_TOGGLE;

            /* Return the memory to Read mode */
            m29f400_set_mode(dev, M_READ_ARRAY);
            return;
        }

        case CMD_ERASE_RESUME: {
            if (dev->mode != M_READ_ARRAY || dev->blocks_to_erase_bitmap == 0) {
                break;
            }

            m29f400_log("FLASH: Erase resume\n");

            dev->status_reg &= ~STATUS_DATA_POLLING;

            /* Resume the erase operation */
            if (dev->status_reg & STATUS_ERASE_TIMEOUT_EXPIRED) {
                timer_on_auto(&dev->cmd_complete_timer, FLASH_BLOCK_ERASE_TIME);
            } else {
                timer_on_auto(&dev->erase_accept_timeout_timer, FLASH_BLOCK_ERASE_ACCEPT_TIMEOUT);
            }
            m29f400_set_mode(dev, M_BLOCK_ERASE);
            return;
        }

        default:
            break;
    }

    switch (dev->mode) {
        case M_PROGRAM: {
            uint16_t current_value;

            block = m29f400_address_to_block(dev, addr);

            /* Write to a protected block: The data remains unchanged, no error is given */
            if (block->protection_status == FLASH_BLOCK_PROTECTED) {
                m29f400_log("FLASH: Program failure - the block #lu is protected\n", block->number);
                goto ProgramDone;
            }

            if (dev->in_16_bit_mode) {
                if (addr & (sizeof(uint16_t) - 1)) {
                    m29f400_log("FLASH: Program error - the address %lX is unaligned\n", addr);

                    dev->status_reg |= STATUS_ERROR;
                    goto ProgramDone;
                }

                current_value = dev->array[addr];
                current_value |= dev->array[addr + 1] << 8;
            } else {
                current_value = dev->array[addr];
            }

            /* The program command cannot change a '0' bit to a '1' */
            if (~current_value & val) {
                m29f400_log("FLASH: Program error - the address %lX "
                            "was not previously erased %04X <> %04X\n",
                            addr,
                            current_value,
                            val);

                dev->status_reg |= STATUS_ERROR;
                goto ProgramDone;
            }

            dev->status_reg = STATUS_ALT_TOGGLE;
            if (!(val & STATUS_DATA_POLLING)) {
                dev->status_reg |= STATUS_DATA_POLLING;
            }

            /* Finally, program the value */
            if (dev->in_16_bit_mode) {
                dev->array[addr] = val & 0xFF;
                dev->array[addr + 1] = val >> 8;
            } else {
                dev->array[addr] = val;
            }
            m29f400_log("FLASH: Program %lX value %04X to %04X\n", addr, current_value, val);

ProgramDone:
            timer_on_auto(&dev->cmd_complete_timer, FLASH_PROGRAM_TIME);
            break;
        }

        case M_BLOCK_ERASE: {
            /* We shouldn't get here if the operation has already started */
            assert(!(dev->status_reg & STATUS_ERASE_TIMEOUT_EXPIRED));

            addr &= FLASH_BLOCK_ERASE_ADDRESS_MASK << dev->addr_select_shift;
            block = m29f400_address_to_block(dev, addr);

            m29f400_log("FLASH: Queued block #%lu %lX-%lX for erase\n",
                        block->number,
                        block->start_addr,
                        block->end_addr);

            /* Add block to the list */
            dev->blocks_to_erase_bitmap |= 1 << block->number;

            /* Wait for a next block to erase */
            timer_stop(&dev->erase_accept_timeout_timer);
            timer_on_auto(&dev->erase_accept_timeout_timer, FLASH_BLOCK_ERASE_ACCEPT_TIMEOUT);
            break;
        }

        case M_CHIP_ERASE: {
            /* Add all blocks to the list */
            dev->blocks_to_erase_bitmap = 0xFFFFFFFF;

            /* Immediately start the erase operation */
            m29f400_erase_begin_timer_callback(dev);
            break;
        }

        default:
            break;
    }
}

static void
m29f400_interpret_cmd_sequence(flash_t *dev, uint32_t addr, uint16_t val)
{
    static const uint8_t cmd_seq_next_state[6][2] = {
        //    Phase 0                 Phase 1
        { CYCLE_CHECK_AA,         CYCLE_INVALID       }, // Cycle 1
        { CYCLE_CHECK_55,         CYCLE_INVALID       }, // Cycle 2
        { CYCLE_CHECK_FIRST_CMD,  CYCLE_INVALID       }, // Cycle 3
        { CYCLE_CHECK_AA,         CYCLE_ENTER_PROGRAM }, // Cycle 4
        { CYCLE_CHECK_55,         CYCLE_INVALID       }, // Cycle 5
        { CYCLE_CHECK_SECOND_CMD, CYCLE_INVALID       }, // Cycle 6
    };
    uint8_t cycle_state;

    addr &= FLASH_CODED_CYCLE_ADDRESS_MASK << dev->addr_select_shift;

    cycle_state = cmd_seq_next_state[dev->bus_cycle][dev->cmd_cycle];
    switch (cycle_state) {
        case CYCLE_CHECK_AA: {
            if (val == 0xAA && addr == dev->addr_AAAA_phys) {
                dev->bus_cycle++;
            } else {
                m29f400_reset_cmd_sequence(dev);
            }
            break;
        }

        case CYCLE_CHECK_55: {
            if (val == 0x55 && addr == dev->addr_5555_phys) {
                dev->bus_cycle++;
            } else {
                m29f400_reset_cmd_sequence(dev);
            }
            break;
        }

        case CYCLE_CHECK_FIRST_CMD: {
            if (addr != dev->addr_AAAA_phys) {
                m29f400_reset_cmd_sequence(dev);
                break;
            }

            switch (val) {
                case CMD_READ_ARRAY:
                    m29f400_set_mode(dev, M_READ_ARRAY);
                    break;

                case CMD_AUTO_SELECT:
                    m29f400_set_mode(dev, M_AUTO_SELECT);
                    break;

                case CMD_PROGRAM:
                    dev->bus_cycle++;
                    dev->cmd_cycle++;
                    break;

                case CMD_SETUP_ERASE:
                    dev->bus_cycle++;
                    break;

                default:
                    m29f400_reset_cmd_sequence(dev);
                    break;
            }
            break;
        }

        case CYCLE_ENTER_PROGRAM: {
            m29f400_set_mode(dev, M_PROGRAM);
            break;
        }

        case CYCLE_CHECK_SECOND_CMD: {
            switch (val) {
                case CMD_BLOCK_ERASE_CONFIRM: {
                    m29f400_set_mode(dev, M_BLOCK_ERASE);
                    break;
                }

                case CMD_CHIP_ERASE_CONFIRM: {
                    if (addr == dev->addr_AAAA_phys) {
                        m29f400_set_mode(dev, M_CHIP_ERASE);
                    } else {
                        m29f400_reset_cmd_sequence(dev);
                    }
                    break;
                }

                default:
                    m29f400_reset_cmd_sequence(dev);
                    break;
            }
            break;
        }

        default:
            m29f400_reset_cmd_sequence(dev);
            break;
    }
}

/* Common write handler for 8- or 16-bit bus mode */
static void
m29f400_mmio_write(flash_t *dev, uint32_t addr, uint16_t val)
{
    addr &= dev->addr_decode_mask;

    m29f400_log("FLASH: [W16] [%lX] <-- %X\n", addr, val);

    switch (dev->mode) {
        /* Ignore all commands while the chip is being programmed or erased */
        case M_CHIP_ERASE:
        case M_PROGRAM: {
            /* A Read/Reset command can be issued to reset the error condition */
            if ((dev->status_reg & STATUS_ERROR) && (val == CMD_READ_ARRAY))
                break;
            return;
        }

        /*
         * Ignore all commands during the Block Erase
         * except the Erase Suspend and Read/Reset command.
         */
        case M_BLOCK_ERASE: {
            /* The command has not started yet, we keep accepting blocks to the erase list */
            if (!(dev->status_reg & STATUS_ERASE_TIMEOUT_EXPIRED))
                break;

            if (val == CMD_ERASE_SUSPEND || val == CMD_READ_ARRAY)
                break;
            return;
        }

        default:
            break;
    }

    /* Receive the command sequence */
    if (dev->mode == M_READ_ARRAY) {
        m29f400_interpret_cmd_sequence(dev, addr, val);
    }

    /* Begin the operation */
    m29f400_accept_cmd(dev, addr, val);
}

/* Common read handler for 8- or 16-bit bus mode */
static uint16_t
m29f400_mmio_read(flash_t *dev, uint32_t addr)
{
    flash_block_t *block;
    uint16_t ret;

    addr &= dev->addr_decode_mask;

    block = m29f400_address_to_block(dev, addr);

    switch (dev->mode) {
        case M_AUTO_SELECT: {
            /* Note that it is possible to enter the Auto Select mode during Erase Suspend */
            switch ((addr >> dev->addr_select_shift) & FLASH_AUTOSELECT_ADDRESS_MASK) {
                case FLASH_AUTOSELECT_ADDR_MANUFACTURER_ID:
                    ret = dev->manufacturer_id;
                    break;

                case FLASH_AUTOSELECT_ADDR_MODEL_ID:
                    ret = dev->model_id;
                    break;

                default:
                    /* Read the block protection status */
                    ret = block->protection_status;
                    break;
            }
            break;
        }

        case M_READ_ARRAY: {
            if (dev->blocks_to_erase_bitmap & (1 << block->number)) {
                /* Erase Suspend: Return the Status Register if we read from a block being erased */
                ret = m29f400_status_register_read(dev, true);
            } else {
                /* Read array data */
                if (dev->in_16_bit_mode) {
                    if (addr < (M29F400T_FLASH_SIZE - 1)) {
                        ret = dev->array[addr];
                        ret |= dev->array[addr + 1] << 8;
                    } else {
                        ret = 0xFFFF;
                    }
                } else {
                    ret = dev->array[addr];
                }
            }
            break;
        }

        /* Return the Status Register during Program and Erase operations */
        default:
            assert(dev->mode == M_PROGRAM || dev->mode == M_BLOCK_ERASE || dev->mode == M_CHIP_ERASE);

            ret = m29f400_status_register_read(dev, !!dev->blocks_to_erase_bitmap & (1 << block->number));
            break;
    }

    m29f400_log("FLASH: [R16] [%lX] --> %X\n", addr, ret);

    return ret;
}

static uint8_t
m29f400_mmio_read8(uint32_t addr, void* priv)
{
    return m29f400_mmio_read(priv, addr);
}

static void
m29f400_mmio_write8(uint32_t addr, uint8_t val, void* priv)
{
    m29f400_mmio_write(priv, addr, val);
}

static uint16_t
m29f400_mmio_read16(uint32_t addr, void* priv)
{
    flash_t *dev = priv;
    uint16_t ret;

    /* Split the read into two accesses when the device is in 8-bit bus mode */
    if (dev->in_16_bit_mode) {
        ret = m29f400_mmio_read(dev, addr);
    } else {
        ret = m29f400_mmio_read(dev, addr);
        ret |= ((uint16_t)m29f400_mmio_read(dev, addr)) << 16;
    }

    return ret;
}

static void
m29f400_mmio_write16(uint32_t addr, uint16_t val, void* priv)
{
    flash_t *dev = priv;

    /* Split the write into two accesses when the device is in 8-bit bus mode */
    if (dev->in_16_bit_mode) {
        m29f400_mmio_write(dev, addr, val);
    } else {
        m29f400_mmio_write(dev, addr, val & 0xFF);
        m29f400_mmio_write(dev, addr + 1, val >> 8);
    }
}

static void
m29f400_finish_init(flash_t *dev)
{
    mem_mapping_disable(&bios_mapping);
    mem_mapping_disable(&bios_high_mapping);

    mem_mapping_add(&dev->flash_mapping_low,
                    M29F400T_FLASH_IO_BASE_LOW,
                    M29F400T_FLASH_SIZE,
                    m29f400_mmio_read8,
                    m29f400_mmio_read16,
                    NULL,
                    m29f400_mmio_write8,
                    m29f400_mmio_write16,
                    NULL,
                    dev->array,
                    MEM_MAPPING_EXTERNAL | MEM_MAPPING_ROM | MEM_MAPPING_ROMCS,
                    dev);

    mem_mapping_add(&dev->flash_mapping_high,
                    M29F400T_FLASH_IO_BASE_HIGH,
                    M29F400T_FLASH_SIZE,
                    m29f400_mmio_read8,
                    m29f400_mmio_read16,
                    NULL,
                    m29f400_mmio_write8,
                    m29f400_mmio_write16,
                    NULL,
                    dev->array,
                    MEM_MAPPING_EXTERNAL | MEM_MAPPING_ROM | MEM_MAPPING_ROMCS,
                    dev);

    /* Assign block numbers */
    for (uint32_t i = 0; i < FLASH_MAX_BLOCKS; ++i) {
        flash_block_t *block = &dev->block[i];

        assert(i < 32);
        block->number = i;
    }

    dev->addr_decode_mask = M29F400T_FLASH_SIZE - 1;

    /*
     * For information about 8- or 16-bit bus mode mapping, see especially
     * "AN202720 Connecting Cypress Flash Memory to a System Address Bus".
     * The Cypress S29CD devices have a nearly identical design to M29F400.
     *
     * On SGI 320/540 systems, the flash memory is configured to operate in 16-bit bus mode.
     * The address line A1 of the CPU is connected to A0 of the flash memory.
     * This means, that the software can only use a x8 address range to access a word.
     *
     *    Standard 8-bit mode          Standard 16-bit mode            16-bit mode on SGI 320/540
     *                                   for a 16-bit CPU
     * Read manufacturer ID:
     * *(uint8_t*)0x00              *(uint16_t*)0x00                *(uint16_t*)0x00
     *
     * Read device code:
     * *(uint8_t*)0x02              *(uint16_t*)0x01                *(uint16_t*)0x02
     *
     * Read array byte 8 and 9:
     * *(uint8_t*)0x08              *(uint16_t*)0x04                *(uint16_t*)0x08
     * *(uint8_t*)0x09
     *
     * The command patterns:
     * *(uint8_t*)0xAAAA = 0xAA     *(uint16_t*)0x5555 = 0x00AA     *(uint16_t*)0xAAAA = 0x00AA
     * *(uint8_t*)0x5555 = 0x55     *(uint16_t*)0x2AAA = 0x0055     *(uint16_t*)0x5554 = 0x0055
     *
     * So we have two formulas to calculate the address:
     * 0xAAAA >> 1 = 0x5555
     * 0x5555 << 1 = 0xAAAA
     * and
     * 0x5555 >> 1 = 0x2AAA
     * 0x2AAA << 1 = 0x5554
     */
    if (dev->in_16_bit_mode) {
        dev->addr_select_shift = 1;

        dev->addr_5555_phys = (dev->addr_5555_phys >> 1) << dev->addr_select_shift;
        dev->addr_AAAA_phys = (dev->addr_AAAA_phys >> 1) << dev->addr_select_shift;
    }
}

static void
m29f400_reset(void *priv)
{
    flash_t *dev = priv;

    /* Reset on power up to Read Array */
    m29f400_reset_cmd(dev);
}

static void *
m29f400_init(UNUSED(const device_t *info))
{
    flash_t *dev = calloc(1, sizeof(flash_t));
    size_t bytes_written;
    FILE *fp;

    snprintf(dev->flash_path, sizeof(dev->flash_path), "%s.bin", machine_get_internal_name_ex(machine));

    /* Load the flash image, if it is already present in the system */
    fp = nvr_fopen(dev->flash_path, "rb");
    if (fp) {
        bytes_written = fread(dev->array, 1, M29F400T_FLASH_SIZE, fp);

        fclose(fp);
    } else {
        bytes_written = MIN(biosmask + 1, M29F400T_FLASH_SIZE);

        /* Clone the ROM data to create a new image */
        memcpy(dev->array, rom, bytes_written);
    }

    /* Fill the rest with 0xFF (make the memory content erased) */
    if (bytes_written < M29F400T_FLASH_SIZE) {
        pclog("Less than %lu bytes read from the M29F400 Flash ROM file\n", (uint32_t)bytes_written);

        memset(dev->array + bytes_written, 0xFF, M29F400T_FLASH_SIZE - bytes_written);
    }

    timer_add(&dev->erase_accept_timeout_timer, m29f400_erase_begin_timer_callback, dev, 0);
    timer_add(&dev->cmd_complete_timer, m29f400_cmd_complete_timer_callback, dev, 0);

    /* 64K MAIN BLOCK */
    dev->block[0].start_addr  = 0x00000;
    dev->block[0].end_addr    = 0x0FFFF;
    /* 64K MAIN BLOCK */
    dev->block[1].start_addr  = 0x10000;
    dev->block[1].end_addr    = 0x1FFFF;
    /* 64K MAIN BLOCK */
    dev->block[2].start_addr  = 0x20000;
    dev->block[2].end_addr    = 0x2FFFF;
    /* 64K MAIN BLOCK */
    dev->block[3].start_addr  = 0x30000;
    dev->block[3].end_addr    = 0x3FFFF;
    /* 64K MAIN BLOCK */
    dev->block[4].start_addr  = 0x40000;
    dev->block[4].end_addr    = 0x4FFFF;
    /* 64K MAIN BLOCK */
    dev->block[5].start_addr  = 0x50000;
    dev->block[5].end_addr    = 0x5FFFF;
    /* 64K MAIN BLOCK */
    dev->block[6].start_addr  = 0x60000;
    dev->block[6].end_addr    = 0x6FFFF;
    /* 32K MAIN BLOCK */
    dev->block[7].start_addr  = 0x70000;
    dev->block[7].end_addr    = 0x77FFF;
    /* 8K PARAMETER BLOCK */
    dev->block[8].start_addr  = 0x78000;
    dev->block[8].end_addr    = 0x79FFF;
    /* 8K PARAMETER BLOCK */
    dev->block[9].start_addr  = 0x7A000;
    dev->block[9].end_addr    = 0x7BFFF;
    /* 16K BOOT BLOCK */
    dev->block[10].start_addr = 0x7C000;
    dev->block[10].end_addr   = 0x7FFFF;

    dev->addr_AAAA_phys = 0xAAAA;
    dev->addr_5555_phys = 0x5555;
    dev->in_16_bit_mode = true;
    dev->manufacturer_id = M29F400T_MANUFACTURER_ID;
    dev->model_id = M29F400T_MODEL_ID;

    m29f400_finish_init(dev);

    return dev;
}

static void
m29f400_close(void *priv)
{
    flash_t *dev = priv;
    FILE *fp;

    fp = nvr_fopen(dev->flash_path, "wb");
    assert(fp != NULL);

    /* Replace the original flash image with new version */
    fwrite(dev->array, M29F400T_FLASH_SIZE, 1, fp);
    fclose(fp);

    free(dev);
}

const device_t m29f400t_flash_device = {
    .name          = "ST M29F400T Flash BIOS",
    .internal_name = "m29f400t_flash",
    .flags         = DEVICE_PCI,
    .local         = 0,
    .init          = m29f400_init,
    .close         = m29f400_close,
    .reset         = m29f400_reset,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
