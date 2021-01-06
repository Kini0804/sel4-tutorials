
#include <stdio.h>
#include <sel4/sel4.h>
#include <utils/attribute.h>

#define PROGNAME        "Faulter: "
extern seL4_CPtr sequencing_ep_cap;

extern seL4_CPtr local_badged_faulter_fault_ep_empty_cap;
extern seL4_CPtr faulter_empty_cap;

static void
touch(seL4_CPtr slot)
{
    UNUSED seL4_Word unused_badge;

    seL4_NBRecv(slot, &unused_badge);
}

int main(void)
{
    UNUSED seL4_Word tmp_badge;

    /* First tell the handler where in our CSpace it is safe for them to
     * place a badged fault handler EP cap so that it can receive fault IPC
     * messages on our behalf.
     */
    printf(PROGNAME "Running. About to send empty slot CPtr to handler.\n");
    seL4_SetMR(0, local_badged_faulter_fault_ep_empty_cap);
    seL4_Call(sequencing_ep_cap, seL4_MessageInfo_new(0, 0, 0, 1));

    printf(PROGNAME "Handler has minted fault EP into our cspace. Proceeding.\n");

    printf(PROGNAME "About to touch fault vaddr.\n");
    touch(faulter_empty_cap);

    printf(PROGNAME "Successfully executed past the fault.\n"
           PROGNAME "Finished execution.\n");

    return 0;
}