
/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
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

#include <utils/arith.h>
#include <utils/zf_log.h>
#include <sel4utils/sel4_zf_logif.h>

#include <sel4platsupport/bootinfo.h>

/* constants */
#define EP_BADGE 0x61   // arbitrary (but unique) number for a badge
#define MSG_DATA 0x6161 // arbitrary data to send

#define APP_PRIORITY seL4_MaxPrio
#define APP_IMAGE_NAME "app"

/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;
vspace_t vspace;

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE (BIT(seL4_PageBits) * 10)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE (BIT(seL4_PageBits) * 100)

/* static memory for virtual memory bootstrapping */
UNUSED static sel4utils_alloc_data_t data;

/* stack for the new thread */
#define THREAD_2_STACK_SIZE 4096
UNUSED static int thread_2_stack[THREAD_2_STACK_SIZE];

int main(void)
{
    UNUSED int error = 0;

    /* get boot info */
    info = platsupport_get_bootinfo();
    ZF_LOGF_IF(info == NULL, "Failed to get bootinfo.");

    /* Set up logging and give us a name: useful for debugging if the thread faults */
    zf_log_set_tag_prefix("dynamic-3:");
    NAME_THREAD(seL4_CapInitThreadTCB, "dynamic-3");

    /* init simple */
    simple_default_init_bootinfo(&simple, info);

    /* print out bootinfo and other info about simple */
    simple_print(&simple);

    /* create an allocator */
    allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE,
                                            allocator_mem_pool);
    ZF_LOGF_IF(allocman == NULL, "Failed to initialize allocator.\n"
                                 "\tMemory pool sufficiently sized?\n"
                                 "\tMemory pool pointer valid?\n");

    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&vka, allocman);

    /* TASK 1: create a vspace object to manage our vspace */
    sel4utils_bootstrap_vspace_with_bootinfo_leaky(&vspace, &data, simple_get_pd(&simple), &vka, info);
    ZF_LOGF_IFERR(error, "Failed to prepare root thread's VSpace for use.\n"
                         "\tsel4utils_bootstrap_vspace_with_bootinfo reserves important vaddresses.\n"
                         "\tIts failure means we can't safely use our vaddrspace.\n");

    /* fill the allocator with virtual memory */
    void *vaddr;
    UNUSED reservation_t virtual_reservation;
    virtual_reservation = vspace_reserve_range(&vspace,
                                               ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
    ZF_LOGF_IF(virtual_reservation.res == NULL, "Failed to reserve a chunk of memory.\n");
    bootstrap_configure_virtual_pool(allocman, vaddr,
                                     ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&simple));

    /* TASK 2: use sel4utils to make a new process */
    sel4utils_process_t new_process;
    sel4utils_process_config_t config = process_config_default_simple(&simple, APP_IMAGE_NAME, APP_PRIORITY);
    error = sel4utils_configure_process_custom(&new_process, &vka, &vspace, config);
    ZF_LOGF_IFERR(error, "Failed to spawn a new thread.\n"
                         "\tsel4utils_configure_process expands an ELF file into our VSpace.\n"
                         "\tBe sure you've properly configured a VSpace manager using sel4utils_bootstrap_vspace_with_bootinfo.\n"
                         "\tBe sure you've passed the correct component name for the new thread!\n");

    /* give the new process's thread a name */
    NAME_THREAD(new_process.thread.tcb.cptr, "dynamic-3: process_2");

    /* create an endpoint */
    vka_object_t ep_object = {0};
    error = vka_alloc_endpoint(&vka, &ep_object);
    ZF_LOGF_IFERR(error, "Failed to allocate new endpoint object.\n");

    /* TASK 3: make a cspacepath for the new endpoint cap */
    cspacepath_t ep_cap_path;
    seL4_CPtr new_ep_cap = 0;
    vka_cspace_make_path(&vka, ep_object.cptr, &ep_cap_path);

    /* TASK 4: copy the endpont cap and add a badge to the new cap */
    new_ep_cap = sel4utils_mint_cap_to_process(&new_process, ep_cap_path, seL4_AllRights, EP_BADGE);

    ZF_LOGF_IF(new_ep_cap == 0, "Failed to mint a badged copy of the IPC endpoint into the new thread's CSpace.\n"
                                "\tsel4utils_mint_cap_to_process takes a cspacepath_t: double check what you passed.\n");

    printf("NEW CAP SLOT: %" PRIxPTR ".\n", ep_cap_path.capPtr);

    /* TASK 5: spawn the process */
    seL4_Word argc = 1;
    char string_args[argc][WORD_STRING_SIZE];
    char *argv[argc];
    sel4utils_create_word_args(string_args, argv, argc, new_ep_cap);
    error = sel4utils_spawn_process_v(&new_process, &vka, &vspace, argc, (char **)&argv, 1);
    ZF_LOGF_IFERR(error, "Failed to spawn and start the new thread.\n"
                         "\tVerify: the new thread is being executed in the root thread's VSpace.\n"
                         "\tIn this case, the CSpaces are different, but the VSpaces are the same.\n"
                         "\tDouble check your vspace_t argument.\n");

    /* we are done, say hello */
    printf("main: hello world\n");
    seL4_Word sender_badge = 0;
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Word msg;
    /* TASK 6: wait for a message */
    tag = seL4_Recv(ep_cap_path.capPtr, &sender_badge);
    /* make sure it is what we expected */
    ZF_LOGF_IF(sender_badge != EP_BADGE,
               "The badge we received from the new thread didn't match our expectation.\n");

    ZF_LOGF_IF(seL4_MessageInfo_get_length(tag) != 1,
               "Response data from the new process was not the length expected.\n"
               "\tHow many registers did you set with seL4_SetMR within the new process?\n");
    /* get the message stored in the first message register */
    msg = seL4_GetMR(0);
    printf("main: got a message %#" PRIxPTR " from %#" PRIxPTR "\n", msg, sender_badge);
    /* modify the message */
    seL4_SetMR(0, ~msg);
    /* TASK 7: send the modified message back */
    seL4_ReplyRecv(ep_cap_path.capPtr, tag, &sender_badge);
    return 0;
}