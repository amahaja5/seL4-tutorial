/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>

#if defined CONFIG_LIB_SEL4_VKA && defined CONFIG_LIB_SEL4_VSPACE

#include <sel4utils/client_server_vspace.h>
#include <vka/capops.h>
#include <sel4utils/mapping.h>
#include <sel4utils/util.h>
#include <stdlib.h>
#include <sel4utils/vspace.h>
#include <sel4utils/vspace_internal.h>

#define OFFSET(x) ( ((uintptr_t)((seL4_Word)(x))) & MASK(PAGE_BITS_4K) )
#define ADDR(start, page) ( ((uintptr_t) start) + (page) * PAGE_SIZE_4K )

typedef struct client_server_vspace {
    /* vspace of the server. we also use this to request memory for metadata */
    vspace_t *server;
    /* vspace of the client we are proxying calls for */
    vspace_t *client;
    vka_t *vka;
    /* a fake sel4utils vspace that acts as a translation layer */
    sel4utils_alloc_data_t translation_data;
    vspace_t translation;
} client_server_vspace_t;

static void unmap(client_server_vspace_t *cs_vspace, void *caddr, size_t size_bits)
{
    /* determine the virtual address of the start */
    void *saddr = (void*)get_cap(cs_vspace->translation_data.top_level, (uintptr_t) caddr);
    assert(saddr);
    /* get the cap from the server vspace */
    seL4_CPtr cap = vspace_get_cap(cs_vspace->server, saddr);
    assert(cap);
    /* unmap */
    vspace_unmap_pages(cs_vspace->server, saddr, 1, size_bits, VSPACE_PRESERVE);
    /* delete and free the cap */
    cspacepath_t cap_path;
    vka_cspace_make_path(cs_vspace->vka, cap, &cap_path);
    vka_cnode_delete(&cap_path);
    vka_cspace_free(cs_vspace->vka, cap);
    /* remove our translation */
    clear_entries(&cs_vspace->translation, (uintptr_t) caddr, size_bits);
}

static void unmap_many(client_server_vspace_t *cs_vspace, void *caddr, size_t num_pages, size_t size_bits)
{
    for (int i = 0; i < num_pages; i++) {
        unmap(cs_vspace, caddr + BIT(size_bits) * i, size_bits);
    }
}

static int dup_and_map(client_server_vspace_t *cs_vspace, seL4_CPtr cap, void *caddr, size_t size_bits)
{
    /* duplicate the cap */
    seL4_CPtr server_cap;
    cspacepath_t cap_path, server_cap_path;
    int error = vka_cspace_alloc(cs_vspace->vka, &server_cap);
    if (error) {
        ZF_LOGE("Failed to allocate cslot");
        return error;
    }
    vka_cspace_make_path(cs_vspace->vka, cap, &cap_path);
    vka_cspace_make_path(cs_vspace->vka, server_cap, &server_cap_path);
    error = vka_cnode_copy(&server_cap_path, &cap_path, seL4_AllRights);
    if (error != seL4_NoError) {
        ZF_LOGE("Error copying frame cap");
        vka_cspace_free(cs_vspace->vka, server_cap);
        return -1;
    }
    /* map it into the server */
    void *saddr = vspace_map_pages(cs_vspace->server, &server_cap_path.capPtr, NULL, seL4_AllRights, 1, size_bits, 1);
    if (!saddr) {
        ZF_LOGE("Error mapping frame into server vspace");
        vka_cnode_delete(&server_cap_path);
        vka_cspace_free(cs_vspace->vka, server_cap);
        return -1;
    }
    /* update our translation */
    uint32_t pages = BYTES_TO_4K_PAGES(BIT(size_bits));
    for (int i = 0; i < pages; i++) {
        if (update_entries(&cs_vspace->translation, ADDR(caddr, i), (seL4_CPtr)ADDR(saddr, i), PAGE_BITS_4K, 0)) {
            /* remove the entries we already put in. we can't call
             * clear_entries because the portion to remove is not
             * a power of two size */
            for (i--; i >= 0; i--) {
                clear_entries(&cs_vspace->translation, ADDR(caddr, i), PAGE_BITS_4K);
            }
            vka_cnode_delete(&server_cap_path);
            vka_cspace_free(cs_vspace->vka, server_cap);
            return -1;
        }
    }
    return 0;
}

