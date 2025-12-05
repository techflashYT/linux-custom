// SPDX-License-Identifier: GPL-2.0-only
/*
 * LEDs driver for Xbox 360 (Xenon) Ring of Light
 *
 * Copyright (C) 2025 Techflash <officialTechflashYT@gmail.com>
 */

#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <xenon/smc-core.h>

#define DRVNAME "leds-xenon"

/*
 * There's no way to to get the current LED status
 * from the Xenon SMC, so we will default to having
 * them all off and keeping track of it manually here.
 */
struct xenon_led {
	struct led_classdev cdev;
	int id;
};
static unsigned char led_state = 0;
static const char *led_positions[4] = {
	"top-left",
	"top-right",
	"bottom-left",
	"bottom-right"
};
static struct xenon_led leds[8];

/*
 * since our led_state is just a byte (as the SMC expects),
 * we need to keep a lock on it so that it can't get mangled
 * by multiple CPUs (since Xenon is an SMP platform).
 */
static DEFINE_SPINLOCK(led_lock);


static void xenon_led_set(struct led_classdev *cdev,
			  enum led_brightness brightness)
{
	struct xenon_led *led = container_of(cdev, struct xenon_led, cdev);
	unsigned char smc_msg[16];
	unsigned char led_state_old = led_state;
	unsigned long flags = 0;

	/* lock it so we can't mangle the led_state if we get here twice */
	spin_lock_irqsave(&led_lock, flags);

	/* keep track of this change */
	if (brightness)
		led_state |= (1 << led->id);
	else
		led_state &= ~(1 << led->id);
	pr_debug("xenon_led_set: brightness=%d, id=%d, led_state_old=0x%02X, led_state_new=0x%02X\n", brightness, led->id, led_state_old, led_state);

	memset(smc_msg, 0, 16);
	smc_msg[0] = 0x99; /* set LEDs */
	smc_msg[1] = 0x01; /* override */
	smc_msg[2] = led_state;

	/* we've completed our use of led_state, release the lock */
	spin_unlock_irqrestore(&led_lock, flags);

	/* apply it */
	xenon_smc_message_wait(smc_msg);
}

static enum led_brightness xenon_led_get(struct led_classdev *cdev)
{
	struct xenon_led *led = container_of(cdev, struct xenon_led, cdev);
	if (led_state & (1 << led->id)) {
		pr_debug("xenon_led_get: id=%d, led_state=0x%02X, ret=1\n", led->id, led_state);
		return 1;
	}
	else {
		pr_debug("xenon_led_get: id=%d, led_state=0x%02X, ret=0\n", led->id, led_state);
		return 0;
	}
}

static int xenon_led_probe(struct platform_device *pdev)
{
	int ret, i;
	char *color;

	/* we really need the SMC to be ready */
	if (!xenon_smc_ready())
		return -EPROBE_DEFER;

	for (i = 0; i < 8; i++) {
		if (i < 4)
			color = "red";
		else
			color = "green";

		leds[i].id = i;
		leds[i].cdev.name = kasprintf(GFP_KERNEL, "xenon:%s:%s", color, led_positions[i % 4]);
		leds[i].cdev.max_brightness = 1;
		leds[i].cdev.brightness_get = xenon_led_get;
		leds[i].cdev.brightness_set = xenon_led_set;

		/*
		 * actually apply it so that the state on the real
		 * Ring of Light is what we're saying it is
		 */
		xenon_led_set(&leds[i].cdev, 0);

		/* register */
		ret = led_classdev_register(&pdev->dev, &leds[i].cdev);
		if (ret)
			return ret;
	}
	return 0;
}

static void xenon_led_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		led_classdev_unregister(&leds[i].cdev);
		kfree(leds[i].cdev.name);
	}
}

static struct platform_driver xenon_led_driver = {
	.probe = xenon_led_probe,
	.remove = xenon_led_remove,
	.driver = {
		.name = DRVNAME,
	},
};

module_platform_driver(xenon_led_driver);

static int __init xenon_led_init(void)
{
	int ret;
	struct platform_device *pdev;

	pdev = platform_device_register_simple(DRVNAME, -1, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		platform_driver_unregister(&xenon_led_driver);
		goto out;
	}


	pr_info("Xenon Ring of Light LED driver loaded");
out:
	return ret;
}

static void __exit xenon_led_exit(void)
{
	platform_driver_unregister(&xenon_led_driver);
}

module_init(xenon_led_init);
module_exit(xenon_led_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ring of Light LED support for the Xenon Game Console");
MODULE_AUTHOR("Techflash <officialTechflashYT@gmail.com>");
MODULE_SOFTDEP("pre: xenon_smc_core");
