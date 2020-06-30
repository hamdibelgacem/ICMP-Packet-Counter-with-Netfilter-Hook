CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -std=c99 -O0 -g -Wconversion -Wextra

all: clean icmpcount_app

ioctl_app: icmpcount_app.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o icmpcount_app
