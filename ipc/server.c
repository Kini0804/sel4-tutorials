

#include <assert.h>
#include <sel4/sel4.h>
#include <stdio.h>
#include <utils/util.h>
#include <timer_driver/driver.h>

// cslot containing IPC endpoint capability
extern seL4_CPtr endpoint;
// cslot containing a capability to the cnode of the server
extern seL4_CPtr cnode;
// empty cslot
extern seL4_CPtr free_slot;

int main(int c, char *argv[])
{
     seL4_Word sender;
     seL4_MessageInfo_t info = seL4_Recv(endpoint, &sender);
     error = ltimer_default_init(&timer, ops, NULL, NULL);
     assert(error == 0);
     while (1)
     {
          seL4_Error error;
          if (sender == 0)
          {
               /* No badge! give this sender a badged copy of the endpoint */
               seL4_Word badge = seL4_GetMR(0);
               seL4_Error error = seL4_CNode_Mint(cnode, free_slot, seL4_WordBits,
                                                  cnode, endpoint, seL4_WordBits,
                                                  seL4_AllRights, badge);
               printf("Badged %lu\n", badge);
               // TODO use cap transfer to send the badged cap in the reply
               info = seL4_MessageInfo_new(0, 0, 1, 0);
               seL4_SetCap(0, free_slot);
               /* reply to the sender and wait for the next message */
               seL4_Reply(info);
               /* now delete the transferred cap */
               error = seL4_CNode_Delete(cnode, free_slot, seL4_WordBits);
               assert(error == seL4_NoError);
               /* wait for the next message */
               info = seL4_Recv(endpoint, &sender);
          }
          else
          {
               // TODO use printf to print out the message sent by the client
               // followed by a new line
               seL4_CNode_SaveCaller(cnode, free_slot, seL4_WordBits);
               info = seL4_Recv(endpoint, &sender);
               for (int i = 0; i < seL4_MessageInfo_get_length(info); i++)
               {
                    printf("%c", (char)seL4_GetMR(i));
               }
               printf("\n");
               seL4_Send(free_slot, seL4_MessageInfo_new(0, 0, 0, 0));
          }
     }
     return 0;
}

static void *thread1_fn(void *arg)
{
     l4_msgtag_t tag;
     int ipc_error;
     unsigned long value = 1;
     (void)arg;
     while (1)
     {
          printf(”Sending
                 : % ld\n”, value);
          /* Store the value which we want to have squared in the first message
       * register of our UTCB. */
          l4_utcb_mr()->mr[0] = value;
          /* To an L4 IPC call, i.e. send a message to thread2 and wait for a
       * reply from thread2. The '1' in the msgtag denotes that we want to
       * transfer one word of our message registers (i.e. MR0). No timeout. */
          l4_calibrate_tsc(l4re_kip());
          l4_cpu_time_t e = l4_rdtsc();
          printf(”the CPU - internal start - time
                 : % lld \n”, e);
          tag = l4_ipc_call(pthread_l4_cap(t2), l4_utcb(),
                            l4_msgtag(0, 1, 0, 0), L4_IPC_NEVER);
          l4_cpu_time_t e2 = l4_rdtsc();
          printf(”the CPU - internal end - time % lld\n”, e2);
          printf(”the CPU - internal time % lld\n”, e2 - e);
          /* Check for IPC error, if yes, print out the IPC error code, if not,
          * print the received result. */
          ipc_error = l4_ipc_error(tag, l4_utcb());
          if (ipc_error)
               fprintf(stderr, ”thread1
                       : IPC error
                       : % x\n”, ipc_error);
          else
               printf(”Received
                      : % ld\n”, l4_utcb_mr()->mr[0]);
          /* Wait some time and increment our value. */
          sleep(1);
          value++;
     }
     return NULL;
}

static void *thread2_fn(void *arg)
{
     l4_msgtag_t tag;
     l4_umword_t label;
     int ipc_error;
     (void)arg;
     /* Wait for requests from any thread. No timeout, i.e. wait forever. */
     tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
     while (1)
     {
          /* Check if we had any IPC failure, if yes, print the error code
       * and just wait again. */
          ipc_error = l4_ipc_error(tag, l4_utcb());
          if (ipc_error)
          {
               fprintf(stderr, ”thread2
                       : IPC error
                       : % x\n”, ipc_error);
               tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
               continue;
          }
          /* So, the IPC was ok, now take the value out of message register 0
       * of the UTCB and store the square of it back to it. */
          l4_utcb_mr()->mr[0] = 5308;
          /* Send the reply and wait again for new messages.
   * The '1' in the msgtag indicated that we want to transfer 1 word in
   * the message registers (i.e. MR0) */
          Empty();
          tag = l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 1, 0, 0),
                                      &label, L4_IPC_NEVER);
     }
     return NULL;
}

int main(void)
{
     // We will have two threads, one is already running the main function, the
     // other (thread2) will be created using pthread_create.
     if (pthread_create(&t2, NULL, thread2_fn, NULL))
     {
          fprintf(stderr, ”Thread creation failed\n”);
          return 1;
     }
     // Just run thread1 in the main thread
     thread1_fn(NULL);
     return 0;
}
