/*
 * irq_timings.c
 *
 * Kernel module for setting up interrupts on specified gpio pin(s), measuring 
 * the timings between the interrupts, and providing the computed timings to a 
 * userspace program.
 *
 * Enlil Odisho
 * github@enlilodisho.com
 * October 2021
 */

#define CLASS_NAME          "irq_timings"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>

MODULE_AUTHOR("Enlil Odisho <github@enlilodisho.com>");
MODULE_DESCRIPTION("Driver for measuring time between interrupts on gpio pins.");
MODULE_LICENSE("GPL");

#define CLASS_ATTR_WRITE(_name) \
    struct class_attribute class_attr_##_name = __ATTR(_name, 0220, NULL, _name##_store)

static struct class* driver_class;

/**
 * Invoked when write to /sys/class/{DRIVER_CLASS}/register attribute file.
 */
static ssize_t register_store(struct class* class,
        struct class_attribute* attr, const char* buf, size_t count)
{
    printk(KERN_INFO "irq_timings: register store called\n");
    return count;
}

/* class sysfs attributes */
static CLASS_ATTR_WRITE(register); // class_attr_register

/**
 * Invoked when module is added to kernel.
 */
static int __init irqts_init(void)
{
    printk(KERN_INFO "irq_timings: hello\n");

    // create driver class
    if (IS_ERR(driver_class = class_create(THIS_MODULE, CLASS_NAME)))
    {
        printk(KERN_ERR "failure creating driver class %s\n", CLASS_NAME);
        goto ClassError;
    }

    // add register class attribute
    if (class_create_file(driver_class, &class_attr_register) < 0)
    {
        printk(KERN_ERR "failure adding register class attribute\n");
        goto AddRegisterAttrError;
    }

    return 0;

    /* handle cleanup after error */
    class_remove_file(driver_class, &class_attr_register);
AddRegisterAttrError:
    class_destroy(driver_class);
ClassError:
    // return -1 to mark error status
    return -1;
}

/**
 * Invoked when module is removed from kernel.
 */
static void __exit irqts_exit(void)
{
    class_remove_file(driver_class, &class_attr_register);
    class_destroy(driver_class);
    printk(KERN_INFO "irq_timings: exit\n");
}

// Register module initialization and exit functions.
module_init(irqts_init);
module_exit(irqts_exit);

