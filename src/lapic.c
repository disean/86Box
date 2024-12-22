/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Local APIC emulation (Stub).
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
#include "cpu.h"
#include <86box/pci.h>
#include <86box/device.h>
#include <86box/mem.h>

#define LAPIC_IO_BASE                 0xFEE00000
#define LAPIC_IO_SIZE                 0x1000

/*
 * On real hardware, LAPIC has memory aliases every 8 bytes:
 * 0xFEE00028 = 0xFEE00020, 0xFEE00038 = 0xFEE00030 and so on;
 * all other LAPIC registers (0xFEE00024, 0xFEE00034, etc) cannot be written
 * and are hard-wired to zero.
 */
#define LAPIC_IO_DECODE_MASK          0x0FF4
#define LAPIC_IO_INVALID_MASK         0x0004
#define LAPIC_IO_DEFAULT_VALUE        0

/*
 * Compilers produce better code for a switch statement
 * when we pass registers by index.
 * This is possible since LAPIC consists of 16-byte aligned registers.
 */
#define REG_TO_IDX(x)  ((x) / 16)

/*
 * Writable registers
 */
#define LAPIC_REG_ID                  REG_TO_IDX(0x020)
#define LAPIC_REG_TASK_PRIORITY       REG_TO_IDX(0x080)
#define LAPIC_REG_EOI                 REG_TO_IDX(0x0B0)
#define LAPIC_REG_LOCAL_DESTINATION   REG_TO_IDX(0x0D0)
#define LAPIC_REG_DESTINATION_FORMAT  REG_TO_IDX(0x0E0)
#define LAPIC_REG_SPURIOUS_INT_VECTOR REG_TO_IDX(0x0F0)
#define LAPIC_REG_ERROR_STATUS        REG_TO_IDX(0x280)
#define LAPIC_REG_LVT_CMCI            REG_TO_IDX(0x2F0) // Introduced in Intel Xeon 5500
#define LAPIC_REG_ICR_LOW             REG_TO_IDX(0x300)
#define LAPIC_REG_ICR_HIGH            REG_TO_IDX(0x310)
#define LAPIC_REG_LVT_TIMER           REG_TO_IDX(0x320)
#define LAPIC_REG_LVT_THERMAL_SENSOR  REG_TO_IDX(0x330) // Introduced in Pentium 4 and Intel Xeon
#define LAPIC_REG_LVT_COUNTERS        REG_TO_IDX(0x340) // Introduced in Pentium Pro
#define LAPIC_REG_LVT_LINT0           REG_TO_IDX(0x350)
#define LAPIC_REG_LVT_LINT1           REG_TO_IDX(0x360)
#define LAPIC_REG_LVT_ERROR           REG_TO_IDX(0x370)
#define LAPIC_REG_TIMER_INITIAL_COUNT REG_TO_IDX(0x380)
#define LAPIC_REG_TIMER_DIVIDE_CONFIG REG_TO_IDX(0x3E0)

/*
 * Read-only registers
 */
