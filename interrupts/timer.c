

#include <stdio.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <timer_driver/driver.h>

// CSlots pre-initialised in this CSpace
extern seL4_CPtr endpoint;
// capability to a reply object
extern seL4_CPtr ntfn;
// capability to the device untyped for the timer

extern seL4_CPtr device_untyped;
// empty cslot for the frame
extern seL4_CPtr timer_frame;
// cnode of this process
extern seL4_CPtr cnode;
// vspace of this process
extern seL4_CPtr vspace;
// frame to map the timer to
extern seL4_CPtr frame;
extern const char timer_vaddr[4096];
// irq control capability
extern seL4_CPtr irq_control;
// empty slot for the irq
extern seL4_CPtr irq_handler;

/* constants */
#define EP_BADGE 61     // arbitrary (but unique) number for a badge
#define MSG_DATA 0x6161 // arbitrary data to send

#define DEFAULT_TIMER_ID 0
#define TTC0_TIMER1_IRQ 42

int main(void)
{
    /* wait for a message */
    seL4_Word sender_badge;
    seL4_MessageInfo_t tag = seL4_Recv(endpoint, &sender_badge);

    /* make sure the message is what we expected */
    assert(sender_badge == EP_BADGE);
    assert(seL4_MessageInfo_get_length(tag) == 1);

    /* get the message stored in the first message register */
    seL4_Word msg = seL4_GetMR(0);
    printf("timer: got a message from %u to sleep %zu seconds\n", sender_badge, msg);

    /* retype the device untyped into a frame */
    seL4_Error error = seL4_Untyped_Retype(device_untyped, seL4_ARM_SmallPageObject, 0,
                                           cnode, 0, 0, timer_frame, 1);
    ZF_LOGF_IF(error, "Failed to retype device untyped");

    /* unmap the existing frame mapped at vaddr so we can map the timer here */
    error = seL4_ARM_Page_Unmap(frame);
    ZF_LOGF_IF(error, "Failed to unmap frame");

    /* map the device frame into the address space */
    error = seL4_ARM_Page_Map(timer_frame, vspace, (seL4_Word)timer_vaddr, seL4_AllRights, 0);
    ZF_LOGF_IF(error, "Failed to map device frame");

    timer_drv_t timer_drv = {0};

    /* TODO invoke irq_control to put the interrupt for TTC0_TIMER1_IRQ in
       cslot irq_handler (depth is seL4_WordBits) */
    error = seL4_IRQControl_Get(irq_control, TTC0_TIMER1_IRQ, cnode, irq_handler, seL4_WordBits);
    ZF_LOGF_IF(error, "Failed");
    /* TODO set ntfn as the notification for irq_handler */
    error = seL4_IRQHandler_SetNotification(irq_handler, ntfn);
    ZF_LOGF_IF(error, "Failed to set notification");
    //  error = seL4_IRQControl_Get(seL4_IRQControl, 7, cspace_root, 10, seL4_WordBits);
    /* set up the timer driver */
    int timer_err = timer_init(&timer_drv, DEFAULT_TIMER_ID, (void *)timer_vaddr);
    ZF_LOGF_IF(timer_err, "Failed to init timer");

    timer_err = timer_start(&timer_drv);
    ZF_LOGF_IF(timer_err, "Failed to start timer");

    /* ack the irq in case of any pending interrupts int the driver */
    error = seL4_IRQHandler_Ack(irq_handler);
    ZF_LOGF_IF(error, "Failed to ack irq");

    timer_err = timer_set_timeout(&timer_drv, NS_IN_MS, true);
    ZF_LOGF_IF(timer_err, "Failed to set timeout");

    int count = 0;
    while (1)
    {
        /* Handle the timer interrupt */
        seL4_Word badge;
        seL4_Wait(ntfn, &badge);
        timer_handle_irq(&timer_drv);
        if (count == 0)
        {
            printf("Tick\n");
        }

        /* TODO ack the interrupt */
        error = seL4_IRQHandler_Ack(irq_handler);
        ZF_LOGF_IF(error, "Failed");
        count++;
        if (count == 1000 * msg)
        {
            break;
        }
    }

    // stop the timer
    timer_stop(&timer_drv);

    /* modify the message */
    seL4_SetMR(0, 0);

    /* send the modified message back */
    seL4_ReplyRecv(endpoint, tag, &sender_badge);

    return 0;
}