/*
 * Copyright (c) 2025 Dang Cao Minh <dangcaominhheo@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/drivers/mbox/mbox_xlnx.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>


#define DT_DRV_COMPAT xlnx_mailbox

LOG_MODULE_REGISTER(xlnx_mailbox, CONFIG_MBOX_LOG_LEVEL);

static int xlnx_mailbox_send(const struct device* dev, mbox_channel_id_t channel_id, const struct mbox_msg* msg)
{
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config *)(dev->config);
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	LOG_INF("Go in here and base is %x", config->base);
	if (channel_id != 0)
	{
		LOG_ERR("There is only one channel (channel 0)");
		return -EINVAL;
	}

	uint32_t* data = (uint32_t*)msg->data;

	for (size_t i = 1; i <= msg->size; i++)
	{
		if (regs->status & (1 << 1))
		{
			LOG_ERR("Mailbox is full");
			return -ENOMEM;
		}
		regs->wrdata = *data;
		data++;
	}

	return 0;
}

static int xlnx_mailbox_mtu_get(const struct device* dev)
{
	return 0;
}

static int xlnx_mailbox_register_callback(const struct device* dev, mbox_channel_id_t channel_id,
	mbox_callback_t cb, void* user_data)
{
	return 0;
}

static int xlnx_mailbox_set_enabled(const struct device* dev,
	mbox_channel_id_t channel_id, bool enabled)
{
	return 0;
}

static uint32_t xlnx_mailbox_max_channels_get(const struct device* dev)
{
	return 0;
}

static int xlnx_mailbox_init(const struct device* dev)
{
	return 0;
}

struct mbox_driver_api xlnx_mailbox_api = {
	.send = xlnx_mailbox_send,
	.register_callback = xlnx_mailbox_register_callback,
	.mtu_get = xlnx_mailbox_mtu_get,
	.max_channels_get = xlnx_mailbox_max_channels_get,
	.set_enabled = xlnx_mailbox_set_enabled,
};


#define XLNX_MAILBOX_DEFINE(inst)								\
struct xlnx_mailbox_config xlnx_mailbox_##inst##_config =		\
{																\
	.base = DT_INST_REG_ADDR_BY_IDX(inst, 0),					\
	.fifo_depth = 16,											\
};																\
DEVICE_DT_INST_DEFINE(inst, 									\
			xlnx_mailbox_init, 									\
			NULL,												\
			NULL, 												\
			&xlnx_mailbox_##inst##_config, 						\
			POST_KERNEL, 										\
			CONFIG_MBOX_INIT_PRIORITY, 							\
			&xlnx_mailbox_api); 								\

DT_INST_FOREACH_STATUS_OKAY(XLNX_MAILBOX_DEFINE)

