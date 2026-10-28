#include "riscv/bus.h"
#include <stdlib.h>

#define BUS_MAX_DEVICES 8

struct Bus {
  struct BusDevice devices[BUS_MAX_DEVICES];
  unsigned count;
};

Bus *bus_create(void) {
  Bus *bus = (Bus *)malloc(sizeof(Bus));
  bus->count = 0;
  return bus;
}

void bus_destroy(Bus *bus) { free(bus); }

static bool contains(uxlen_t base, uxlen_t size, uxlen_t addr, uxlen_t len) {
  return len <= size && addr >= base && addr - base <= size - len;
}

static bool overlaps(uxlen_t base1, uxlen_t size1, uxlen_t base2,
                     uxlen_t size2) {
  return base1 < base2 + size2 && base2 < base1 + size1;
}

static struct BusDevice *find_device(Bus *bus, uxlen_t addr, unsigned size) {
  for (unsigned i = 0; i < bus->count; i++) {
    struct BusDevice *dev = &bus->devices[i];
    if (contains(dev->base, dev->size, addr, size))
      return dev;
  }
  return NULL;
}

bool bus_attach(Bus *bus, struct BusDevice device) {
  if (bus->count >= BUS_MAX_DEVICES)
    return false;
  for (unsigned i = 0; i < bus->count; i++) {
    struct BusDevice *dev = &bus->devices[i];
    if (overlaps(dev->base, dev->size, device.base, device.size))
      return false;
  }
  bus->devices[bus->count++] = device;
  return true;
}

enum BusStatus bus_read(Bus *bus, uxlen_t addr, unsigned size,
                        uint64_t *value) {
  struct BusDevice *dev = find_device(bus, addr, size);
  if (dev == NULL)
    return BUS_ACCESS_FAULT;
  return dev->read(dev->device, addr - dev->base, size, value);
}

enum BusStatus bus_write(Bus *bus, uxlen_t addr, unsigned size,
                         uint64_t value) {
  struct BusDevice *dev = find_device(bus, addr, size);
  if (dev == NULL)
    return BUS_ACCESS_FAULT;
  return dev->write(dev->device, addr - dev->base, size, value);
}
