#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Le Tien Thanh - SE201799");
MODULE_DESCRIPTION("ASSIGNMENT-01 ioctl character driver");
MODULE_VERSION("1.0");


#define DEVICE_NAME      "asgn1"
#define CLASS_NAME       "asgn1_class"
#define ASGN1_MAJOR      241
#define BUFFER_SIZE      1024
#define VERSION_STRING   "asgn1_driver v1.0"
#define KERNEL_PREFIX    "[KERNEL]"


static char device_buffer[BUFFER_SIZE];
static int buffer_len;

static int open_count;
static int read_count;
static int write_count;
static int last_write_size;

static int current_mode = 1;

static struct class *asgn1_class;
static struct device *asgn1_device;

static DEFINE_MUTEX(asgn1_mutex);


static int asgn1_open(struct inode *inode, struct file *file)
{
    int count;

    mutex_lock(&asgn1_mutex);

    open_count++;
    count = open_count;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: device opened (open_count=%d)\n", count);

    return 0;
}


static int asgn1_release(struct inode *inode, struct file *file)
{
    pr_info("asgn1_driver: device closed\n");

    return 0;
}


static ssize_t asgn1_read(struct file *file,
                          char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    size_t bytes_to_read;

    mutex_lock(&asgn1_mutex);

    if (*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }

    bytes_to_read = buffer_len - *offset;

    if (len < bytes_to_read)
        bytes_to_read = len;

    if (copy_to_user(buf,
                     device_buffer + *offset,
                     bytes_to_read)) {
        mutex_unlock(&asgn1_mutex);

        pr_err("asgn1_driver: copy_to_user failed\n");
        return -EFAULT;
    }

    *offset += bytes_to_read;
    read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: read %zu bytes\n",
            bytes_to_read);

    return bytes_to_read;
}


static ssize_t asgn1_write(struct file *file,
                           const char __user *buf,
                           size_t len,
                           loff_t *offset)
{
    size_t payload_len;
    size_t prefix_len = sizeof(KERNEL_PREFIX) - 1;
    size_t max_payload;

    if (len == 0)
        return 0;

    mutex_lock(&asgn1_mutex);

    if (current_mode == 0) {
        max_payload = BUFFER_SIZE - prefix_len - 1;
        payload_len = len;

        if (payload_len > max_payload)
            payload_len = max_payload;

        memcpy(device_buffer, KERNEL_PREFIX, prefix_len);

        if (copy_from_user(device_buffer + prefix_len,
                           buf,
                           payload_len)) {
            mutex_unlock(&asgn1_mutex);
            pr_err("asgn1_driver: copy_from_user failed\n");
            return -EFAULT;
        }

        buffer_len = prefix_len + payload_len;
    } else {
        max_payload = BUFFER_SIZE - 1;
        payload_len = len;

        if (payload_len > max_payload)
            payload_len = max_payload;

        if (copy_from_user(device_buffer,
                           buf,
                           payload_len)) {
            mutex_unlock(&asgn1_mutex);
            pr_err("asgn1_driver: copy_from_user failed\n");
            return -EFAULT;
        }

        buffer_len = payload_len;
    }

    device_buffer[buffer_len] = '\0';

    write_count++;
    last_write_size = payload_len;

    pr_info("asgn1_driver: write %zu bytes, mode=%d, stored=%d bytes\n",
            payload_len,
            current_mode,
            buffer_len);

    mutex_unlock(&asgn1_mutex);

    return payload_len;
}


static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    struct asgn1_stats stats;
    int new_mode;
    size_t version_len;

    switch (cmd) {

    case ASGN1_RESET_BUFFER:
        mutex_lock(&asgn1_mutex);

        memset(device_buffer, 0, BUFFER_SIZE);
        buffer_len = 0;
        read_count = 0;
        write_count = 0;
        last_write_size = 0;

        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: ioctl RESET_BUFFER\n");
        return 0;

    case ASGN1_GET_STATS:
        mutex_lock(&asgn1_mutex);

        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;

        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg,
                         &stats,
                         sizeof(stats))) {
            pr_err("asgn1_driver: GET_STATS copy_to_user failed\n");
            return -EFAULT;
        }

        pr_info("asgn1_driver: ioctl GET_STATS\n");
        return 0;

    case ASGN1_SET_MODE:
        if (copy_from_user(&new_mode,
                           (int __user *)arg,
                           sizeof(new_mode))) {
            pr_err("asgn1_driver: SET_MODE copy_from_user failed\n");
            return -EFAULT;
        }

        if (new_mode != 0 && new_mode != 1) {
            pr_warn("asgn1_driver: invalid mode %d\n", new_mode);
            return -EINVAL;
        }

        mutex_lock(&asgn1_mutex);
        current_mode = new_mode;
        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: ioctl SET_MODE mode=%d\n",
                new_mode);
        return 0;

    case ASGN1_GET_VERSION:
        version_len = strlen(VERSION_STRING) + 1;

        if (version_len > ASGN1_VERSION_LEN)
            version_len = ASGN1_VERSION_LEN;

        if (copy_to_user((void __user *)arg,
                         VERSION_STRING,
                         version_len)) {
            pr_err("asgn1_driver: GET_VERSION copy_to_user failed\n");
            return -EFAULT;
        }

        pr_info("asgn1_driver: ioctl GET_VERSION\n");
        return 0;

    default:
        pr_warn("asgn1_driver: unsupported ioctl 0x%x\n", cmd);
        return -ENOTTY;
    }
}


static struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .open = asgn1_open,
    .release = asgn1_release,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
};


static int __init asgn1_init(void)
{
    int ret;

    ret = register_chrdev(ASGN1_MAJOR,
                          DEVICE_NAME,
                          &asgn1_fops);

    if (ret < 0) {
        pr_err("asgn1: cannot register device\n");
        return ret;
    }

    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(asgn1_class)) {
        unregister_chrdev(ASGN1_MAJOR, DEVICE_NAME);
        pr_err("asgn1: cannot create class\n");
        return PTR_ERR(asgn1_class);
    }

    asgn1_device = device_create(asgn1_class,
                                 NULL,
                                 MKDEV(ASGN1_MAJOR, 0),
                                 NULL,
                                 DEVICE_NAME);

    if (IS_ERR(asgn1_device)) {
        class_destroy(asgn1_class);
        unregister_chrdev(ASGN1_MAJOR, DEVICE_NAME);

        pr_err("asgn1: cannot create device\n");
        return PTR_ERR(asgn1_device);
    }

    pr_info("asgn1: driver loaded, major=%d\n",
            ASGN1_MAJOR);

    return 0;
}


static void __exit asgn1_exit(void)
{
    device_destroy(asgn1_class,
                   MKDEV(ASGN1_MAJOR, 0));

    class_destroy(asgn1_class);

    unregister_chrdev(ASGN1_MAJOR,
                      DEVICE_NAME);

    pr_info("asgn1: driver unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
