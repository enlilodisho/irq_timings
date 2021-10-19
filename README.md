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


