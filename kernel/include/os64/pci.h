#ifndef OS64_PCI_H
#define OS64_PCI_H
#include <stdint.h>

typedef struct {
    uint8_t bus, slot, function, class_code, subclass, prog_if, revision;
    uint16_t vendor, device;
} pci_device_t;

void pci_initialize(void);
unsigned pci_device_count(void);
int pci_device_at(unsigned index, pci_device_t *device);
const char *pci_class_name(uint8_t class_code, uint8_t subclass);
#endif
