TARGET = irq_timings

obj-m += ${TARGET}.o

all:
	        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	        make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	        sudo insmod ${TARGET}.ko

uninstall:
	        sudo rmmod ${TARGET}

