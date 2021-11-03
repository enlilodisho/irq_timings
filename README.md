# irq_timings

A kernel module that computes the timings between interrupts for a gpio pin.

## Introduction

### Motivation

Suppose you have a sensor where to accurately measure the data, you need to be 
able to measure the length of time the sensor outputs a HIGH or LOW signal. An 
example of some sensors where this task may be required are: rf receivers, ir 
photodiodes, etc. One way to measure the time is by configuring an interrupt on 
the rising and falling edges of the gpio pin the sensor is connected to. This 
introduces a problem, where the delay between an interrupt being generated and 
the code in your interrupt handler function which measures the time difference 
becomes unpredictable.

### Solution

irq_timings aims to solve this problem by handling the interrupt and computing 
the timings in a kernel module rather than in a userspace program. This provides 
more accurate timing data from the sensor which your userspace program can 
retrieve.

## Build & Install

To build kernel module:
```
make
```

To install kernel module after building:
```
make install
```

To uninstall kernel module:
```
make uninstall
```

## Usage

#### Step 1: Registering a GPIO Pin

To attach an interrupt to a gpio pin and compute the timings between interrupts,
you will need to register the gpio pin with the irq_timings kernel module.
```
echo "PIN_NUMBER" > /sys/class/irq_timings/register
```

#### Step 2: Reading the timings for a registered gpio pin

For each registered gpio pin, there will be a corresponding file created in
`/sys/class/irq_timings/` with the name `gpio + PIN_NUMBER`. Reading from this
file will display the last timings collected for the gpio pin.

#### Example for registering and reading timings from gpio pin 16
```
echo "16" > /sys/class/irq_timings/register
cat /sys/class/irq_timings/gpio16
```

##### Unregistering a pin

To remove the interrupt and stop collecting timings for a gpio pin:
```
echo "PIN_NUMBER" > /sys/class/irq_timings/unregister
```

