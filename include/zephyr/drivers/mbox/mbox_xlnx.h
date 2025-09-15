/*
 * Copyright (c) 2025 Dang Cao Minh <dangcaominhheo@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

#define     STI_MASK    BIT(0)
#define     RTI_MASK    BIT(1)

typedef struct xlnx_mailbox_regs
{
    uint32_t wrdata;     // 0x00 : Write Data (WO)
    uint32_t reserved0;  // 0x04 : Reserved
    uint32_t rddata;     // 0x08 : Read Data (RO)
    uint32_t reserved1;  // 0x0C : Reserved
    uint32_t status;     // 0x10 : Status (RO)
    uint32_t error;      // 0x14 : Error (RO, clear on read)
    uint32_t sit;        // 0x18 : Send Interrupt Threshold (RW)
    uint32_t rit;        // 0x1C : Receive Interrupt Threshold (RW)
    uint32_t is;         // 0x20 : Interrupt Status (RW)
    uint32_t ie;         // 0x24 : Interrupt Enable (RW)
    uint32_t ip;         // 0x28 : Interrupt Pending (RO)
} xlnx_mailbox_regs;

struct xlnx_mailbox_config
{
	struct xlnx_mailbox_regs* base;
	struct k_spinlock lock;
    uint32_t fifo_depth;
    void (*irq_connect)(void);

};

struct xlnx_mailbox_data
{
    mbox_callback_t callback;
    void* userdata;
    uint32_t* rx_buff;
};

#define MAILBOX_IRQ_FLAGS 0