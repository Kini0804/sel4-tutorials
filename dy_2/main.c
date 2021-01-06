
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

/*
 * seL4 tutorial part 3: IPC between 2 threads
 */

/* Include Kconfig variables. */
#include <autoconf.h>

#include <stdio.h>
#include <assert.h>

#include <sel4/sel4.h>
#include <sel4runtime.h>
#include <sel4runtime/gen_config.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <vka/object.h>
#include <vka/object_capops.h>

#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <vspace/vspace.h>

#include <sel4utils/vspace.h>
#include <sel4utils/mapping.h>

#include <utils/arith.h>
#include <utils/zf_log.h>
#include <sel4utils/sel4_zf_logif.h>

#include <sel4platsupport/bootinfo.h>

/* constants */
#define IPCBUF_FRAME_SIZE_BITS 12 // use a 4K frame for the IPC buffer
#define IPCBUF_VADDR 0x7000000    // arbitrary (but free) address for IPC buffer

#define EP_BADGE 0x61   // arbitrary (but unique) number for a badge
#define MSG_DATA 0x6161 // arbitrary data to send

/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;

/* variables shared with second thread */
vka_object_t ep_object;
cspacepath_t ep_cap_path;

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE (BIT(seL4_PageBits) * 10)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* stack for the new thread */
#define THREAD_2_STACK_SIZE 512
static uint64_t thread_2_stack[THREAD_2_STACK_SIZE];

/* tls region for the new thread */
static char tls_region[CONFIG_SEL4RUNTIME_STATIC_TLS] = {};

/* convenience function */
extern void name_thread(seL4_CPtr tcb, char *name);

/* function to run in the new thread */
void thread_2(void)
{
    seL4_Word sender_badge = 0;
    UNUSED seL4_MessageInfo_t tag;
    seL4_Word msg = 0;
    printf("thread_2: hallo wereld\n");
    /* TASK 11: wait for a message to come in over the endpoint */
    tag = seL4_Recv(ep_object.cptr, &sender_badge);
    /* TASK 12: make sure it is what we expected */
    ZF_LOGF_IF(sender_badge != EP_BADGE,
               "badge error!\n");
    ZF_LOGF_IF(seL4_MessageInfo_get_length(tag) != 1,
               "length error!\n");
    /* TASK 13: get the message stored in the first message register */
    msg = seL4_GetMR(0);
    printf("thread_2: got a message %#" PRIxPTR " from %#" PRIxPTR "\n", msg, sender_badge);
    /* modify the message */
    msg = ~msg;
    /* TASK 14: copy the modified message back into the message register */
    seL4_SetMR(0, msg);
    /* TASK 15: send the message back */
    seL4_ReplyRecv(ep_object.cptr, tag, &sender_badge);
}

