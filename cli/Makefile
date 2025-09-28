
CC = g++
CFLAGS = -I. -O2 -Wall -static -std=c++14
DEPS = i2c.h ugreen_leds.h zfs_monitor.h
OBJ = i2c.o ugreen_leds.o 
ZFS_OBJ = zfs_monitor.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ugreen_leds_cli: $(OBJ) ugreen_leds_cli.o
	$(CC) -o $@ $^ $(CFLAGS)

ugreen_zfs_monitor: $(OBJ) $(ZFS_OBJ) ugreen_zfs_monitor.o
	$(CC) -o $@ $^ $(CFLAGS)

all: ugreen_leds_cli ugreen_zfs_monitor

clean:
	rm -f *.o ugreen_leds_cli ugreen_zfs_monitor

install: ugreen_leds_cli ugreen_zfs_monitor
	cp ugreen_leds_cli /usr/local/bin/
	cp ugreen_zfs_monitor /usr/local/bin/
	chmod +x /usr/local/bin/ugreen_leds_cli
	chmod +x /usr/local/bin/ugreen_zfs_monitor

.PHONY: all clean install
