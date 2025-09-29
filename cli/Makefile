
CC = g++
CXX = g++
CFLAGS = -I. -O2 -Wall -static -std=c++17
CXXFLAGS = -I. -O2 -Wall -static -std=c++17
LDFLAGS = 
LIBS =
DEPS = i2c.h ugreen_leds.h zfs_monitor.h ugreen_monitor.h
OBJ = i2c.o ugreen_leds.o 
COMMON_OBJECTS = i2c.o ugreen_leds.o
ZFS_OBJ = zfs_monitor.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ugreen_leds_cli: $(OBJ) ugreen_leds_cli.o
	$(CC) -o $@ $^ $(CFLAGS)

# ZFS Monitor
ugreen_zfs_monitor: $(COMMON_OBJECTS) ugreen_zfs_monitor.o zfs_monitor.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# General Monitor
ugreen_monitor: $(COMMON_OBJECTS) ugreen_monitor_main.o ugreen_monitor.o zfs_monitor.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

all: ugreen_leds_cli ugreen_zfs_monitor ugreen_monitor

clean:
	rm -f *.o ugreen_leds_cli ugreen_zfs_monitor ugreen_monitor

install: ugreen_leds_cli ugreen_zfs_monitor ugreen_monitor
	cp ugreen_leds_cli /usr/local/bin/
	cp ugreen_zfs_monitor /usr/local/bin/
	cp ugreen_monitor /usr/local/bin/
	chmod +x /usr/local/bin/ugreen_leds_cli
	chmod +x /usr/local/bin/ugreen_zfs_monitor

.PHONY: all clean install