int main(void)
{
    UNUSED int error;

    /* get boot info */
    info = platsupport_get_bootinfo();
    ZF_LOGF_IF(info == NULL, "Failed to get bootinfo.");
    /* Set up logging and give us a name: useful for debugging if the thread faults */
    zf_log_set_tag_prefix("dynamic-2:");
    name_thread(seL4_CapInitThreadTCB, "dynamic-2");

    /* init simple */
    simple_default_init_bootinfo(&simple, info);

    /* print out bootinfo and other info about simple */
    simple_print(&simple);

    /* create an allocator */
    allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE, allocator_mem_pool);
    ZF_LOGF_IF(allocman == NULL, "Failed to initialize alloc manager.\n"
                                 "\tMemory pool sufficiently sized?\n"
                                 "\tMemory pool pointer valid?\n");

    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&vka, allocman);

    /* get our cspace root cnode */
    seL4_CPtr cspace_cap;
    cspace_cap = simple_get_cnode(&simple);

    /* get our vspace root page directory */
    seL4_CPtr pd_cap;
    pd_cap = simple_get_pd(&simple);

    /* create a new TCB */
    vka_object_t tcb_object = {0};
    error = vka_alloc_tcb(&vka, &tcb_object);
    ZF_LOGF_IFERR(error, "Failed to allocate new TCB.\n"
                         "\tVKA given sufficient bootstrap memory?");
    /* TASK 1: get a frame cap for the ipc buffer */
    vka_object_t ipc_frame_object;
    vka_alloc_frame(&vka, IPCBUF_FRAME_SIZE_BITS, &ipc_frame_object);
    seL4_Word ipc_buffer_vaddr = IPCBUF_VADDR;
    /* TASK 2: try to map the frame the first time  */
    error = seL4_ARCH_Page_Map(ipc_frame_object.cptr, pd_cap, ipc_buffer_vaddr, seL4_AllRights, seL4_ARCH_Default_VMAttributes);
    if (error != 0)
    {
        /* TASK 3: create a page table */
        vka_object_t pt_object;
        error = vka_alloc_page_table(&vka, &pt_object);
        ZF_LOGF_IFERR(error, "Failed to allocate new page table.\n");
        /* TASK 4: map the page table */
        error = seL4_ARCH_PageTable_Map(pt_object.cptr, pd_cap, ipc_buffer_vaddr, seL4_ARCH_Default_VMAttributes);
        ZF_LOGF_IFERR(error, "Failed to map page table into VSpace.\n"
                             "\tWe are inserting a new page table into the top-level table.\n"
                             "\tPass a capability to the new page table, and not for example, the IPC buffer frame vaddr.\n")
        /* TASK 5: then map the frame in */
        error = seL4_ARCH_Page_Map(ipc_frame_object.cptr, pd_cap, ipc_buffer_vaddr, seL4_AllRights, seL4_ARCH_Default_VMAttributes);
        ZF_LOGF_IFERR(error, "Failed again to map the IPC buffer frame into the VSpace.\n"
                             "\t(It's not supposed to fail.)\n"
                             "\tPass a capability to the IPC buffer's physical frame.\n"
                             "\tRevisit the first seL4_ARCH_Page_Map call above and double-check your arguments.\n");
    }

    /* TASK 6: create an endpoint */
    vka_alloc_endpoint(&vka, &ep_object);
    ZF_LOGF_IFERR(error, "Failed to allocate new endpoint object.\n");
    /* TASK 7: make a badged copy of it in our cspace. */
    error = vka_mint_object(&vka, &ep_object, &ep_cap_path, seL4_AllRights, EP_BADGE);
    ZF_LOGF_IFERR(error, "Failed to mint new badged copy of IPC endpoint.\n"
                         "\tseL4_Mint is the backend for vka_mint_object.\n"
                         "\tseL4_Mint is simply being used here to create a badged copy of the same IPC endpoint.\n"
                         "\tThink of a badge in this case as an IPC context cookie.\n");
    /* initialise the new TCB */
    error = seL4_TCB_Configure(tcb_object.cptr, seL4_CapNull,
                               cspace_cap, seL4_NilData, pd_cap, seL4_NilData,
                               ipc_buffer_vaddr, ipc_frame_object.cptr);
    ZF_LOGF_IFERR(error, "Failed to configure the new TCB object.\n"
                         "\tWe're running the new thread with the root thread's CSpace.\n"
                         "\tWe're running the new thread in the root thread's VSpace.\n");
    /* give the new thread a name */
    name_thread(tcb_object.cptr, "dynamic-2: thread_2");

    /* set start up registers for the new thread */
    seL4_UserContext regs = {0};
    size_t regs_size = sizeof(seL4_UserContext) / sizeof(seL4_Word);

    /* set instruction pointer where the thread shoud start running */
    sel4utils_set_instruction_pointer(&regs, (seL4_Word)thread_2);

    /* check that stack is aligned correctly */
    const int stack_alignment_requirement = sizeof(seL4_Word) * 2;
    uintptr_t thread_2_stack_top = (uintptr_t)thread_2_stack + sizeof(thread_2_stack);

    ZF_LOGF_IF(thread_2_stack_top % (stack_alignment_requirement) != 0,
               "Stack top isn't aligned correctly to a %dB boundary.\n"
               "\tDouble check to ensure you're not trampling.",
               stack_alignment_requirement);

    /* set stack pointer for the new thread. remember the stack grows down */
    sel4utils_set_stack_pointer(&regs, thread_2_stack_top);

    /* actually write the TCB registers. */
    error = seL4_TCB_WriteRegisters(tcb_object.cptr, 0, 0, regs_size, &regs);
    ZF_LOGF_IFERR(error, "Failed to write the new thread's register set.\n"
                         "\tDid you write the correct number of registers? See arg4.\n");

    /* create a thread local storage (TLS) region for the new thread to store the
      ipc buffer pointer */
    uintptr_t tls = sel4runtime_write_tls_image(tls_region);
    seL4_IPCBuffer *ipcbuf = (seL4_IPCBuffer *)ipc_buffer_vaddr;
    error = sel4runtime_set_tls_variable(tls, __sel4_ipc_buffer, ipcbuf);
    ZF_LOGF_IF(error, "Failed to set ipc buffer in TLS of new thread");
    /* set the TLS base of the new thread */
    error = seL4_TCB_SetTLSBase(tcb_object.cptr, tls);
    ZF_LOGF_IF(error, "Failed to set TLS base");

    /* start the new thread running */
    error = seL4_TCB_Resume(tcb_object.cptr);
    ZF_LOGF_IFERR(error, "Failed to start new thread.\n");

    /* we are done, say hello */
    printf("main: hello world\n");
    /*
     * now send a message to the new thread, and wait for a reply
     */
    seL4_Word msg = 0;
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    /* TASK 8: set the data to send. We send it in the first message register */
    tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, MSG_DATA);

    /* TASK 9: send and wait for a reply. */
    tag = seL4_Call(ep_cap_path.capPtr, tag);

    /* TASK 10: get the reply message */
    msg = seL4_GetMR(0);

    /* check that we got the expected repy */
    ZF_LOGF_IF(seL4_MessageInfo_get_length(tag) != 1,
               "Response data from thread_2 was not the length expected.\n"
               "\tHow many registers did you set with seL4_SetMR within thread_2?\n");

    ZF_LOGF_IF(msg != ~MSG_DATA,
               "Response data from thread_2's content was not what was expected.\n");

    printf("main: got a reply: %#" PRIxPTR "\n", msg);

    return 0;
}