#define LAPIC_REG_VER                 REG_TO_IDX(0x030)
#define LAPIC_REG_ARB_PRIORITY        REG_TO_IDX(0x090) // Not supported in Pentium 4 and Intel Xeon
#define LAPIC_REG_PROCESSOR_PRIORITY  REG_TO_IDX(0x0A0)
#define LAPIC_REG_REMOTE_READ         REG_TO_IDX(0x0C0) // Not supported in Pentium 4 and Intel Xeon
#define LAPIC_REG_ISR_0               REG_TO_IDX(0x100)
#define LAPIC_REG_ISR_1               REG_TO_IDX(0x110)
#define LAPIC_REG_ISR_2               REG_TO_IDX(0x120)
#define LAPIC_REG_ISR_3               REG_TO_IDX(0x130)
#define LAPIC_REG_ISR_4               REG_TO_IDX(0x140)
#define LAPIC_REG_ISR_5               REG_TO_IDX(0x150)
#define LAPIC_REG_ISR_6               REG_TO_IDX(0x160)
#define LAPIC_REG_ISR_7               REG_TO_IDX(0x170)
#define LAPIC_REG_TRIGGER_MODE_0      REG_TO_IDX(0x180)
#define LAPIC_REG_TRIGGER_MODE_1      REG_TO_IDX(0x190)
#define LAPIC_REG_TRIGGER_MODE_2      REG_TO_IDX(0x1A0)
#define LAPIC_REG_TRIGGER_MODE_3      REG_TO_IDX(0x1B0)
#define LAPIC_REG_TRIGGER_MODE_4      REG_TO_IDX(0x1C0)
#define LAPIC_REG_TRIGGER_MODE_5      REG_TO_IDX(0x1D0)
#define LAPIC_REG_TRIGGER_MODE_6      REG_TO_IDX(0x1E0)
#define LAPIC_REG_TRIGGER_MODE_7      REG_TO_IDX(0x1F0)
#define LAPIC_REG_IRR_0               REG_TO_IDX(0x200)
#define LAPIC_REG_IRR_1               REG_TO_IDX(0x210)
#define LAPIC_REG_IRR_2               REG_TO_IDX(0x220)
#define LAPIC_REG_IRR_3               REG_TO_IDX(0x230)
#define LAPIC_REG_IRR_4               REG_TO_IDX(0x240)
#define LAPIC_REG_IRR_5               REG_TO_IDX(0x250)
#define LAPIC_REG_IRR_6               REG_TO_IDX(0x260)
#define LAPIC_REG_IRR_7               REG_TO_IDX(0x270)
#define LAPIC_REG_TIMER_CURRENT_COUNT REG_TO_IDX(0x390)

/*
 * Version register
 */
#define LAPIC_VER_VERSION_MASK              0x000000FF
#define LAPIC_VER_MAX_LVT_ENTRY_MASK        0x00FF0000
#define LAPIC_VER_HAS_EOI_BROADCAST_SUPPR   0x01000000

#define LAPIC_VER_82489DX_DESCRETE          0x00000000
#define LAPIC_VER_INTEGRATED_APIC_V11       0x00000011

#define LAPIC_VER_MAX_LVT_ENTRY_SHIFT       16

/*
 * Task-Priority register
 */
#define LAPIC_TPR_PRIORITY_SUB_CLASS_MASK   0x0000000F
#define LAPIC_TPR_PRIORITY_CLASS_MASK       0x000000F0

#define update_reg(_value, _write_bits_mask, _reg) \
    (_reg) = ((_value) & (_write_bits_mask)) | ((_reg) & ~(_write_bits_mask));

typedef struct lapic_regs_t {
    uint32_t id;
    uint32_t version;
    uint32_t task_priority;
    uint32_t arbitration_priority;
    uint32_t processor_priority;
    uint32_t remote_read;
    uint32_t local_destination;
    uint32_t destination_format;
    uint32_t spurious_int_vector;
    uint32_t in_service[8];
    uint32_t trigger_mode[8];
    uint32_t interrupt_request[8];
    uint32_t error_status;
    uint32_t lvt_cmci;
    uint32_t interrupt_command_low;
    uint32_t interrupt_command_high;
    uint32_t lvt_timer;
    uint32_t lvt_thermal_sensor;
    uint32_t lvt_performance_counters;
    uint32_t lvt_lint0;
    uint32_t lvt_lint1;
    uint32_t lvt_error;
    uint32_t timer_initial_count;
    uint32_t timer_current_count;
    uint32_t timer_divider;
} lapic_regs_t;

typedef struct lapic_t {
    lapic_regs_t regs;
    uint32_t pending_error_status;
    mem_mapping_t mmio_mapping;
} lapic_t;

#define ENABLE_LAPIC_LOG 1
#ifdef ENABLE_LAPIC_LOG
int lapic_do_log = ENABLE_LAPIC_LOG;

