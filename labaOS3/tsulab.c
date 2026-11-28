#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/version.h>

static long long session_start = 1799600400LL;
module_param(session_start, llong, 0644);
MODULE_PARM_DESC(session_start, "Unix time of the session start (UTC)");

static struct proc_dir_entry *proc_file;

static ssize_t procfile_read(struct file *file, char __user *buffer,
			     size_t buffer_length, loff_t *offset)
{
	char message[160];
	long long left;
	size_t length;

	if (*offset != 0)
		return 0;

	left = session_start - (long long)ktime_get_real_seconds();

	if (left > 0) {
		long long days = left / 86400;
		int hours = (left % 86400) / 3600;
		int minutes = (left % 3600) / 60;
		int seconds = left % 60;

		snprintf(message, sizeof(message),
			 "Seconds until the session: %lld\n"
			 "That is %lld d %02d h %02d m %02d s\n",
			 left, days, hours, minutes, seconds);
	} else {
		snprintf(message, sizeof(message),
			 "The session has already started!\n");
	}

	length = min(strlen(message), buffer_length);

	if (copy_to_user(buffer, message, length))
		return -EFAULT;

	*offset += length;
	return length;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_ops = {
	.proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_ops = {
	.read = procfile_read,
};
#endif

static int __init tsulab_init(void)
{
	proc_file = proc_create("tsulab", 0444, NULL, &proc_file_ops);
	if (!proc_file)
		return -ENOMEM;

	pr_info("Welcome to the Tomsk State University\n");
	return 0;
}

static void __exit tsulab_exit(void)
{
	proc_remove(proc_file);
	pr_info("Tomsk State University forever!\n");
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TSU lab: /proc/tsulab with time until the session");
