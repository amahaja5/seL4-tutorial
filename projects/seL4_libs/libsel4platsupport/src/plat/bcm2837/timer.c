/*
 * Copyright 2016, Data61 CSIRO
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#include <vka/vka.h>
#include <vspace/vspace.h>
#include <simple/simple.h>
#include <sel4platsupport/timer.h>
#include <platsupport/plat/timer.h>
#include <utils/util.h>
#include <stdlib.h>
#include "../../timer_common.h"

int sel4platsupport_plat_init_default_timer_caps(UNUSED vka_t *vka, UNUSED vspace_t *vspace, UNUSED simple_t *simple, UNUSED timer_objects_t *timer_objects)
{
    return 0;
}

seL4_timer_t *
sel4platsupport_get_timer(enum timer_id id, vka_t *vka, vspace_t *vspace,
                          simple_t *simple, seL4_CPtr notification)
{
    seL4_timer_t *timer = timer_common_init(vspace, simple, vka, notification,
                             bcm_timer_irqs[id],
                             (void*)bcm_timer_paddrs[id]);
    if (timer == NULL) {
        return NULL;
    }

    /* do hardware init */
    timer_config_t config = {
        .vaddr = timer->vaddr,
    };

    timer->timer = ps_get_timer(id, &config);
    if (timer->timer == NULL) {
        timer_common_destroy(timer, vka, vspace);
        return NULL;
    }

    /* success */
    return timer;

}

seL4_timer_t *
sel4platsupport_get_default_timer(vka_t *vka, vspace_t *vspace, simple_t *simple,
                                  seL4_CPtr notification)
{
    return sel4platsupport_get_timer(TMR_DEFAULT, vka, vspace, simple, notification);
}
