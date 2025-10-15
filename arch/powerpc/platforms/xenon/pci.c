/*
 * Xenon PCI support
 * Maintained by: Felix Domke <tmbinc@elitedvb.net>
 * Minor modification by: wolie <wolie@telia.com>
 * based on:
 * Copyright (C) 2004 Benjamin Herrenschmuidt (benh@kernel.crashing.org),
 *		      IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/pci-ecam.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include <asm/sections.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/iommu.h>
#include <asm/ppc-pci.h>

#include <asm/dma-mapping.h>

#include "pci.h"

/* The Xenon's PCI controller supports modern ECAM PCIe configuration mechanism.
 * Controllers like this are supposed to also support the legacy mechanism, so PPC Linux uses it,
 * but if this one does, we don't know where it's mapped.
 * Rough port of pci-ecam.c to confirm to the PowerPC PCI system.
 */

#define XENON_ECAM_MAXBUS 15

static void __iomem *xenon_pci_ecam_map_bus(struct pci_bus *bus,
					    unsigned int devfn, int where)
{
	unsigned int busn = bus->number;
	unsigned int slot = PCI_SLOT(devfn);
	unsigned int func = PCI_FUNC(devfn);

	struct pci_controller *hose = pci_bus_to_host(bus);
	if (hose == NULL)
		return NULL;

	if (busn < 0 || busn > 16)
		return NULL;

	// HACK to remap the GPU onto bus 1. Should remove once Bus 0 is working.
	if (busn == 1 && slot == 15) {
		busn = 0;
		slot = 2;
	}

	return ((void __iomem *)hose->cfg_addr) +
	       PCIE_ECAM_OFFSET(busn, PCI_DEVFN(slot, func), where);
}

static struct pci_ops xenon_pci_ops = {
	.map_bus = xenon_pci_ecam_map_bus,
	.read = pci_generic_config_read,
	.write = pci_generic_config_write,
};

static int __init xenon_pci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node;

	struct pci_controller *hose;
	struct resource cfg_range;
	u32 bus_range[2];
	int err;

	hose = pcibios_alloc_controller(dn);
	if (hose == NULL) {
		printk("pcibios_alloc_controller failed!\n");
		return -ENOMEM;
	}
	hose->parent = &pdev->dev;

	err = of_property_read_u32_array(dn, "bus-range", bus_range,
					 ARRAY_SIZE(bus_range));
	if (err) {
		bus_range[0] = 0;
		bus_range[1] = XENON_ECAM_MAXBUS;
	}

	/*
	 * TODO: Bus 0 has a P2P bridge on it that appears to run bus 1 for us. Unfortunately,
	 * I can't get Linux resource allocation to work with it.. It's outside my wheelhouse.
	 * For now, treat Bus 1 as the root and hack the GPU onto bus 1.
	 */
	hose->first_busno = 1; //bus_range[0];
	hose->last_busno = XENON_ECAM_MAXBUS; //bus_range[1];

	// todo this should really come from the DT "reg" or "cfg"
	cfg_range.name = "PCI ECAM";
	cfg_range.start = 0xd0000000;
	cfg_range.end = cfg_range.start + PCIE_ECAM_BUS(hose->last_busno + 1);
	cfg_range.flags = IORESOURCE_MEM;

	hose->cfg_addr = devm_ioremap_resource(dev, &cfg_range);
	if (IS_ERR(hose->cfg_addr)) {
		dev_err(dev, "Failed to map ECAM - %ld\n",
			PTR_ERR(hose->cfg_addr));
		return PTR_ERR(hose->cfg_addr);
	}
	hose->ops = &xenon_pci_ops;

	pci_process_bridge_OF_ranges(hose, dn, 1);

	dev_info(dev, "PCI initialized\n");
	return 0;
}

void __init xenon_pci_init(void)
{
	int found = 0;
	struct device_node *dn;
	struct platform_device *pdev;

	for_each_compatible_node(dn, "pci", "xenon") {
		pdev = of_platform_device_create(dn, "xenon-pci", NULL);
		xenon_pci_probe(pdev);
		found++;
	}
	if (!found) {
		pr_warn("No PCI found!\n");
		return;
	}

	pr_info("Found %d instances\n", found);

	return;
}
