/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

#ifndef _VM_VMCALL_INTF
#define _VM_VMCALL_INTF

#include "vmm.h"

typedef int (*vmcall_handler)(vmm_vcpu_t *vcpu);

/*
    A handler for a given vmcall function
     Called in a vmcall exception if its token matches the vcpu register eax
*/
typedef struct vmcall_handler {
    int token;
    vmcall_handler func;
} vmcall_handler_t;

/*
    Simple functions for registering handlers,
        calling a handler
*/
int reg_new_handler(vmm_t *vmm, vmcall_handler func, int token);
int vmm_vmcall_handler(vmm_vcpu_t *vcpu);

/* Handlers that can be registered */
int vchan_handler(vmm_vcpu_t *vcpu);

#endif
