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

#define CLASS_NAME      "irq_timings"
#define GPIO_COUNT      100     // only first {GPIO_COUNT} pins will be supported
#define BUFFER_SIZE     1024    // size of buffer for timings
#define CLASS_ATTR_PERM 0220    // permissions for class attribute files

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

MODULE_AUTHOR("Enlil Odisho <github@enlilodisho.com>");
MODULE_DESCRIPTION("Driver for measuring time between interrupts on gpio pins.");
MODULE_LICENSE("GPL");

static const unsigned long CLASS_ATTR_NAME_SIZE = sizeof("gpio") + sizeof(GPIO_COUNT);

#define CLASS_ATTR_WRITE(_name) \
    struct class_attribute class_attr_##_name = __ATTR(_name, CLASS_ATTR_PERM, \
                                                       NULL, _name##_store)
/* struct representing driver_class. defined below */
static struct class driver_class;

/* struct representing gpio data */
static struct gpio_data {
    struct class_attribute class_attr_gpio;
    unsigned int irq_number;
    unsigned int writeBuf[BUFFER_SIZE];
    size_t writeI;
} *registered_gpios[GPIO_COUNT];

static void free_gpio_data(size_t gpio)
{
    if (registered_gpios[gpio] != NULL)
    {
        kfree(registered_gpios[gpio]->class_attr_gpio.attr.name);
        kfree(registered_gpios[gpio]);
        registered_gpios[gpio] = NULL;
    }
}

static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id,
        struct pt_regs* regs)
{
    printk(KERN_INFO "irq_timings: gpio_irq_handler called (irq:%u)\n", irq);
    return (irq_handler_t) IRQ_HANDLED;
}

/**
 * Invoked when read from /sys/class/{CLASS_NAME}/gpio{GPIO_ID}
 */
static ssize_t gpio_show(struct class* class, struct class_attribute* attr,
        char* buf)
{
    return 0;
}

/**
 * Invoked when write to /sys/class/{CLASS_NAME}/register attribute file.
 */
static ssize_t register_store(struct class* class,
        struct class_attribute* attr, const char* buf, size_t count)
{
    unsigned long gpio;
    struct gpio_data* gpioData;
    char* class_attr_name;
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
        goto GpioDirectionSetupError;
    }

    // create struct gpio_data obj for this gpio pin
    gpioData = kmalloc(sizeof(struct gpio_data), GFP_KERNEL);
    class_attr_name = kmalloc(CLASS_ATTR_NAME_SIZE, GFP_KERNEL);
    snprintf(class_attr_name, CLASS_ATTR_NAME_SIZE, "gpio%lu", gpio);
    gpioData->class_attr_gpio.attr = (struct attribute) { class_attr_name,
                                    VERIFY_OCTAL_PERMISSIONS(CLASS_ATTR_PERM) };
    gpioData->class_attr_gpio.show = gpio_show;
    gpioData->class_attr_gpio.store = NULL;
    gpioData->writeI = 0;
    registered_gpios[gpio] = gpioData;

    // add gpio class attribute file
    if (class_create_file(&driver_class, &registered_gpios[gpio]->class_attr_gpio) < 0)
    {
        printk(KERN_ERR "error creating gpio%lu class attribute file\n", gpio);
        goto GpioClassAttributeFileError;
    }

    // setup interrupt
    registered_gpios[gpio]->irq_number = gpio_to_irq(gpio);
    if (request_irq(registered_gpios[gpio]->irq_number,
                (irq_handler_t) gpio_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                class_attr_name, registered_gpios[gpio]) < 0)
    {
        printk(KERN_ERR "error setting up interrupt on gpio %lu\n", gpio);
        goto GpioInterruptSetupError;
    }

    return count;

    /* handler cleanup after error */
GpioInterruptSetupError:
    class_remove_file(&driver_class, &registered_gpios[gpio]->class_attr_gpio);
GpioClassAttributeFileError:
    free_gpio_data(gpio);
GpioDirectionSetupError:
    gpio_free(gpio);
    // return -1 to mark error status
    return -1;
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

    // remove gpio interrupt
    free_irq(registered_gpios[gpio]->irq_number, registered_gpios[gpio]);

    // remove gpio class attribute file
    class_remove_file(&driver_class, &registered_gpios[gpio]->class_attr_gpio);

    // free gpio pin
    gpio_free(gpio);

    // free and remove gpio data from registered_gpios
    free_gpio_data(gpio);

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
            free_irq(registered_gpios[i]->irq_number, registered_gpios[i]);
            class_remove_file(&driver_class, &registered_gpios[i]->class_attr_gpio);
            gpio_free(i);
            free_gpio_data(i);
        }
    }
    class_destroy(&driver_class);
    printk(KERN_INFO "irq_timings: exit\n");
}

// Register module initialization and exit functions.
module_init(irqts_init);
module_exit(irqts_exit);

