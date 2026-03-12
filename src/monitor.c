#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/workqueue.h>

static int interval = 5;
module_param(interval, int, S_IRUGO);

static struct workqueue_struct *monitor_wq;

static struct task_struct *monitor_thread;

static void monitor_work_handler(struct work_struct *work)
{
	struct task_struct *task;
	int count = 0;

	rcu_read_lock();
	for_each_process(task) count++;
	rcu_read_unlock();

	pr_info("monitor: total tasks = %d\n", count);
}

static DECLARE_WORK(monitor_work, monitor_work_handler);

static void count_and_log_tasks(void)
{
	struct task_struct *task;
	int count = 0;

	rcu_read_lock();
	for_each_process(task) count++;
	rcu_read_unlock();

	pr_info("monitor: total tasks = %d\n", count);
}

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
				count_and_log_tasks();
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

	monitor_wq = create_singlethread_workqueue("monitor_wq");
	if (!monitor_wq)
		return -ENOMEM;

	queue_work(monitor_wq, &monitor_work);

	monitor_thread = kthread_run(thread_fn, NULL, "monitor_thread");
	if (IS_ERR(monitor_thread)) {
		destroy_workqueue(monitor_wq);
		return PTR_ERR(monitor_thread);
	}

	pr_info("Monitor module loaded\n");

	return 0;
}

static void __exit monitor_exit(void)
{
	kthread_stop(monitor_thread);
	pr_info("monitor: thread stopped\n");
	flush_workqueue(monitor_wq);
	destroy_workqueue(monitor_wq);
	pr_info("Monitor module unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Process monitor kernel module");