/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(GD_GPL)
 */

#include <config.h>
#include <types.h>
#include <api/failures.h>
#include <api/constants.h>
#include <machine/registerset.h>
#include <object/structures.h>
#include <arch/machine.h>

word_t CONST
Arch_decodeTransfer(word_t flags)
{
    return 0;
}

exception_t CONST
Arch_performTransfer(word_t arch, tcb_t *tcb_src, tcb_t *tcb_dest)
{
    return EXCEPTION_NONE;
}

#if CONFIG_MAX_NUM_NODES > 1
void
Arch_migrateTCB(tcb_t *thread)
{
    /* TODO */
#warning "Arch_migrateTCB is not implemented for ARM"
}
#endif /* CONFIG_MAX_NUM_NODES > 1 */

void
Arch_setTCBIPCBuffer(tcb_t *thread, word_t bufferAddr)
{
#if defined(CONFIG_IPC_BUF_GLOBALS_FRAME)
#elif defined(CONFIG_IPC_BUF_TPIDRURW)
    setRegister(thread, TPIDRURW, bufferAddr);
#else
#error "Unknown IPC buffer strategy"
#endif
}
