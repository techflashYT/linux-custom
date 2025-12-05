// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * arch/powerpc/platforms/xenon/udbg.h
 *
 * Copyright (C) 2025 Techflash <officialTechflashYT@gmail.com>
 */

#ifndef __XENON_UDBG_H
#define __XENON_UDBG_H

#ifdef CONFIG_PPC_EARLY_DEBUG_XENON

extern void __init udbg_init_xenon_virtual(void);

#else

static inline void __init udbg_init_xenon_virtual(void)
{
}

#endif /* CONFIG_PPC_EARLY_DEBUG_XENON */

#endif /* __XENON_UDBG_H */
