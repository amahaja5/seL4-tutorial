/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(D61_GPL)
 */

#ifndef __MACHINE_V6_H__
#define __MACHINE_V6_H__

static inline void wfi(void)
{
    asm volatile("mcr p15, 0, %0, c7, c0, 4" : : "r"(0) : "memory");
}

static inline void dsb(void)
{
    asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r"(0) : "memory");
}

static inline void dmb(void)
{
    asm volatile("mcr p15, 0, %0, c7, c10, 5" : : "r"(0) : "memory");
}

static inline void isb(void)
{
    asm volatile("mcr p15, 0, %0, c7, c5, 4" : : "r"(0) : "memory");
}

#endif /* __MACHINE_V6_H__ */
