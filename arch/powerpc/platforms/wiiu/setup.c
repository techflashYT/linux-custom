// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Nintendo Wii U board-specific support
 *
 * Copyright (C) 2023 The linux-wiiu Team
 */
#define DRV_MODULE_NAME "wiiu"
#define pr_fmt(fmt) DRV_MODULE_NAME ": " fmt

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm.h>

#include <asm/machdep.h>
#include <asm/time.h>
#include <asm/udbg.h>
#include <asm/interrupt.h>

#include "espresso-pic.h"
#include "latte-pic.h"
#include "latte-ipc.h"

#define WIIU_LOADER_CMD_POWEROFF 0xCAFE0001
#define WIIU_LOADER_CMD_REBOOT   0xCAFE0002

static void __noreturn wiiu_halt(void)
{
	for (;;)
		cpu_relax();
}

static void __noreturn wiiu_power_off(void)
{
	latteipc_starbuck_msg(WIIU_LOADER_CMD_POWEROFF);
	wiiu_halt();
}

static void __noreturn wiiu_restart(char *cmd)
{
	latteipc_starbuck_msg(WIIU_LOADER_CMD_REBOOT);
	wiiu_halt();
}

static int __init wiiu_probe(void)
{
	if (!of_machine_is_compatible("nintendo,wiiu"))
		return 0;

	pm_power_off = wiiu_power_off;

	return 1;
}

static void __init wiiu_init_irq(void)
{
	espresso_pic_init();
	latte_pic_init();
}

static int __init wiiu_device_probe(void)
{
	if (!machine_is(wiiu))
		return 0;

	of_platform_default_populate(NULL, NULL, NULL);
	return 0;
}
device_initcall(wiiu_device_probe);

#ifdef CONFIG_SMP
static void __init wiiu_smp_probe(void)
{
	// ..?
}

static void wiiu_do_exi_bootstub(unsigned long entry)
{
	void __iomem *exi_iob;
	int i;

	exi_iob = ioremap(0x08100100, 16);
	if (!exi_iob)
	{
		pr_err("%s: couldn't map EXI!\n", __func__);
		return;
	}

	//printk("value of...")

	// lis r3, entry@h
	iowrite32be(0x3c600000 | entry >> 16, exi_iob + 4 * 0);
	// ori r3, r3, entry@l
	iowrite32be(0x60630000 | (entry & 0xffff), exi_iob + 4 * 1);
	// mtsrr0 r3
	iowrite32be(0x7c7a03a6, exi_iob + 4 * 2);
	// li r3, 0
	iowrite32be(0x38600000, exi_iob + 4 * 3);
	// mtsrr1 r3
	iowrite32be(0x7c7b03a6, exi_iob + 4 * 4);
	// rfi
	iowrite32be(0x4c000064, exi_iob + 4 * 5);

	for (i = 6; i < 0x10; ++i)
		iowrite32be(0, exi_iob + 4 * i);
	printk("%s: EXI bootstub setup OK!\n", __func__);
}

extern void __secondary_start_wiiu(void); // head_book3s_32.S

static int wiiu_kick_cpu(int nr)
{
	int n;

	wiiu_do_exi_bootstub(virt_to_phys(&__secondary_start_wiiu));
	printk("%s: starting CPU %d\n", __func__, nr);

	if (nr == 1 || nr == 2)
		mtspr(SPRN_SCR_ESPRESSO, mfspr(SPRN_SCR_ESPRESSO) | (1 << (23 - nr)));
	else
		return -ENOENT;

	/* Wait for the CPU to come up */
	for (n = 0; n < 1000; n++) {
		dcbi(&__secondary_hold_acknowledge);
		if (__secondary_hold_acknowledge == nr)
			break;

		udelay(1000);
	}

	if (__secondary_hold_acknowledge == nr)
		printk("%s: cpu %d ok\n", __func__, nr);
	else
		printk("%s: cpu %d gave invalid ack: %08lx\n", __func__, nr, __secondary_hold_acknowledge);

	return 0;
}

static void wiiu_ipi_cpu(int cpu)
{
	unsigned long scr, mask = 1 << (20 - cpu);

	if ((scr = mfspr(SPRN_SCR_ESPRESSO)), scr & mask)
		printk("%s: cpu %d already had ipi pending? %08lx\n", __func__, cpu, scr);

	while ((scr = mfspr(SPRN_SCR_ESPRESSO)), !(scr & mask))
		mtspr(SPRN_SCR_ESPRESSO, scr | mask);
}

struct smp_ops_t espresso_smp_ops = {
	.probe		= wiiu_smp_probe,
	.message_pass	= NULL, /* Use smp_muxed_ipi_message_pass */
	.cause_ipi	= wiiu_ipi_cpu,
	.cause_nmi_ipi	= NULL,
	.kick_cpu	= wiiu_kick_cpu,
};

#endif // CONFIG_SMP

DEFINE_INTERRUPT_HANDLER_ASYNC(TAUException)
{
	unsigned int cpu = smp_processor_id();
	unsigned long scr, mask = 1 << (20 - cpu);

	// ack the irq
	while ((scr = mfspr(SPRN_SCR_ESPRESSO)), scr & mask)
		mtspr(SPRN_SCR_ESPRESSO, scr & ~mask);

#ifdef CONFIG_SMP
	smp_ipi_demux();
#endif
}

static void __init wiiu_setup_arch(void)
{
#ifdef CONFIG_SMP
	smp_ops = &espresso_smp_ops;
#endif
}

define_machine(wiiu) {
	.name		= "wiiu",
	.probe		= wiiu_probe,
	.setup_arch	= wiiu_setup_arch,
	.halt		= wiiu_halt,
	.progress	= udbg_progress,
	.calibrate_decr	= generic_calibrate_decr,
	.init_IRQ	= wiiu_init_irq,
	.get_irq	= espresso_pic_get_irq,
	.restart	= wiiu_restart,
};
