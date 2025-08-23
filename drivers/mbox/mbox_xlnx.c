/*
 * Copyright (c) 2025 Dang Cao Minh <dangcaominhheo@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(xlnx_mailbox, CONFIG_MBOX_LOG_LEVEL);

#define DT_DRV_COMPAT xlnx_mailbox