static void
lapic_log(const char *fmt, ...)
{
    va_list ap;

    if (lapic_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define lapic_log(fmt, ...)
#    define lapic_reg_name(lapic_reg)
#endif

static void
lapic_write32(uint32_t lapic_reg, uint32_t val, void* priv)
{
    lapic_t *dev = priv;

    switch (lapic_reg) {
        case LAPIC_REG_ID:
            update_reg(val, 0x0F000000, dev->regs.id);
            break;

        case LAPIC_REG_TASK_PRIORITY:
            update_reg(val, 0x000000FF, dev->regs.task_priority);
            break;

        case LAPIC_REG_EOI:
            break;

        case LAPIC_REG_LOCAL_DESTINATION:
            update_reg(val, 0xFF000000, dev->regs.local_destination);
            break;

        case LAPIC_REG_DESTINATION_FORMAT:
            update_reg(val, 0xF0000000, dev->regs.destination_format);
            break;

        case LAPIC_REG_SPURIOUS_INT_VECTOR: {
            // TODO: Should be dynamic
            uint32_t write_bits = 0x000003F0;

            update_reg(val, write_bits, dev->regs.spurious_int_vector);
            break;
        }

        case LAPIC_REG_ERROR_STATUS:
            dev->regs.error_status = dev->pending_error_status;
            dev->pending_error_status = 0;
            break;

        case LAPIC_REG_LVT_CMCI:
            // TODO: Not implemented
            break;

        case LAPIC_REG_ICR_LOW:
            update_reg(val, 0x000CCFFF, dev->regs.interrupt_command_low);
            break;

        case LAPIC_REG_ICR_HIGH:
            update_reg(val, 0xFF000000, dev->regs.interrupt_command_high);
            break;

        case LAPIC_REG_LVT_TIMER:
            update_reg(val, 0x000307FF, dev->regs.lvt_timer);
            break;

        case LAPIC_REG_LVT_THERMAL_SENSOR:
            // TODO: Not implemented
            break;

        case LAPIC_REG_LVT_COUNTERS:
            update_reg(val, 0x000107FF, dev->regs.lvt_performance_counters);
            break;

        case LAPIC_REG_LVT_LINT0:
            update_reg(val, 0x0001A7FF, dev->regs.lvt_lint0);
            break;

        case LAPIC_REG_LVT_LINT1:
            update_reg(val, 0x0001A7FF, dev->regs.lvt_lint1);
            break;

        case LAPIC_REG_LVT_ERROR:
            update_reg(val, 0x000100FF, dev->regs.lvt_error);
            break;

        case LAPIC_REG_TIMER_INITIAL_COUNT:
            update_reg(val, 0xFFFFFFFF, dev->regs.timer_initial_count);
            break;

        case LAPIC_REG_TIMER_DIVIDE_CONFIG:
            update_reg(val, 0x0000000B, dev->regs.timer_divider);
            break;

        default:
            break;
    }
}

static uint32_t
lapic_read32(uint32_t lapic_reg, void* priv)
{
    lapic_t *dev = priv;
    uint32_t ret;

    switch (lapic_reg) {
        case LAPIC_REG_ID:
            ret = dev->regs.id;
            break;
        case LAPIC_REG_VER:
            ret = dev->regs.version;
            break;
        case LAPIC_REG_TASK_PRIORITY:
            ret = dev->regs.task_priority;
            break;
        case LAPIC_REG_ARB_PRIORITY:
            ret = dev->regs.arbitration_priority;
            break;
        case LAPIC_REG_PROCESSOR_PRIORITY:
            ret = dev->regs.processor_priority;
            break;
        case LAPIC_REG_REMOTE_READ:
            ret = dev->regs.remote_read;
            break;
        case LAPIC_REG_LOCAL_DESTINATION:
            ret = dev->regs.local_destination;
            break;
        case LAPIC_REG_DESTINATION_FORMAT:
            ret = dev->regs.destination_format;
            break;
        case LAPIC_REG_SPURIOUS_INT_VECTOR:
            ret = dev->regs.spurious_int_vector;
            break;

        case LAPIC_REG_ISR_0:
        case LAPIC_REG_ISR_1:
        case LAPIC_REG_ISR_2:
        case LAPIC_REG_ISR_3:
        case LAPIC_REG_ISR_4:
        case LAPIC_REG_ISR_5:
        case LAPIC_REG_ISR_6:
        case LAPIC_REG_ISR_7:
            ret = dev->regs.in_service[lapic_reg - LAPIC_REG_ISR_0];
            break;

        case LAPIC_REG_TRIGGER_MODE_0:
        case LAPIC_REG_TRIGGER_MODE_1:
        case LAPIC_REG_TRIGGER_MODE_2:
        case LAPIC_REG_TRIGGER_MODE_3:
        case LAPIC_REG_TRIGGER_MODE_4:
        case LAPIC_REG_TRIGGER_MODE_5:
        case LAPIC_REG_TRIGGER_MODE_6:
        case LAPIC_REG_TRIGGER_MODE_7:
            ret = dev->regs.trigger_mode[lapic_reg - LAPIC_REG_TRIGGER_MODE_0];
            break;

        case LAPIC_REG_IRR_0:
        case LAPIC_REG_IRR_1:
        case LAPIC_REG_IRR_2:
        case LAPIC_REG_IRR_3:
        case LAPIC_REG_IRR_4:
        case LAPIC_REG_IRR_5:
        case LAPIC_REG_IRR_6:
        case LAPIC_REG_IRR_7:
            ret = dev->regs.in_service[lapic_reg - LAPIC_REG_IRR_0];
            break;

        case LAPIC_REG_ERROR_STATUS:
            ret = dev->regs.error_status;
            break;
        case LAPIC_REG_LVT_CMCI:
            ret = dev->regs.lvt_cmci;
            break;
        case LAPIC_REG_ICR_LOW:
            ret = dev->regs.interrupt_command_low;
            break;
        case LAPIC_REG_ICR_HIGH:
            ret = dev->regs.interrupt_command_high;
            break;
        case LAPIC_REG_LVT_TIMER:
            ret = dev->regs.lvt_timer;
            break;
        case LAPIC_REG_LVT_THERMAL_SENSOR:
            ret = dev->regs.lvt_thermal_sensor;
            break;
        case LAPIC_REG_LVT_COUNTERS:
            ret = dev->regs.lvt_performance_counters;
            break;
        case LAPIC_REG_LVT_LINT0:
            ret = dev->regs.lvt_lint0;
            break;
        case LAPIC_REG_LVT_LINT1:
            ret = dev->regs.lvt_lint1;
            break;
        case LAPIC_REG_LVT_ERROR:
            ret = dev->regs.lvt_error;
            break;
        case LAPIC_REG_TIMER_INITIAL_COUNT:
            ret = dev->regs.timer_initial_count;
            break;
        case LAPIC_REG_TIMER_CURRENT_COUNT:
            ret = dev->regs.timer_current_count;
            break;
        case LAPIC_REG_TIMER_DIVIDE_CONFIG:
            ret = dev->regs.timer_divider;
            break;

        default:
            ret = LAPIC_IO_DEFAULT_VALUE;
            break;
    }

    return ret;
}

static void
lapic_mmio_write32(uint32_t addr, uint32_t val, void* priv)
{
    lapic_t *dev = priv;
    uint32_t write_bits_mask;

    assert((addr & 0x3) == 0);

    addr &= LAPIC_IO_DECODE_MASK;

    lapic_log("LAPIC: [W32] [%3lX] <-- %X\n", addr, val);

    if ((addr & LAPIC_IO_INVALID_MASK) == 0) {
        lapic_write32(REG_TO_IDX(addr), val, priv);
    }
}

static uint32_t
lapic_mmio_read32(uint32_t addr, void* priv)
{
    lapic_t *dev = priv;
    uint32_t ret;

    assert((addr & 0x3) == 0);

    addr &= LAPIC_IO_DECODE_MASK;

    if ((addr & LAPIC_IO_INVALID_MASK) == 0) {
        ret = lapic_read32(REG_TO_IDX(addr), priv);
    } else {
        ret = LAPIC_IO_DEFAULT_VALUE;
    }

    lapic_log("LAPIC: [R32] [%3lX] --> %X\n", addr, ret);
    return ret;
}

static uint8_t
lapic_mmio_read8(uint32_t addr, void* priv)
{
    uint32_t ret;
assert(0);
    ret = lapic_mmio_read32(addr, priv);
    ret >>= (addr & 3) << 3;

    return ret & 0xFF;
}

static uint16_t
lapic_mmio_read16(uint32_t addr, void* priv)
{
    uint32_t ret;
assert(0);
    ret = lapic_mmio_read32(addr, priv);
    ret >>= (addr & 3) << 3;

    return ret & 0xFFFF;
}

static void
lapic_mmio_write8(uint32_t addr, uint8_t val, void* priv)
{
    uint32_t data;
assert(0);
    data = lapic_mmio_read32(addr, priv);

    data &= ~0xFF << (addr & 3) << 3;
    data |= val << ((addr & 3) << 3);

    lapic_mmio_write32(addr, data, priv);
}

static void
lapic_mmio_write16(uint32_t addr, uint16_t val, void* priv)
{
    uint32_t data;
assert(0);
    data = lapic_mmio_read32(addr, priv);

    data &= ~0xFFFF << (addr & 3) << 3;
    data |= val << ((addr & 3) << 3);

    lapic_mmio_write32(addr, data, priv);
}

static void
lapic_reset_hard(lapic_t *dev)
{
    uint32_t lvt_entries;

    /*
     * - 5 for the P6 family processors
     * - 6 for Pentium 4 and Intel Xeon
     * - 7 for Nehalem microarchitecture
     */
    lvt_entries = 5;

    dev->regs.version = LAPIC_VER_INTEGRATED_APIC_V11 |
                        ((lvt_entries - 1) << LAPIC_VER_MAX_LVT_ENTRY_SHIFT);

    dev->regs.destination_format = 0xFFFFFFFF;
    dev->regs.spurious_int_vector = 0x1FF;
    dev->regs.interrupt_command_low = 0x80010;
    dev->regs.lvt_timer = 0x10000;
    dev->regs.lvt_performance_counters = 0x10000;
    dev->regs.lvt_lint0 = 0x10000;
    dev->regs.lvt_lint1 = 0x10000;
    dev->regs.lvt_error = 0x10000;
}

static void
lapic_close(void *priv)
{
    lapic_t *dev = priv;

    free(dev);
}

static void *
lapic_init(const device_t *devinfo)
{
    lapic_t *dev = calloc(1, sizeof(lapic_t));

    mem_mapping_add(&dev->mmio_mapping,
                    LAPIC_IO_BASE,
                    LAPIC_IO_SIZE,
                    lapic_mmio_read8,
                    lapic_mmio_read16,
                    lapic_mmio_read32,
                    lapic_mmio_write8,
                    lapic_mmio_write16,
                    lapic_mmio_write32,
                    NULL,
                    MEM_MAPPING_EXTERNAL,
                    dev);

    lapic_reset_hard(dev);
}

const device_t local_apic_device = {
    .name          = "Local Advanced Programmable Interrupt Controller",
    .internal_name = "lapic",
    .flags         = DEVICE_AT,
    .local         = 0,
    .init          = lapic_init,
    .close         = lapic_close,
    .reset         = NULL,
    { .available = NULL },
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
