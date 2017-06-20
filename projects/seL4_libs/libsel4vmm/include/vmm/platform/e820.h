/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#ifndef __LIB_VMM_E820_H_
#define __LIB_VMM_E820_H_

/* e820 memory region types. */
#define E820_RAM        1
#define E820_RESERVED   2
#define E820_ACPI       3
#define E820_NVS        4
#define E820_UNUSABLE   5

#define E820_MAX_REGIONS 128 /* E820 supports up to 128. */

#endif /* __LIB_VMM_E820_H_ */

