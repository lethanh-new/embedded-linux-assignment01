#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Le Tien Thanh - SE201799");
MODULE_DESCRIPTION("ASSIGNMENT-01 Misc Counter Driver");



static atomic_t counter = ATOMIC_INIT(0);
static atomic_t read_count = ATOMIC_INIT(0);
static atomic_t write_count = ATOMIC_INIT(0);

static struct proc_dir_entry *counter_proc;




static ssize_t counter_read(struct file *file,
                            char __user *buf,
                            size_t len,
                            loff_t *ppos)
{
    char kbuf[32];
    int value;
    int size;

    if (*ppos != 0)
        return 0;

    value = atomic_read(&counter);

    size = scnprintf(kbuf, sizeof(kbuf), "%d\n", value);

    if (len < size)
        return -EINVAL;

    if (copy_to_user(buf, kbuf, size))
        return -EFAULT;

    *ppos += size;

    atomic_inc(&counter);
    atomic_inc(&read_count);

    return size;
}



static ssize_t counter_write(struct file *file,
                             const char __user *buf,
                             size_t len,
                             loff_t *ppos)
{
    char kbuf[32];
    int value;
    int ret;

    if (len == 0)
        return 0;

    if (len >= sizeof(kbuf))
        return -EINVAL;

    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    kbuf[len] = '\0';

    ret = kstrtoint(kbuf, 10, &value);
    if (ret)
        return -EINVAL;

    if (value < 0)
        return -EINVAL;

    atomic_set(&counter, value);
    atomic_inc(&write_count);

    return len;
}


static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read  = counter_read,
    .write = counter_write,
};



static struct miscdevice counter_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "counter",
    .fops  = &counter_fops,
};




static int counter_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m,
               "Current counter: %d\n",
               atomic_read(&counter));

    seq_printf(m,
               "Total reads: %d\n",
               atomic_read(&read_count));

    seq_printf(m,
               "Total writes: %d\n",
               atomic_read(&write_count));

    return 0;
}


static int counter_proc_open(struct inode *inode,
                             struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}


static const struct proc_ops counter_proc_ops = {
    .proc_open    = counter_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};


static int __init counter_init(void)
{
    int ret;

    atomic_set(&counter, 0);
    atomic_set(&read_count, 0);
    atomic_set(&write_count, 0);

    ret = misc_register(&counter_misc);
    if (ret) {
        pr_err("counter_driver: misc_register failed\n");
        return ret;
    }

    counter_proc = proc_create("counter_info",
                               0444,
                               NULL,
                               &counter_proc_ops);

    if (!counter_proc) {
        pr_err("counter_driver: cannot create /proc/counter_info\n");

        misc_deregister(&counter_misc);

        return -ENOMEM;
    }

    pr_info("counter_driver: loaded\n");
    pr_info("counter_driver: /dev/counter created\n");
    pr_info("counter_driver: /proc/counter_info created\n");

    return 0;
}



static void __exit counter_exit(void)
{
    proc_remove(counter_proc);

    misc_deregister(&counter_misc);

    pr_info("counter_driver: unloaded\n");
}


module_init(counter_init);
module_exit(counter_exit);