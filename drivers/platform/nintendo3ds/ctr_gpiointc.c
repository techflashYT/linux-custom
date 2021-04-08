// SPDX-License-Identifier: GPL-2.0
/*
 *  ctr_gpiointc.c
 *
 *  Copyright (C) 2021 Santiago Herrera
 */

#define DRIVER_NAME "3ds-gpiointc"
#define pr_fmt(fmt)	DRIVER_NAME ": " fmt

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/irqchip/chained_irq.h>

struct ctr_gpiointc {
	struct device *dev;

	struct irq_domain *irqdom;
	struct irq_chip_generic *irqgc;

	struct gpio_desc *edge_gpio;
	struct gpio_desc *en_gpio;
};

static void ctr_gpiointc_irq_toggle(struct irq_data *d, int enable)
{
	struct irq_chip_generic *irqgc = irq_data_get_irq_chip_data(d);
	struct ctr_gpiointc *intc = irqgc->private;

	irq_gc_lock(irqgc);
	gpiod_set_value(intc->en_gpio, enable);
	irq_gc_unlock(irqgc);
}

static void ctr_gpiointc_irq_mask(struct irq_data *d)
{
	ctr_gpiointc_irq_toggle(d, 0);
}

static void ctr_gpiointc_irq_unmask(struct irq_data *d)
{
	ctr_gpiointc_irq_toggle(d, 1);
}

static int ctr_gpiointc_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct irq_chip_generic *irqgc = irq_data_get_irq_chip_data(d);
	struct ctr_gpiointc *intc = irqgc->private;

	irq_gc_lock(irqgc);
	gpiod_set_value(intc->edge_gpio, !!(type == IRQ_TYPE_EDGE_RISING));
	irq_gc_unlock(irqgc);
	return 0;
}

static irqreturn_t ctr_gpiointc_irq(int irq, void *data)
{
	struct ctr_gpiointc *intc = data;
	generic_handle_irq(irq_find_mapping(intc->irqdom, 0));
	return IRQ_HANDLED;
}

static int ctr_gpiointc_xlate(struct irq_domain *h, struct device_node *node,
				const u32 *intspec, u32 intsize, irq_hw_number_t *out_hwirq,
				u32 *out_type)
{
	if (node != irq_domain_get_of_node(h))
		return -ENODEV;

	if (intsize != 1)
		return -EINVAL;

	switch(intspec[0]) {
	case IRQ_TYPE_EDGE_RISING:
	case IRQ_TYPE_EDGE_FALLING:
		*out_type = intspec[0];
		break;

	default:
		return -EINVAL;
	}

	/* single hwirq per muxer */
	*out_hwirq = 0;
	return 0;
}

static const struct irq_domain_ops ctr_gpiointc_irq_domain_ops = {
	.xlate = ctr_gpiointc_xlate,
};

static int ctr_gpiointc_domap(struct platform_device *pdev,
							struct ctr_gpiointc *intc)
{
	int irq;
	struct gpio_desc *in_gpio;
	struct device *dev = &pdev->dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/* this only needs to be set to input and retain ownership */
	in_gpio = devm_gpiod_get(dev, "input", GPIOD_IN);
	if (IS_ERR(in_gpio))
		return PTR_ERR(in_gpio);

	intc->edge_gpio = devm_gpiod_get(dev, "edge", GPIOD_OUT_LOW);
	if (IS_ERR(intc->edge_gpio))
		return PTR_ERR(intc->edge_gpio);

	intc->en_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(intc->en_gpio))
		return PTR_ERR(intc->en_gpio);

	return devm_request_irq(dev, irq,
		ctr_gpiointc_irq, 0, dev_name(dev), intc);
}

static int ctr_gpiointc_initirq(struct device *dev, struct ctr_gpiointc *intc)
{
	int err;
	int irq_base;
	struct irq_chip_type *ct;

	irq_base = devm_irq_alloc_descs(dev, -1, 0, 1, -1);
	if (irq_base < 0)
		return irq_base;

	intc->irqgc = devm_irq_alloc_generic_chip(dev, dev_name(dev),
		1, irq_base, NULL, handle_simple_irq);
	if (!intc->irqgc)
		return -ENOMEM;

	intc->irqgc->private = intc;
	ct = intc->irqgc->chip_types;
	ct->type = IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING;
	ct->chip.irq_mask = ctr_gpiointc_irq_mask;
	ct->chip.irq_unmask = ctr_gpiointc_irq_unmask;
	ct->chip.irq_set_type = ctr_gpiointc_irq_set_type;

	err = devm_irq_setup_generic_chip(dev, intc->irqgc, IRQ_MSK(1),
			0, IRQ_NOREQUEST, IRQ_NOPROBE);
	if (err < 0)
		return err;

	intc->irqdom = irq_domain_add_simple(dev->of_node, 1,
		irq_base, &ctr_gpiointc_irq_domain_ops, intc);
	if (!intc->irqdom)
		return -ENODEV;

	return 0;
}

static int ctr_gpiointc_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev;
	struct ctr_gpiointc *intc;

	dev = &pdev->dev;

	intc = devm_kzalloc(dev, sizeof(*intc), GFP_KERNEL);
	if (!intc)
		return -ENOMEM;

	intc->dev = dev;
	err = ctr_gpiointc_initirq(dev, intc);
	if (err)
		return err;

	err = ctr_gpiointc_domap(pdev, intc);
	if (err)
		return err;

	return 0;
}

static const struct of_device_id ctr_gpiointc_of_match[] = {
	{ .compatible = "nintendo," DRIVER_NAME },
	{ },
};
MODULE_DEVICE_TABLE(of, ctr_gpiointc_of_match);

static struct platform_driver ctr_gpiointc_driver = {
	.probe		= ctr_gpiointc_probe,

	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ctr_gpiointc_of_match),
	},
};

module_platform_driver(ctr_gpiointc_driver);

MODULE_DESCRIPTION("Nintendo 3DS GPIO IRQ controller");
MODULE_AUTHOR("Santiago Herrera");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
