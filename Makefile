# my module name (optional)
MODULE=system_timer

# environment variables; depend where you have kernel src and tools
CCPREFIX=/home/radu/RaspberryPi/tools/arm-bcm2708/arm-bcm2708-linux-gnueabi/bin/arm-bcm2708-linux-gnueabi-
KERNEL_SRC=/home/radu/RaspberryPi/linux


obj-m += ${MODULE}.o

#module_upload=${MODULE}.ko

all: clean compile

compile:
	make ARCH=arm CROSS_COMPILE=${CCPREFIX} -C ${KERNEL_SRC} M=$(PWD) modules

clean:
	make -C ${KERNEL_SRC} M=$(PWD) clean

#info:
#	modinfo ${module_upload}
