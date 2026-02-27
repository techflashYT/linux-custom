/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PSVITA_PARTITION_H
#define _LINUX_PSVITA_PARTITION_H

struct psvita_partition {
	__le32 offset;		/* partition offset in blocks */
	__le32 size;		/* partition size in blocks */
	u8 code;		/* partition code */
	u8 type;		/* partition type */
	u8 active;		/* partition active */
	__le32 flags;		/* partition flags */
	u16 unk;		/* ?? */
} __packed;

enum psvita_partition_type {
	PSVITA_TYPE_FAT16 = 0x6,
	PSVITA_TYPE_EXFAT = 0x7,
	PSVITA_TYPE_RAW_DATA = 0xDA
};

#endif /* LINUX_PSVITA_PARTITION_H */
