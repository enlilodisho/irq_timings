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
#define GPIO_COUNT          100     // only first {GPIO_COUNT} pins will be supported
#define BUFFER_SIZE         1024    // size of buffer for timings

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>

MODULE_AUTHOR("Enlil Odisho <github@enlilodisho.com>");
MODULE_DESCRIPTION("Driver for measuring time between interrupts on gpio pins.");
MODULE_LICENSE("GPL");

#define CLASS_ATTR_WRITE(_name) \
    struct class_attribute class_attr_##_name = __ATTR(_name, 0220, NULL, _name##_store)

static struct gpio_data {
    unsigned int writeBuf[BUFFER_SIZE];
    size_t writeI;
} *registered_gpios[GPIO_COUNT];
//static struct gpio_data* registered_gpios[GPIO_COUNT] = {NULL};

/**
 * Invoked when write to /sys/class/{CLASS_NAME}/register attribute file.
 */
static ssize_t register_store(struct class* class,
        struct class_attribute* attr, const char* buf, size_t count)
{
    unsigned long gpio;
    struct gpio_data* gpioData;
    printk(KERN_INFO "irq_timings: register store called\n");

    // read gpio pin from input
    if (kstrtoul(buf, 0, &gpio) < 0)
    {
        printk(KERN_WARNING "error parsing input\n");
        return -EINVAL;
    }
    
    // verify gpio is within GPIO_COUNT
    if (gpio >= GPIO_COUNT)
    {
        printk(KERN_ERR "gpio %lu is outside acceptable range\n", gpio);
        return -EINVAL;
    }

    // verify gpio is not already registered
    if (registered_gpios[gpio] != NULL)
    {
        printk(KERN_ERR "gpio %lu is already registered\n", gpio);
        return -1;
    }

    // allocate gpio pin
    if (gpio_request(gpio, "gpio-"+gpio) < 0)
    {
        printk(KERN_ERR "error allocating gpio %lu\n", gpio);
        return -EINVAL;
    }

    // set gpio pin as input
    if (gpio_direction_input(gpio) < 0)
    {
        printk(KERN_ERR "error setting gpio %lu as input\n", gpio);
        gpio_free(gpio);
        return -1;
    }

    gpioData = kmalloc(sizeof(struct gpio_data), GFP_KERNEL);
    gpioData->writeI = 0;
    registered_gpios[gpio] = gpioData;

    return count;
}

/**
 * Invoked when write to /sys/class/{CLASS_NAME}/unregister attribute file.
 */
static ssize_t unregister_store(struct class* class,
        struct class_attribute* attr, const char* buf, size_t count)
{
    unsigned long gpio;
    printk(KERN_INFO "irq_timings: unregister store called\n");

    // read gpio from input
    if (kstrtoul(buf, 0, &gpio) < 0)
    {
        printk(KERN_WARNING "error parsing input\n");
        return -EINVAL;
    }

    // verify gpio is within GPIO_COUNT
    if (gpio >= GPIO_COUNT)
    {
        printk(KERN_ERR "gpio %lu is outside acceptable range\n", gpio);
        return -EINVAL;
    }

    // verify gpio is currently registered
    if (registered_gpios[gpio] == NULL)
    {
        printk(KERN_ERR "gpio %lu is not registered\n", gpio);
        return -1;
    }

    // free gpio pin
    gpio_free(gpio);

    // remove gpio data from registered_gpios
    kfree(registered_gpios[gpio]);
    registered_gpios[gpio] = NULL;

    return count;
}

/* class sysfs attributes */
static CLASS_ATTR_WRITE(register); // class_attr_register
static CLASS_ATTR_WRITE(unregister); // class_attr_unregister

/* list all class attributes in attributes group */
static struct attribute* irq_timings_class_attrs[] = {
    &class_attr_register.attr,
    &class_attr_unregister.attr,
    NULL
};
ATTRIBUTE_GROUPS(irq_timings_class);

/* struct representing driver class */
static struct class driver_class = {
    .name           = CLASS_NAME,
    .owner          = THIS_MODULE,
    .class_groups   = irq_timings_class_groups
};

/**
 * Invoked when module is added to kernel.
 */
static int __init irqts_init(void)
{
    printk(KERN_INFO "irq_timings: hello\n");

    // create driver class
    if (class_register(&driver_class) < 0)
    {
        printk(KERN_ERR "failure creating driver class %s\n", CLASS_NAME);
        goto ClassError;
    }

    return 0;

    /* handle cleanup after error */
    class_destroy(&driver_class);
ClassError:
    // return -1 to mark error status
    return -1;
}

/**
 * Invoked when module is removed from kernel.
 */
static void __exit irqts_exit(void)
{
    size_t i;
    // free all registered gpio
    for (i = 0; i < GPIO_COUNT; i++)
    {
        if (registered_gpios[i] != NULL)
        {
            kfree(registered_gpios[i]);
            registered_gpios[i] = NULL;
        }
    }
    class_destroy(&driver_class);
    printk(KERN_INFO "irq_timings: exit\n");
}

// Register module initialization and exit functions.
module_init(irqts_init);
module_exit(irqts_exit);

