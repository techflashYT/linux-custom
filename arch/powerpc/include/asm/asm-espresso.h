#ifndef _ASM_POWERPC_ASM_ESPRESSO_H
#define _ASM_POWERPC_ASM_ESPRESSO_H

#include <asm/asm-const.h>

#ifdef __KERNEL__

/* The Espresso has an undocumented erratum where every stwcx needs a dcbst immediately before it.
 */
#ifdef CONFIG_ESPRESSO_ERRATA
#define PPCESPRESSO_ERRATA(ra, rb)	stringify_in_c(dcbst ra, rb;)
#else
#define PPCESPRESSO_ERRATA(ra, rb)
#endif

#endif /* __KERNEL__ */

#endif /* _ASM_POWERPC_ASM_ESPRESSO_H */
