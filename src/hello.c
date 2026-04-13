// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#define DEMO_BUF_SIZE 128

static void *kbuf;
static void *vbuf;

static int __init hello_init(void)
{
	pr_info("Hello, world!\n");

	kbuf = kmalloc(DEMO_BUF_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	vbuf = vmalloc(DEMO_BUF_SIZE);
	if (!vbuf) {
		kfree(kbuf);
		return -ENOMEM;
	}

	pr_info("hello: kmalloc buffer at %px\n", kbuf);
	pr_info("hello: vmalloc buffer at %px\n", vbuf);

	return 0;
}

static void __exit hello_exit(void)
{
	vfree(vbuf);
	kfree(kbuf);
	pr_info("Goodbye, world!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nuriel");
MODULE_DESCRIPTION("Hello world kernel module with kmalloc/vmalloc demo");
