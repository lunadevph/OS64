#include "pci.h"
#include "io.h"

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc
#define PCI_MAX_DEVICES 64

static pci_device_t devices[PCI_MAX_DEVICES];
static unsigned count;

static uint32_t config_read(unsigned bus, unsigned slot, unsigned function, unsigned offset) {
    outl(PCI_CONFIG_ADDRESS, 0x80000000u | (bus << 16) | (slot << 11) |
         (function << 8) | (offset & 0xfcu));
    return inl(PCI_CONFIG_DATA);
}

void pci_initialize(void) {
    count = 0;
    for (unsigned bus = 0; bus < 256 && count < PCI_MAX_DEVICES; bus++) {
        for (unsigned slot = 0; slot < 32 && count < PCI_MAX_DEVICES; slot++) {
            uint32_t first = config_read(bus, slot, 0, 0);
            if ((first & 0xffffu) == 0xffffu) continue;
            uint32_t header = config_read(bus, slot, 0, 0x0c);
            unsigned functions = (header & 0x00800000u) ? 8 : 1;
            for (unsigned function = 0; function < functions && count < PCI_MAX_DEVICES; function++) {
                uint32_t id = config_read(bus, slot, function, 0);
                if ((id & 0xffffu) == 0xffffu) continue;
                uint32_t type = config_read(bus, slot, function, 8);
                pci_device_t *device = &devices[count++];
                device->bus = (uint8_t)bus; device->slot = (uint8_t)slot;
                device->function = (uint8_t)function;
                device->vendor = (uint16_t)id; device->device = (uint16_t)(id >> 16);
                device->revision = (uint8_t)type; device->prog_if = (uint8_t)(type >> 8);
                device->subclass = (uint8_t)(type >> 16); device->class_code = (uint8_t)(type >> 24);
            }
        }
    }
}

unsigned pci_device_count(void) { return count; }
int pci_device_at(unsigned index, pci_device_t *device) {
    if (!device || index >= count) return 0;
    *device = devices[index]; return 1;
}

const char *pci_class_name(uint8_t class_code, uint8_t subclass) {
    if (class_code == 0x01 && subclass == 0x01) return "IDE controller";
    if (class_code == 0x01 && subclass == 0x06) return "SATA controller";
    if (class_code == 0x02 && subclass == 0x00) return "Ethernet controller";
    if (class_code == 0x03 && subclass == 0x00) return "VGA controller";
    if (class_code == 0x04 && subclass == 0x01) return "audio device";
    if (class_code == 0x06 && subclass == 0x00) return "host bridge";
    if (class_code == 0x06 && subclass == 0x01) return "ISA bridge";
    if (class_code == 0x06 && subclass == 0x04) return "PCI bridge";
    if (class_code == 0x0c && subclass == 0x03) return "USB controller";
    return "unclassified device";
}
