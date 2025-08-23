/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/devicetree.h"
#include <stdio.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

void print()
{
	printk("Hello World\n");
}

int main(void)
{
	// struct device* dev = DEVICE_DT_GET(DT_ALIAS(serial1));
	uint32_t *p = (uint32_t *)(0x20000000);
	while (1)
	{
		*p = 0;
		k_sleep(K_MSEC(500));
		*p = 0xf;
		k_sleep(K_MSEC(500));
	}

	return 0;
}

SHELL_CMD_REGISTER(print, NULL, "Print hello world", print);
