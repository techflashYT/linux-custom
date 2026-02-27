// SPDX-License-Identifier: GPL-2.0
/*
 *  block/partitions/psvita.c
 *  Copyright (C) 2026 Michael "Techflash" Garofalo
 *
 *  Based on block/partitons/msdos.c:
 *
 *  Code extracted from drivers/block/genhd.c
 *  Copyright (C) 1991-1998  Linus Torvalds
 *
 *  Thanks to Branko Lankester, lankeste@fwi.uva.nl, who found a bug
 *  in the early extended-partition checks and added DM partitions
 *
 *  Support for DiskManager v6.0x added by Mark Lord,
 *  with information provided by OnTrack.  This now works for linux fdisk
 *  and LILO, as well as loadlin and bootln.  Note that disks other than
 *  /dev/hda *must* have a "DOS" type 0x51 partition in the first slot (hda1).
 *
 *  More flexible handling of extended partitions - aeb, 950831
 *
 *  Check partition table on IDE disks for common CHS translations
 *
 *  Re-organised Feb 1998 Russell King
 *
 *  BSD disklabel support by Yossi Gottlieb <yogo@math.tau.ac.il>
 *  updated by Marc Espie <Marc.Espie@openbsd.org>
 *
 *  Unixware slices support by Andrzej Krzysztofowicz <ankry@mif.pg.gda.pl>
 *  and Krzysztof G. Baranowski <kgb@knm.org.pl>
 */
#include <linux/types.h>
#include <linux/psvita_partition.h>

#include "check.h"

/*
 * Many architectures don't like unaligned accesses, while
 * the nr_block and start_block partition table entries are
 * at a 2 (mod 4) address.
 */
#include <linux/unaligned.h>

static inline sector_t nr_blocks(struct psvita_partition *p)
{
	return (sector_t)get_unaligned_le32(&p->size);
}

static inline sector_t start_block(struct psvita_partition *p)
{
	return (sector_t)get_unaligned_le32(&p->offset);
}

#define PSVITA_LABEL_MAGIC      "Sony Computer Entertainment Inc."
#define MSDOS_LABEL_MAGIC1	0x55
#define MSDOS_LABEL_MAGIC2	0xAA

static inline int
psvita_magic_present(unsigned char *p)
{
	return (!memcmp(p, PSVITA_LABEL_MAGIC, 0x20) && p[510] == MSDOS_LABEL_MAGIC1 && p[511] == MSDOS_LABEL_MAGIC2);
}

static void set_info(struct parsed_partitions *state, int slot)
{
	struct partition_meta_info *info = &state->parts[slot].info;

	snprintf(info->uuid, sizeof(info->uuid), "%02x", slot);
	info->volname[0] = 0;
	state->parts[slot].has_info = true;
}

int psvita_partition(struct parsed_partitions *state)
{
	sector_t sector_size;
	Sector sect;
	unsigned char *data;
	struct psvita_partition *p;
	int slot;

	sector_size = queue_logical_block_size(state->disk->queue) / 512;
	data = read_part_sector(state, 0, &sect);
	if (!data)
		return -1;

	if (!psvita_magic_present(data)) {
		put_dev_sector(sect);
		return 0;
	}

	p = (struct psvita_partition *) (data + 0x50);

	/*
	 * Find the primary partitions.
	 */

	state->next = 5;
	for (slot = 1 ; slot <= 0x10 ; slot++, p++) {
		sector_t start = start_block(p)*sector_size;
		sector_t size = nr_blocks(p)*sector_size;

		if (!size)
			continue;
		if (!p->type)
			continue;

		put_partition(state, slot, start, size);
		set_info(state, slot);
	}

	strlcat(state->pp_buf, "\n", PAGE_SIZE);

	put_dev_sector(sect);
	return 1;
}
