# ICMPCOUNT 

## Content
This repo contains the following folders:

### dev/ : driver source code

- `icmpcount_interface.c` is the source code of the driver. The function that should be modified to add more IOCTL
- `icmpcount_dev.h` is the header file
- `icmpcount.h` contains the list of IOCTL codes. This list is also included in the application
- `Makefile` to build the driver. If the driver is cross-compiled, the variable KDEV should be adjusted

### app/ : application source code

- `ioctl_app.c` shows an example to access the driver and get the returned ICMP counter.
- `Makefile` to build the application

### load_driver.sh

- Load the driver _ioctl_. This probably needs sudo permissions!


## Build & Run

### Kernel module
	$ cd app/
	$ make
	$ cd ../dev/
	$ make
	$ cd ..
	$ sudo ./load_driver.sh
	
To remove the kernel module use:
	$ sudo rmmod icmpcount

### Userland binary
	$ cd app/
	$ sudo ./icmpcount_app
	$ ping localhost # Will generate some ICMP packet on the `lo` network interface

and **Ctrl-C** to interrupt at any moment

## TODO

- Support IPv6.

