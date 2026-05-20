// SPDX-License-Identifier: GPL-2.0
#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#define SNAPSHOT_RING_SIZE 64

struct snapshot {
	ktime_t timestamp;
	unsigned int process_count;
	unsigned int cpu_id;
};

static int interval = 5;
module_param(interval, int, S_IRUGO);

static struct workqueue_struct *monitor_wq;
static struct task_struct *monitor_thread;

static struct snapshot *snapshots;
static unsigned int snap_head;
static DEFINE_SPINLOCK(snap_lock);

static void take_snapshot(void)
{
	struct task_struct *task;
	unsigned int count = 0;
	unsigned long flags;
	unsigned int idx;

	rcu_read_lock();
	for_each_process(task) count++;
	rcu_read_unlock();

	spin_lock_irqsave(&snap_lock, flags);
	idx = snap_head % SNAPSHOT_RING_SIZE;
	snapshots[idx].timestamp = ktime_get();
	snapshots[idx].process_count = count;
	snapshots[idx].cpu_id = smp_processor_id();
	snap_head++;
	spin_unlock_irqrestore(&snap_lock, flags);

	pr_info("monitor: total tasks = %u\n", count);
}

static void monitor_work_handler(struct work_struct *work)
{
	take_snapshot();
}

static DECLARE_WORK(monitor_work, monitor_work_handler);

static int thread_fn(void *data)
{
	int bit;

	allow_signal(SIGINT);
	allow_signal(SIGTERM);

	while (!kthread_should_stop()) {
		if (msleep_interruptible(interval * 1000) == 0) {
			queue_work(monitor_wq, &monitor_work);
			continue;
		}

		if (!signal_pending(current))
			continue;

		for_each_set_bit(
		    bit, current->signal->shared_pending.signal.sig, _NSIG)
		{
			if (bit == SIGINT - 1) {
				pr_info("monitor: SIGINT received, counting "
					"tasks\n");
				take_snapshot();
			} else if (bit == SIGTERM - 1) {
				pr_info("monitor: SIGTERM received, exiting "
					"thread\n");
				flush_signals(current);
				return 0;
			}
		}

		flush_signals(current);
	}
	return 0;
}

static int __init monitor_init(void)
{
	if (interval < 1) {
		pr_warn("Interval must be greater than 0. Setting parameter to "
			"value 1.\n");
		interval = 1;
	}

	snapshots = kcalloc(SNAPSHOT_RING_SIZE, sizeof(*snapshots), GFP_KERNEL);
	if (!snapshots)
		return -ENOMEM;

	monitor_wq = create_singlethread_workqueue("monitor_wq");
	if (!monitor_wq) {
		kfree(snapshots);
		return -ENOMEM;
	}

	queue_work(monitor_wq, &monitor_work);

	monitor_thread = kthread_run(thread_fn, NULL, "monitor_thread");
	if (IS_ERR(monitor_thread)) {
		destroy_workqueue(monitor_wq);
		kfree(snapshots);
		return PTR_ERR(monitor_thread);
	}

	pr_info("Monitor module loaded\n");

	return 0;
}

static void __exit monitor_exit(void)
{
	unsigned int i, n;

	kthread_stop(monitor_thread);
	pr_info("monitor: thread stopped\n");
	flush_workqueue(monitor_wq);
	destroy_workqueue(monitor_wq);

	n = snap_head < SNAPSHOT_RING_SIZE ? snap_head : SNAPSHOT_RING_SIZE;
	pr_info("monitor: dumping %u snapshot(s)\n", n);
	for (i = 0; i < n; i++) {
		pr_info("monitor: snapshot[%u] ts=%lld ns count=%u cpu=%u\n", i,
			ktime_to_ns(snapshots[i].timestamp),
			snapshots[i].process_count, snapshots[i].cpu_id);
	}

	kfree(snapshots);
	pr_info("Monitor module unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Process monitor kernel module with snapshot ring buffer");
