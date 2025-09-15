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

static void xlnx_mailbox_irq_handler(const void* dat)
{
	struct device* dev = (struct device*)dat;
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config *)(dev->config);
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	struct xlnx_mailbox_data* data = (struct xlnx_mailbox_data*)(dev->data);
	LOG_INF("Interrupt invoked");
	size_t size = 0;
	while (!(regs->status & 1))
	{
		LOG_INF("Received data %u", regs->rddata);
		data->rx_buff[size] = regs->rddata;
		size++;
	}
	if (data->callback != NULL)
	{
		struct mbox_msg msg =
		{
			.data = data->rx_buff,
			.size = size,
		};
		(data->callback)(dev, 0, data->userdata, &msg);
	}
	regs->is = 0x2;
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

	if (!(regs->status & (1 << 2)))
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
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config*)(dev->config);
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	if (!(regs->status & (1 << 2)))
	{
		return 0;
	}
	else
	{
		return config->fifo_depth / 2;
	}
}

static int xlnx_mailbox_register_callback(const struct device* dev, mbox_channel_id_t channel_id,
	mbox_callback_t cb, void* user_data)
{
	if (channel_id != 0)
	{
		LOG_ERR("There is only one channel (channel 0)");
		return -EINVAL;
	}

	struct xlnx_mailbox_data* data = (struct xlnx_mailbox_data*)(dev->data);
	if (cb == NULL)
	{
		LOG_ERR("Callback cannot be null");
		return -EINVAL;
	}

	data->callback = cb;
	data->userdata = user_data;

	return 0;
}

static int xlnx_mailbox_set_enabled(const struct device* dev,
	mbox_channel_id_t channel_id, bool enabled)
{
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config*)(dev->config);
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	if(enabled)
	{
		// Enable send and receive interrupt threshold
		regs->ie |= STI_MASK | RTI_MASK;
	}
	else
	{
		// Disable send and receive interrupt threshold
		regs->ie &= ~(STI_MASK | RTI_MASK);
	}
	return 0;
}

static uint32_t xlnx_mailbox_max_channels_get(const struct device* dev)
{
	return 1;
}

#define xlnx_mailbox_register_irq(inst)				\
static void xlnx_mailbox_register_irq_##inst(void)	\
{													\
	IRQ_CONNECT(DT_INST_IRQN(inst),					\
		DT_INST_IRQ(inst, priority),				\
		xlnx_mailbox_irq_handler,					\
		DEVICE_DT_INST_GET(inst),					\
		MAILBOX_IRQ_FLAGS);							\
	irq_enable(DT_INST_IRQN(inst));					\
}													\

static int xlnx_mailbox_init(const struct device* dev)
{
	struct xlnx_mailbox_config* config = (struct xlnx_mailbox_config*)dev->config;
	struct xlnx_mailbox_regs* regs = (struct xlnx_mailbox_regs*)(config->base);
	if (config->irq_connect != NULL)
	{
		config->irq_connect();
		// Enable send and receive interrupt threshold
		regs->ie |= STI_MASK | RTI_MASK;
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
static uint32_t xlnx_mailbox_rx_buff_##inst[DT_INST_PROP(inst, fifo_depth)]; \
struct xlnx_mailbox_data xlnx_mailbox_##inst##_data =			\
{																\
	.rx_buff = xlnx_mailbox_rx_buff_##inst,						\
};																\
DEVICE_DT_INST_DEFINE(inst, 									\
			xlnx_mailbox_init, 									\
			NULL,												\
			&xlnx_mailbox_##inst##_data, 						\
			&xlnx_mailbox_##inst##_config, 						\
			POST_KERNEL, 										\
			CONFIG_MBOX_INIT_PRIORITY, 							\
			&xlnx_mailbox_api); 								\

DT_INST_FOREACH_STATUS_OKAY(XLNX_MAILBOX_DEFINE)

