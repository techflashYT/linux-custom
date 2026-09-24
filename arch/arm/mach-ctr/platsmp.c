// SPDX-License-Identifier: GPL-2.0-only
/*
 * SMP support for the Nintendo 3DS
 *
 * Copyright (C) 2016 Sergi Granell
 * Copyright (C) 2021 Santiago Herrera
 * Copyright (C) 2021 Nick Desaulniers
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>

#include <asm/smp_plat.h>
#include <asm/smp_scu.h>

#include <mach/platsmp.h>

/*
 * CPUn waits for event
 * [0x1FFFFFF0+ n*4] is where it expects the entrypoint.
 */

#define SECONDARY_STARTUP_ADDR(n)	(0x1FFFFFF0 + ((n)*4))
#define SCU_INVALIDATE			0x0c
#define GIC_SGI_CONFIG_NN		0xaaaaaaaa

static void __iomem *scu_base;
static void __iomem *gic_dist_base;

static void ctr_gic_sgi_nn(unsigned int cpu)
{
	u32 cfg = readl_relaxed(gic_dist_base + GIC_DIST_CONFIG);

	if (cfg != GIC_SGI_CONFIG_NN) {
		writel_relaxed(GIC_SGI_CONFIG_NN,
			       gic_dist_base + GIC_DIST_CONFIG);
	}
}

static int ctr_smp_boot_secondary(unsigned int cpu,
				    struct task_struct *idle)
{
	void __iomem *boot_addr;

	if (scu_base)
		writel_relaxed(0xf << (4 * MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0)),
			       scu_base + SCU_INVALIDATE);

	boot_addr = ioremap((phys_addr_t)SECONDARY_STARTUP_ADDR(cpu),
			       sizeof(phys_addr_t));

	/* Set CPU boot address */
	writel(virt_to_phys(ctr_secondary_startup),
		boot_addr);

	iounmap(boot_addr);

	/* Trigger event */
	sev();
	return 0;
}

static void __init ctr_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *np;
	unsigned int i, ncores;

	np = of_find_compatible_node(NULL, NULL, "arm,arm11mp-scu");
	if (np) {
		scu_base = of_iomap(np, 0);
		scu_enable(scu_base);
		ncores = scu_get_core_count(scu_base);
		for (i = 0; i != ncores; ++i)
			set_cpu_possible(i, true);
		of_node_put(np);
	}

	np = of_find_compatible_node(NULL, NULL, "arm,arm11mp-gic");
	if (np) {
		gic_dist_base = of_iomap(np, 0);
		of_node_put(np);
	}

	if (gic_dist_base)
		ctr_gic_sgi_nn(smp_processor_id());
}

static void ctr_smp_secondary_init(unsigned int cpu)
{
	if (gic_dist_base)
		ctr_gic_sgi_nn(cpu);
}

static const struct smp_operations ctr_smp_ops __initconst = {
	.smp_prepare_cpus	= ctr_smp_prepare_cpus,
	.smp_secondary_init	= ctr_smp_secondary_init,
	.smp_boot_secondary	= ctr_smp_boot_secondary,
};
CPU_METHOD_OF_DECLARE(ctr_smp, "nintendo,3ds-smp", &ctr_smp_ops);