static int dup_and_map_many(client_server_vspace_t *cs_vspace, seL4_CPtr *caps, void *caddr, size_t num_pages, size_t size_bits)
{
    for (int i = 0; i < num_pages; i++) {
        int error = dup_and_map(cs_vspace, caps[i], caddr + BIT(size_bits) * i, size_bits);
        if (error) {
            unmap_many(cs_vspace, caddr, i - 1, size_bits);
            return error;
        }
    }
    return 0;
}

static int find_dup_and_map_many(client_server_vspace_t *cs_vspace, void *caddr, size_t num_pages, size_t size_bits)
{
    seL4_CPtr caps[num_pages];
    for (int i = 0; i < num_pages; i++) {
        caps[i] = vspace_get_cap(cs_vspace->client, caddr + BIT(size_bits) * i);
        assert(caps[i]);
    }
    return dup_and_map_many(cs_vspace, caps, caddr, num_pages, size_bits);
}

static reservation_t cs_reserve_range_aligned(vspace_t *vspace, size_t size, size_t size_bits, seL4_CapRights_t rights,
                                              int cacheable, void **result)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    assert(cacheable);
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    /* we are not interested in client reservations, just proxy */
    return vspace_reserve_range_aligned(cs_vspace->client, size, size_bits, rights, cacheable, result);
}

static reservation_t cs_reserve_range_at(vspace_t *vspace, void *vaddr, size_t size, seL4_CapRights_t
                                         rights, int cacheable)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    assert(cacheable);
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    /* we are not interested in client reservations, just proxy */
    return vspace_reserve_range_at(cs_vspace->client, vaddr, size, rights, cacheable);
}

static void cs_free_reservation(vspace_t *vspace, reservation_t reservation)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    vspace_free_reservation(cs_vspace->client, reservation);
}

static int cs_map_pages_at_vaddr(vspace_t *vspace, seL4_CPtr caps[], uintptr_t cookies[], void *vaddr,
                                 size_t num_pages, size_t size_bits, reservation_t reservation)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    assert(size_bits >= 12);
    /* first map all the pages into the client */
    int result = vspace_map_pages_at_vaddr(cs_vspace->client, caps, cookies, vaddr, num_pages, size_bits, reservation);
    if (result) {
        ZF_LOGE("Error mapping pages into client vspace");
        /* some error, propagate up */
        return result;
    }
    /* now map them all into the server */
    result = dup_and_map_many(cs_vspace, caps, vaddr, num_pages, size_bits);
    if (result) {
        /* unmap */
        ZF_LOGE("Error mapping pages into server vspace");
        vspace_unmap_pages(cs_vspace->client, vaddr, num_pages, size_bits, VSPACE_PRESERVE);
        return result;
    }
    return 0;
}

static seL4_CPtr cs_get_cap(vspace_t *vspace, void *vaddr)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    /* return the client cap */
    return vspace_get_cap(cs_vspace->client, vaddr);
}

static seL4_CPtr cs_get_root(vspace_t *vspace)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    return cs_vspace->translation_data.vspace_root;
}

static void cs_unmap_pages(vspace_t *vspace, void *vaddr, size_t num_pages, size_t size_bits, vka_t *vka)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    /* remove our mappings */
    unmap_many(cs_vspace, vaddr, num_pages, size_bits);
    /* unmap from the client */
    vspace_unmap_pages(cs_vspace->client, vaddr, num_pages, size_bits, vka);
}

