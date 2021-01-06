#include <stdio.h>
#include <sel4/sel4.h>
#include <utils/util.h>
#include <sel4utils/util.h>
#include <sel4utils/helpers.h>

// the root CNode of the current thread
extern seL4_CPtr root_cnode;
// VSpace of the current thread
extern seL4_CPtr root_vspace;
// TCB of the current thread
extern seL4_CPtr root_tcb;
// Untyped object large enough to create a new TCB object

extern seL4_CPtr tcb_untyped;
extern seL4_CPtr buf2_frame_cap;
extern const char buf2_frame[4096];

// Empty slot for the new TCB object
extern seL4_CPtr tcb_cap_slot;
// Symbol for the IPC buffer mapping in the VSpace, and capability to the mapping
extern seL4_CPtr tcb_ipc_frame;
extern const char thread_ipc_buff_sym[4096];
// Symbol for the top of a 16 * 4KiB stack mapping, and capability to the mapping
extern const char tcb_stack_base[65536];
static const uintptr_t tcb_stack_top = (const uintptr_t)&tcb_stack_base + sizeof(tcb_stack_base);

int new_thread(void *arg1, void *arg2, void *arg3)
{
    printf("Hello2: arg1 %p, arg2 %p, arg3 %p\n", arg1, arg2, arg3);
    void (*func)(int) = arg1;
    func(*(int *)arg2);
    while (1)
        ;
}

int main(int c, char *arbv[])
{

    printf("Hello, World!\n");

    seL4_DebugDumpScheduler();

    // TODO fix the parameters in this invocation
    seL4_Error result = seL4_Untyped_Retype(tcb_untyped, seL4_TCBObject, seL4_TCBBits, root_cnode, 0, 0, tcb_cap_slot, 1);
    ZF_LOGF_IF(result, "Failed to retype thread: %d", result);
    seL4_DebugDumpScheduler();

    //TODO fix the parameters in this invocation
    result = seL4_TCB_Configure(tcb_cap_slot, seL4_CapNull, root_cnode, 0, root_vspace, 0, (seL4_Word)thread_ipc_buff_sym, tcb_ipc_frame);
    ZF_LOGF_IF(result, "Failed to configure thread: %d", result);

    // TODO fix the call to set priority using the authority of the current thread
    // and change the priority to 254
    result = seL4_TCB_SetPriority(tcb_cap_slot, root_tcb, 254);
    ZF_LOGF_IF(result, "Failed to set the priority for the new TCB object.\n");
    seL4_DebugDumpScheduler();

    seL4_UserContext regs = {0};
    int error = seL4_TCB_ReadRegisters(tcb_cap_slot, 0, 0, sizeof(regs) / sizeof(seL4_Word), &regs);
    ZF_LOGF_IFERR(error, "Failed to read the new thread's register set.\n");

    // TODO use valid instruction pointer
    sel4utils_set_instruction_pointer(&regs, (seL4_Word)new_thread);
    // TODO use valid stack pointer
    sel4utils_set_stack_pointer(&regs, tcb_stack_top);
    // TODO fix parameters to this invocation
    error = seL4_TCB_WriteRegisters(tcb_cap_slot, 0, 0, sizeof(regs) / sizeof(seL4_Word), &regs);
    ZF_LOGF_IFERR(error, "Failed to write the new thread's register set.\n"
                         "\tDid you write the correct number of registers? See arg4.\n");
    seL4_DebugDumpScheduler();

    // TODO resume the new thread
    error = seL4_TCB_Resume(tcb_cap_slot);
    ZF_LOGF_IFERR(error, "Failed to start new thread.\n");
    while (1)
        ;
    return 0;
}