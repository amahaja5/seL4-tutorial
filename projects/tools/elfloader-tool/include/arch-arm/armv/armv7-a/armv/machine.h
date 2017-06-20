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

#ifndef __MACHINE_V7_H__
#define __MACHINE_V7_H__

static inline void wfi(void)
{
    asm volatile("wfi" ::: "memory");
}

static inline void dsb(void)
{
    asm volatile("dsb" ::: "memory");
}

static inline void dmb(void)
{
    asm volatile("dmb" ::: "memory");
}

static inline void isb(void)
{
    asm volatile("isb" ::: "memory");
}

#endif /* __MACHINE_V7_H__ */
