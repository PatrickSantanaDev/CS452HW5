PATH=/usr/bin:/bin:/sbin

name:=Scanner
module:=$(name).ko

obj-m:=$(name).o
KDIR :=/lib/modules/$(shell uname -r)/build
PWD  :=$(shell pwd)

EXTRA_CFLAGS+=-DDEVNAME='"$(name)"'

modules clean: ; $(MAKE) -C $(KDIR) M=$(PWD) $@

install: $(module)
	sudo rmmod $(module) || true
	sudo insmod $(module)
	sudo rm -f /dev/$(name) || true
	sudo mknod -m a+rw /dev/$(name) c $$(./getmaj $(name)) 0

uninstall:
	sudo rmmod $(module) || true
	sudo rm -f /dev/$(name) || true

Force: 
TryScanner: TryScanner.c
	gcc -o $@ $< -Wall -g

Testing: Testing/Tester.c Force
	gcc -o Testing/Test Testing/Tester.c -Wall -g

try: TryScanner
	./$<