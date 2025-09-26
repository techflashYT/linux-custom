#ifndef XENON_PCI_H
#define XENON_PCI_H

extern void __init xenon_pci_init(void);
extern void xenon_pcibios_fixup_bus(struct pci_bus *bus);

#endif
