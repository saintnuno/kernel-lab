// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/smp.h>

static DEFINE_PER_CPU(unsigned long, demo_counter);

static struct task_struct *percpu_thread_a;
static struct task_struct *percpu_thread_b;

static int percpu_fn(void *data)
{
	const char *name = data;

	while (!kthread_should_stop()) {
		preempt_disable();
		this_cpu_inc(demo_counter);
		pr_info("percpu_demo: thread %s on cpu=%d count=%lu\n", name,
			smp_processor_id(), this_cpu_read(demo_counter));
		preempt_enable();

		if (msleep_interruptible(1000) != 0)
			break;
	}
	return 0;
}

static int __init percpu_demo_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) per_cpu(demo_counter, cpu) = 0;

	percpu_thread_a = kthread_run(percpu_fn, "A", "percpu_demo_a");
	if (IS_ERR(percpu_thread_a))
		return PTR_ERR(percpu_thread_a);

	percpu_thread_b = kthread_run(percpu_fn, "B", "percpu_demo_b");
	if (IS_ERR(percpu_thread_b)) {
		kthread_stop(percpu_thread_a);
		return PTR_ERR(percpu_thread_b);
	}

	pr_info("percpu_demo: loaded\n");
	return 0;
}

static void __exit percpu_demo_exit(void)
{
	int cpu;

	kthread_stop(percpu_thread_a);
	kthread_stop(percpu_thread_b);

	for_each_possible_cpu(cpu)
	    pr_info("percpu_demo: cpu=%d final count=%lu\n", cpu,
		    per_cpu(demo_counter, cpu));

	pr_info("percpu_demo: unloaded\n");
}

module_init(percpu_demo_init);
module_exit(percpu_demo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Demonstrates correct per-CPU counter usage");