static int cs_new_pages_at_vaddr(vspace_t *vspace, void *vaddr, size_t num_pages,
                                 size_t size_bits, reservation_t reservation)
{
    client_server_vspace_t *cs_vspace = vspace->data;
    get_alloc_data(cs_vspace->client)->vspace_root = cs_vspace->translation_data.vspace_root;
    assert(size_bits >= 12);
    /* create new pages in the client first */
    int result = vspace_new_pages_at_vaddr(cs_vspace->client, vaddr, num_pages, size_bits, reservation);
    if (result) {
        ZF_LOGE("Error creating pages in client vspace");
        return result;
    }
    result = find_dup_and_map_many(cs_vspace, vaddr, num_pages, size_bits);
    if (result) {
        ZF_LOGE("Error mapping new page in server vspace");
        assert(!"Cannot delete pages we created in client vspace");
        return -1;
    }
    return 0;
}

int sel4utils_get_cs_vspace(vspace_t *vspace, vka_t *vka, vspace_t *server, vspace_t *client)
{
    client_server_vspace_t *cs_vspace = malloc(sizeof(*cs_vspace));
    if (cs_vspace == NULL) {
        ZF_LOGE("Failed to create translation vspace");
        return -1;
    }
    cs_vspace->server = server;
    cs_vspace->client = client;
    cs_vspace->vka = vka;

    int error = sel4utils_get_vspace(server, &cs_vspace->translation, &cs_vspace->translation_data,
                                 vka, 0, NULL, NULL);
    if (error) {
        ZF_LOGE("Failed to create translation vspace");
        free(cs_vspace);
        return error;
    }

    vspace->data = cs_vspace;

    vspace->new_pages_at_vaddr = cs_new_pages_at_vaddr;

    vspace->map_pages_at_vaddr = cs_map_pages_at_vaddr;
    vspace->unmap_pages = cs_unmap_pages;

    vspace->reserve_range_aligned = cs_reserve_range_aligned;
    vspace->reserve_range_at = cs_reserve_range_at;
    vspace->free_reservation = cs_free_reservation;

    vspace->get_cap = cs_get_cap;
    vspace->get_root = cs_get_root;

    vspace->allocated_object = NULL;
    vspace->allocated_object_cookie = NULL;

    return 0;
}

void *sel4utils_cs_vspace_translate(vspace_t *vspace, void *addr)
{
    client_server_vspace_t *cs_vspace = vspace->data;

    seL4_CPtr translation_result = get_cap(cs_vspace->translation_data.top_level, (uintptr_t) addr);

    /* the translation will throw away any non aligned bits, if we got a successful
     * translation then add the offset back onto the result */
    return translation_result == 0 ? NULL : (void *)(translation_result + OFFSET((uintptr_t) addr));
}

int sel4utils_cs_vspace_for_each(vspace_t *vspace, void *addr, uint32_t len,
                                 int (*proc)(void* addr, uint32_t len, void *cookie),
                                 void *cookie)
{
    uintptr_t next_addr;
    uintptr_t end_addr = (uintptr_t)(addr + len);
    for (uintptr_t current_addr = (uintptr_t)addr; current_addr < end_addr; current_addr = next_addr) {
        uintptr_t current_aligned = PAGE_ALIGN_4K(current_addr);
        uintptr_t next_page_start = current_aligned + PAGE_SIZE_4K;
        next_addr = MIN(end_addr, next_page_start);
        uintptr_t saddr = (uintptr_t) sel4utils_cs_vspace_translate(vspace, (void *) current_aligned);
        if (!saddr) {
            return -1;
        }
        int result = proc((void*)(saddr + (current_addr - current_aligned)), next_addr - current_addr, cookie);
        if (result) {
            return result;
        }
    }
    return 0;
}

int sel4utils_cs_vspace_set_root(vspace_t *vspace, seL4_CPtr vspace_root)
{
    assert(vspace);

    client_server_vspace_t *cs_vspace = vspace->data;
    cs_vspace->translation_data.vspace_root = vspace_root;

    return 0;
}

#endif
