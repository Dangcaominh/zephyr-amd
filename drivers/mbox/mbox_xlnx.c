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
#include <zephyr/spinlock.h>


#define DT_DRV_COMPAT xlnx_mailbox

LOG_MODULE_REGISTER(xlnx_mailbox, CONFIG_MBOX_LOG_LEVEL);

static void xlnx_mailbox_irq_handler(const void* data)
{
	struct device* dev = (struct device*)data;
	LOG_INF("Interrupt invoked");
}

static int xlnx_mailbox_send(const struct device* dev, mbox_channel_id_t channel_id, const struct mbox_msg* msg)
{
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config *)(dev->config);
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	if (channel_id != 0)
	{
		LOG_ERR("There is only one channel (channel 0)");
		return -EINVAL;
	}

	if (regs->status & (1 << 3))
	{
		LOG_ERR("Mailbox has reached interrupt threshold");
		return -EAGAIN;
	}

	if (msg->size * 2 >= config->fifo_depth)
	{
		LOG_ERR("Message size cannot exceed half the fifo depth buffer of size %u", config->fifo_depth);
		return -EMSGSIZE;
	}
	
	uint32_t* data = (uint32_t*)msg->data;
	k_spinlock_key_t key = k_spin_lock(&config->lock);
	for (size_t i = 1; i <= msg->size; i++)
	{
		if (regs->status & (1 << 1))
		{
			LOG_ERR("Mailbox is full");
			k_spin_unlock(&config->lock, key);
			return -ENOMEM;
		}
		regs->wrdata = *data;
		data++;
	}
	k_spin_unlock(&config->lock, key);

	return 0;
}

static int xlnx_mailbox_mtu_get(const struct device* dev)
{

	return 0;
}

static int xlnx_mailbox_register_callback(const struct device* dev, mbox_channel_id_t channel_id,
	mbox_callback_t cb, void* user_data)
{
	if (channel_id != 0)
	{
		LOG_ERR("There is only one channel (channel 0)");
		return -EINVAL;
	}
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

#define xlnx_mailbox_register_irq(inst)				\
static void xlnx_mailbox_register_irq_##inst(void)	\
{													\
	IRQ_CONNECT(DT_IRQ(DT_DRV_INST(inst), irq),		\
		DT_IRQ(DT_DRV_INST(inst), priority),		\
		xlnx_mailbox_irq_handler,					\
		DEVICE_DT_INST_GET(inst),					\
		MAILBOX_IRQ_FLAGS);							\
	irq_enable(DT_IRQ(DT_DRV_INST(inst), irq));		\
}													\

static int xlnx_mailbox_init(const struct device* dev)
{
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config*)dev->config;
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	if (config->irq_connect != NULL)
	{
		config->irq_connect();
		// Enable send and receive interrupt threshold
		regs->ie |= (1 << 0) | (1 << 1);
		regs->sit = config->fifo_depth / 2;
		regs->rit = config->fifo_depth / 2;
	}

	LOG_INF("Mailbox driver initialized");
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
xlnx_mailbox_register_irq(inst)									\
struct xlnx_mailbox_config xlnx_mailbox_##inst##_config =		\
{																\
	.base = DT_INST_REG_ADDR_BY_IDX(inst, 0),					\
	.fifo_depth = 16,											\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, interrupts),        \
		(.irq_connect = xlnx_mailbox_register_irq_##inst,),     \
		(.irq_connect = NULL))                                  \
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

