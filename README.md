RPiSystemTimer
==============

A kernel device driver for the Raspberry Pi system timer that allows an application to register with the driver, and then receive signals every X microseconds. The user application would need to implement a signal handler that overwrites the default action of the signals sent (SIGUSR1, SIGUSR2) and call the necessary method from there.


Device Driver - sys_timer.c - compile and load from a terminal window using 'insmod'

User applications - interrupts.c - An example of the signal handler and default action overwrite


- more details soon..
