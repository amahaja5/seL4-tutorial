/*
 * Copyright 2015, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*
 * seL4 tutorial part 4: create a new process and IPC with it
 */


/* Include Kconfig variables. */
#include <autoconf.h>

#include <stdio.h>
#include <assert.h>

#include <sel4/sel4.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <vka/object.h>

#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <vspace/vspace.h>

#include <sel4utils/vspace.h>
#include <sel4utils/mapping.h>
#include <sel4utils/process.h>

#include <sel4platsupport/timer.h>
#include <platsupport/plat/timer.h>

/* constants */
#define EP_BADGE 0x61 // arbitrary (but unique) number for a badge
#define MSG_DATA 0x6161 // arbitrary data to send

#define APP_PRIORITY seL4_MaxPrio
#define APP_IMAGE_NAME "hello-timer-client"

/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;
vspace_t vspace;
seL4_timer_t *timer;

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 10)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 100)

/* static memory for virtual memory bootstrapping */
UNUSED static sel4utils_alloc_data_t data;

/* stack for the new thread */
#define THREAD_2_STACK_SIZE 4096
UNUSED static int thread_2_stack[THREAD_2_STACK_SIZE];

/* convenience function */
extern void name_thread(seL4_CPtr tcb, char *name);

int main(void)
{
    UNUSED int error;

    /* give us a name: useful for debugging if the thread faults */
    name_thread(seL4_CapInitThreadTCB, "hello-timer");

    /* get boot info */
    info = seL4_GetBootInfo();

    /* init simple */
    simple_default_init_bootinfo(&simple, info);

    /* print out bootinfo and other info about simple */
    simple_print(&simple);

    /* create an allocator */
    allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE,
        allocator_mem_pool);
    assert(allocman);

    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&vka, allocman);

   /* create a vspace object to manage our vspace */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&vspace,
        &data, simple_get_pd(&simple), &vka, info);

    /* fill the allocator with virtual memory */
    void *vaddr;
    UNUSED reservation_t virtual_reservation;
    virtual_reservation = vspace_reserve_range(&vspace,
        ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
    assert(virtual_reservation.res);
    bootstrap_configure_virtual_pool(allocman, vaddr,
        ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&simple));

   /* use sel4utils to make a new process */
    sel4utils_process_t new_process;
    error = sel4utils_configure_process(&new_process, &vka, &vspace,
        APP_PRIORITY, APP_IMAGE_NAME);
    assert(error == 0);

    /* give the new process's thread a name */
    name_thread(new_process.thread.tcb.cptr, "hello-timer: timer_client");

    /* create an endpoint */
    vka_object_t ep_object = {0};
    error = vka_alloc_endpoint(&vka, &ep_object);
    assert(error == 0);

    /*
     * make a badged endpoint in the new process's cspace.  This copy 
     * will be used to send an IPC to the original cap
     */

    /* make a cspacepath for the new endpoint cap */
    cspacepath_t ep_cap_path;
    seL4_CPtr new_ep_cap;
    vka_cspace_make_path(&vka, ep_object.cptr, &ep_cap_path);

    /* copy the endpont cap and add a badge to the new cap */
    new_ep_cap = sel4utils_mint_cap_to_process(&new_process, ep_cap_path,
        seL4_AllRights, seL4_CapData_Badge_new(EP_BADGE));
    assert(new_ep_cap != 0);

    /* spawn the process */
    error = sel4utils_spawn_process_v(&new_process, &vka, &vspace, 0, NULL, 1);
    assert(error == 0);

    /* TODO 1: create a notification endpoint for the timer interrupt */
    /* hint: vka_alloc_notification()
     * int vka_alloc_notification(vka_t *vka, vka_object_t *result)
     * @param vka Pointer to vka interface.
     * @param result Structure for the Endpoint object.  This gets initialised.
     * @return 0 on success
     * https://github.com/seL4/seL4_libs/blob/3.0.x-compatible/libsel4vka/include/vka/object.h#L110
     */

    /* TODO 2: call sel4platsupport library to get the default timer */
    /* hint: sel4platsupport_get_default_timer
     * seL4_timer_t * sel4platsupport_get_default_timer(vka_t *vka, vspace_t *vspace, simple_t *simple, seL4_CPtr aep);
     * @param vka Pointer to vka interface
     * @param vspace Pointer to vspace interface
     * @param simple Pointer to simple interface
     * @param aep Async endpoint to receive the interrupt
     * @return Pointer to timer structure
     */

    /* we are done, say hello */
    printf("main: hello world\n");

    /*
     * now wait for a message from the new process, then send a reply back
     */

    seL4_Word sender_badge;
    seL4_MessageInfo_t tag;
    seL4_Word msg;

    /* wait for a message */
    tag = seL4_Recv(ep_cap_path.capPtr, &sender_badge);

    /* make sure it is what we expected */
    assert(sender_badge == EP_BADGE);
    assert(seL4_MessageInfo_get_length(tag) == 1);

    /* get the message stored in the first message register */
    msg = seL4_GetMR(0);
    printf("main: got a message from %#x to sleep %u seconds\n", sender_badge, msg);

    seL4_Word sender;

    /*
     * The PIT only has 16-bit count down register, we cannot make it
     * sleep for 1 second, instead, we make PIT to fire an interrupt every
     * one millisecond, and count for 1000 times to make one second.
     */
    int count = 0;
    while(1) {
        /* TODO 3: wait for the timeout */
	/* hint 1: set timeout to 1 millisecond
	 * hint 2: wait for the incoming interrupt
	 * hint 3: handle the interrupt
	 *
	 * int timer_oneshot_relative(pstimer_t* device, uint64_t ns)
	 * @param device generic timer handler
	 * @param ns number of nanoseconds before firing the interrupt
	 * @return 0 on success
	 *
         * void sel4_timer_handle_single_irq(seL4_timer_t* timer);
	 * @param device generic timer handler
	 *
	 * https://github.com/seL4/util_libs/blob/master/libplatsupport/include/platsupport/timer.h#L146
	 */

        count++;
        if (count == 1000 * msg) break;
    }

    /* get the current time */
    msg = timer_get_time(timer->timer);

    /* modify the message */
    seL4_SetMR(0, msg);

    /* send the modified message back */
    seL4_ReplyRecv(ep_cap_path.capPtr, tag, &sender_badge);

    return 0;
}

