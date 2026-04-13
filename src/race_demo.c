// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>

#define RACE_LIMIT 100000

static unsigned long shared_counter;

static struct task_struct *racer_a;
static struct task_struct *racer_b;

static int racer_fn(void *data)
{
	const char *name = data;
	unsigned long i;

	for (i = 0; i < RACE_LIMIT; i++) {
		if (kthread_should_stop())
			break;
		shared_counter++;
	}

	pr_info("race_demo: thread %s finished, observed counter = %lu\n", name,
		shared_counter);

	while (!kthread_should_stop())
		msleep_interruptible(100);

	return 0;
}

static int __init race_demo_init(void)
{
	shared_counter = 0;

	racer_a = kthread_run(racer_fn, "A", "race_demo_a");
	if (IS_ERR(racer_a))
		return PTR_ERR(racer_a);

	racer_b = kthread_run(racer_fn, "B", "race_demo_b");
	if (IS_ERR(racer_b)) {
		kthread_stop(racer_a);
		return PTR_ERR(racer_b);
	}

	pr_info("race_demo: loaded, limit per thread = %d\n", RACE_LIMIT);
	return 0;
}

static void __exit race_demo_exit(void)
{
	kthread_stop(racer_a);
	kthread_stop(racer_b);
	pr_info("race_demo: final counter = %lu (expected %d)\n",
		shared_counter, 2 * RACE_LIMIT);
	pr_info("race_demo: unloaded\n");
}

module_init(race_demo_init);
module_exit(race_demo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Demonstrates a data race on a shared counter");
