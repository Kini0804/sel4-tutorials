

# seL4 Dynamic Libraries: IPC

The tutorial is designed to
teach the basics of seL4 IPC using Endpoint objects, and userspace
paging management. You'll be led through the process of creating a new
thread (retyping an untyped object into a TCB object), and also made to
manually do your own memory management to allocate some virtual memory
for use as the shared memory buffer between your two threads.

Don't gloss over the globals declared before `main()` -- they're declared
for your benefit so you can grasp some of the basic data structures.

You'll observe that the things you've already covered in the second
tutorial are filled out and you don't have to repeat them: in much the
same way, we won't be repeating conceptual explanations on this page, if
they were covered by a previous tutorial in the series.

## Learning outcomes

- Repeat the spawning of a thread. "''If it's nice, do it twice''"
        -- Caribbean folk-saying. Once again, the new thread will be
        sharing its creator's VSpace and CSpace.
- Introduction to the idea of badges, and minting badged copies of
        an Endpoint capability. NB: you don't mint a new copy of the
        Endpoint object, but a copy of the capability that
        references it.
- Basic IPC: sending and receiving: how to make two
        threads communicate.
- IPC Message format, and Message Registers. seL4 binds some of
        the first Message Registers to hardware registers, and the rest
        are transferred in the IPC buffer.
- Understand that each thread has only one IPC buffer, which is
        pointed to by its TCB. It's possible to make a thread wait on
        both an Endpoint and a Notification using "Bound Notifications".
- Understand CSpace pointers, which are really just integers with
        multiple indexes concatenated into one. Understanding them well
        however, is important to understanding how capabilities work. Be
        sure you understand the diagram on the "**CSpace example and
        addressing**" slide.

## Initialising

```sh
# For instructions about obtaining the tutorial sources see https://docs.sel4.systems/Tutorials/#get-the-code
#
# Follow these instructions to initialise the tutorial
# initialising the build directory with a tutorial exercise
./init --tut dynamic-2
# building the tutorial exercise
cd dynamic-2_build
ninja
```



## Prerequisites

1. [Set up your machine](https://docs.sel4.systems/HostDependencies).
1. [dynamic-1](https://docs.sel4.systems/Tutorials/dynamic-1)

## Exercises

When you first run this tutorial, you will see a fault as follows:
```
Booting all finished, dropped to user space
Caught cap fault in send phase at address (nil)
while trying to handle:
vm fault on data at address 0x70003c8 with status 0x6
in thread 0xffffff801ffb5400 "rootserver" at address 0x402977
With stack:
0x43df70: 0x0
0x43df78: 0x0
```

### Allocate an IPC buffer

As we mentioned in passing before, threads in seL4 do their own memory
management. You implement your own Virtual Memory Manager, essentially.
To the extent that you must allocate a physical memory frame, then map
it into your thread's page directory -- and even manually allocate page
tables on your own, where necessary.

Here's the first step in a conventional memory manager's process of
allocating memory: first allocate a physical frame. As you would expect,
you cannot directly write to or read from this frame object since it is
not mapped into any virtual address space as yet. Standard restrictions
of a MMU-utilizing kernel apply.

- <https://github.com/seL4/seL4_libs/blob/master/libsel4vka/include/vka/object.h>

```

    /* TASK 1: get a frame cap for the ipc buffer */
    /* hint: vka_alloc_frame()
     * int vka_alloc_frame(vka_t *vka, uint32_t size_bits, vka_object_t *result)
     * @param vka Pointer to vka interface.
     * @param size_bits Frame size: 2^size_bits
     * @param result Structure for the Frame object.  This gets initialised.
     * @return 0 on success
     */
    vka_object_t ipc_frame_object;
```

On completion, the output will not change.

### Try to map a page

Take note of the line of code that precedes this: the one where a
virtual address is randomly chosen for use. This is because, as we
explained before, the process is responsible for its own Virtual Memory
Management. As such, if it chooses, it can map any page in its VSpace to
physical frame. It can technically choose to do unconventional things,
like not unmap PFN #0. The control of how the address space is managed
is up to the threads that have write-capabilities to that address space.
There is both flexibility and responsibility implied here. Granted, seL4
itself provides strong guarantees of isolation even if a thread decides
to go rogue.

Attempt to map the frame you allocated earlier, into your VSpace. A keen
reader will pick up on the fact that it's unlikely that this will work,
since you'd need a new page table to contain the new page-table-entry.
The tutorial deliberately walks you through both the mapping of a frame
into a VSpace, and the mapping of a new page-table into a VSpace.

- <https://github.com/seL4/seL4_libs/blob/master/libsel4vspace/arch_include/x86/vspace/arch/page.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/arch_include/x86/interfaces/sel4arch.xml>

```

    /* TASK 2: try to map the frame the first time  */
    /* hint 1: seL4_ARCH_Page_Map()
     * The *ARCH* versions of seL4 sys calls are abstractions over the architecture provided by libsel4utils
     * this one is defined as:
     * #define seL4_ARCH_Page_Map seL4_X86_Page_Map
     * The signature for the underlying function is:
     * int seL4_X86_Page_Map(seL4_X86_Page service, seL4_X86_PageDirectory pd, seL4_Word vaddr, seL4_CapRights rights, seL4_X86_VMAttributes attr)
     * @param service Capability to the page to map.
     * @param pd Capability to the VSpace which will contain the mapping.
     * @param vaddr Virtual address to map the page into.
     * @param rights Rights for the mapping.
     * @param attr VM Attributes for the mapping.
     * @return 0 on success.
     *
     * Note: this function is generated during build.  It is generated from the following definition:
     *
     * hint 2: for the rights, use seL4_AllRights
     * hint 3: for VM attributes use seL4_ARCH_Default_VMAttributes
     * Hint 4: It is normal for this function call to fail. That means there are
     *    no page tables with free slots -- proceed to the next step where you'll
     *    be led to allocate a new empty page table and map it into the VSpace,
     *    before trying again.
     */
```

On completion, the output will be as follows:
```
dynamic-2: main@main.c:260 [Err seL4_FailedLookup]:
	Failed to allocate new page table.
```

### Allocate a page table

So just as you previously had to manually retype a new frame to use for
your IPC buffer, you're also going to have to manually retype a new
page-table object to use as a leaf page-table in your VSpace.

- <https://github.com/seL4/seL4_libs/blob/master/libsel4vka/include/vka/object.h>

```

        /* TASK 3: create a page table */
        /* hint: vka_alloc_page_table()
         * int vka_alloc_page_table(vka_t *vka, vka_object_t *result)
         * @param vka Pointer to vka interface.
         * @param result Structure for the PageTable object.  This gets initialised.
         * @return 0 on success
         */
 ```
On completion, you will see another fault.

### Map a page table

If you successfully retyped a new page table from an untyped memory
object, you can now map that new page table into your VSpace, and then
try again to finally map the IPC-buffer's frame object into the VSpace.

- <https://github.com/seL4/seL4_libs/blob/master/libsel4vspace/arch_include/x86/vspace/arch/page.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/arch_include/x86/interfaces/sel4arch.xml>

```

        /* TASK 4: map the page table */
        /* hint 1: seL4_ARCH_PageTable_Map()
         * The *ARCH* versions of seL4 sys calls are abstractions over the architecture provided by libsel4utils
         * this one is defined as:
         * #define seL4_ARCH_PageTable_Map seL4_X86_PageTable_Map
         * The signature for the underlying function is:
         * int seL4_X86_PageTable_Map(seL4_X86_PageTable service, seL4_X86_PageDirectory pd, seL4_Word vaddr, seL4_X86_VMAttributes attr)
         * @param service Capability to the page table to map.
         * @param pd Capability to the VSpace which will contain the mapping.
         * @param vaddr Virtual address to map the page table into.
         * @param rights Rights for the mapping.
         * @param attr VM Attributes for the mapping.
         * @return 0 on success.
         *
         * Note: this function is generated during build.  It is generated from the following definition:
         *
         * hint 2: for VM attributes use seL4_ARCH_Default_VMAttributes
         */
```
On completion, you will see another fault.

### Map a page

Use `seL4_ARCH_Page_Map` to map the frame in.
If everything was done correctly, there is no reason why this step
should fail. Complete it and proceed.
```

        /* TASK 5: then map the frame in */
        /* hint 1: use seL4_ARCH_Page_Map() as above
         * hint 2: for the rights, use seL4_AllRights
         * hint 3: for VM attributes use seL4_ARCH_Default_VMAttributes
         */
```
On completion, you will see the following:
```
main: hello world
dynamic-2: main@main.c:464 [Cond failed: seL4_MessageInfo_get_length(tag) != 1]
	Response data from thread_2 was not the length expected.
	How many registers did you set with seL4_SetMR within thread_2?
```

### Allocate an endpoint

Now we have a (fully mapped) IPC buffer -- but no Endpoint object to
send our IPC data across. We must retype an untyped object into a kernel
Endpoint object, and then proceed. This could be done via a more
low-level approach using `seL4_Untyped_Retype()`, but instead, the
tutorial makes use of the VKA allocator. Remember that the VKA allocator
is an seL4 type-aware object allocator? So we can simply ask it for a
new object of a particular type, and it will do the low-level retyping
for us, and return a capability to a new object as requested.

In this case, we want a new Endpoint so we can do IPC. Complete the step
and proceed.

- <https://github.com/seL4/seL4_libs/blob/master/libsel4vka/include/vka/object.h>

```

    /* TASK 6: create an endpoint */
    /* hint: vka_alloc_endpoint()
     * int vka_alloc_endpoint(vka_t *vka, vka_object_t *result)
     * @param vka Pointer to vka interface.
     * @param result Structure for the Endpoint object.  This gets initialised.
     * @return 0 on success
     */
```
On completion, the output will not change.

### Badge an endpoint

Badges are used to uniquely identify a message queued on an endpoint as
having come from a particular sender. Recall that in seL4, each thread
has only one IPC buffer. If multiple other threads are sending data to a
listening thread, how can that listening thread distinguish between the
data sent by each of its IPC partners? Each sender must "badge" its
capability to its target's endpoint.

Note the distinction: the badge is not applied to the target endpoint,
but to the sender's **capability** to the target endpoint. This
enables the listening thread to mint off copies of a capability to an
Endpoint to multiple senders. Each sender is responsible for applying a
unique badge value to the capability that the listener gave it so that
the listener can identify it.

In this step, you are badging the endpoint that you will use when
sending data to the thread you will be creating later on. The
`vka_mint_object()` call will return a new, badged copy of the
capability to the endpoint that your new thread will listen on. When you
send data to your new thread, it will receive the badge value with the
data, and know which sender you are. Complete the step and proceed.

- <https://github.com/seL4/seL4_libs/blob/master/libsel4vka/include/vka/object_capops.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/types_32.bf>

```

    /* TASK 7: make a badged copy of it in our cspace. This copy will be used to send
     * an IPC message to the original cap */
    /* hint 1: vka_mint_object()
     * int vka_mint_object(vka_t *vka, vka_object_t *object, cspacepath_t *result, seL4_CapRights rights, seL4_CapData_t badge)
     * @param[in] vka The allocator for the cspace.
     * @param[in] object Target object for cap minting.
     * @param[out] result Allocated cspacepath.
     * @param[in] rights The rights for the minted cap.
     * @param[in] badge The badge for the minted cap.
     * @return 0 on success
     *
     * hint 2: for the rights, use seL4_AllRights
     * hint 3: for the badge use seL4_CapData_Badge_new()
     * seL4_CapData_t CONST seL4_CapData_Badge_new(seL4_Uint32 Badge)
     * @param[in] Badge The badge number to use
     * @return A CapData structure containing the desired badge info
     *
     * seL4_CapData_t is generated during build.
     * The type definition and generated field access functions are defined in a generated file:
     * build/x86/pc99/libsel4/include/sel4/types_gen.h
     * It is generated from the following definition:
     *
     * hint 4: for the badge use EP_BADGE
     */
```
On completion, the output will not change.

### Message registers

Here we get a formal introduction to message registers. At first glance,
you might wonder why the `sel4_SetMR()` calls don't specify a message
buffer, and seem to know which buffer to fill out -- and that would be
correct, because they do. They are operating directly on the sending
thread's IPC buffer. Recall that each thread has only one IPC buffer. Go
back and look at your call to `seL4_TCB_Configure()` in step 7 again:
you set the IPC buffer for the new thread in the last 2 arguments to
this function. Likewise, the thread that created **your** main thread
also set an IPC buffer up for you.

So `seL4_SetMR()` and `seL4_GetMR()` simply write to and read from the IPC
buffer you designated for your thread. `MSG_DATA` is uninteresting -- can
be any value. You'll find the `seL4_MessageInfo_t` type explained in the
manuals. In short, it's a header that is embedded in each message that
specifies, among other things, the number of Message Registers that hold
meaningful data, and the number of capabilities that are going to be
transmitted in the message.

- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf>
- <https://github.com/seL4/seL4/blob/master/libsel4/arch_include/x86/sel4/arch/functions.h>

```

    /* TASK 8: set the data to send. We send it in the first message register */
    /* hint 1: seL4_MessageInfo_new()
     * seL4_MessageInfo_t CONST seL4_MessageInfo_new(seL4_Uint32 label, seL4_Uint32 capsUnwrapped, seL4_Uint32 extraCaps, seL4_Uint32 length)
     * @param label The value of the label field
     * @param capsUnwrapped The value of the capsUnwrapped field
     * @param extraCaps The value of the extraCaps field
     * @param length The number of message registers to send
     * @return The seL4_MessageInfo_t containing the given values.
     *
     * seL4_MessageInfo_new() is generated during build. It can be found in:
     * build/x86/pc99/libsel4/include/sel4/types_gen.h
     *
     * hint 2: use 0 for the first 3 fields.
     * hint 3: send only 1 message register of data
     *
     * hint 4: seL4_SetMR()
     * void seL4_SetMR(int i, seL4_Word mr)
     * @param i The message register to write
     * @param mr The value of the message register
     *
     * hint 5: send MSG_DATA
     */
```

On completion, the output should change as follows:
```
dynamic-2: main@main.c:472 [Cond failed: msg != ~MSG_DATA]
	Response data from thread_2's content was not what was expected.
```

### IPC

Now that you've constructed your message and badged the endpoint that
you'll use to send it, it's time to send it. The `seL4_Call()` syscall
will send a message across an endpoint synchronously. If there is no
thread waiting at the other end of the target endpoint, the sender will
block until there is a waiter. The reason for this is because the seL4
kernel would prefer not to buffer IPC data in the kernel address space,
so it just sleeps the sender until a receiver is ready, and then
directly copies the data. It simplifies the IPC logic. There are also
polling send operations, as well as polling receive operations in case
you don't want to be forced to block if there is no receiver on the
other end of an IPC Endpoint.

When you send your badged data using `seL4_Call()`, our receiving thread
(which we created earlier) will pick up the data, see the badge, and
know that it was us who sent the data. Notice how the sending thread
uses the **badged** capability to the endpoint object, and the
receiving thread uses the unmodified original capability to the same
endpoint? The sender must identify itself.

Notice also that the fact that both the sender and the receiver share
the same root CSpace, enables the receiving thread to just casually use
the original, unbadged capability without any extra work needed to make
it accessible.

Notice however also, that while the sending thread has a capability that
grants it full rights to send data across the endpoint since it was the
one that created that capability, the receiver's capability may not
necessarily grant it sending powers (write capability) to the endpoint.
It's entirely possible that the receiver may not be able to send a
response message, if the sender doesn't want it to.

- <https://github.com/seL4/seL4/blob/master/libsel4/sel4_arch_include/ia32/sel4/sel4_arch/syscalls.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf>

```

    /* TASK 9: send and wait for a reply. */
    /* hint: seL4_Call()
     * seL4_MessageInfo_t seL4_Call(seL4_CPtr dest, seL4_MessageInfo_t msgInfo)
     * @param dest The capability to be invoked.
     * @param msgInfo The messageinfo structure for the IPC.  This specifies information about the message to send (such as the number of message registers to send).
     * @return A seL4_MessageInfo_t structure.  This is information about the repy message.
     *
     * hint 2: seL4_MessageInfo_t is generated during build.
     * The type definition and generated field access functions are defined in a generated file:
     * build/x86/pc99/libsel4/include/sel4/types_gen.h
     * It is generated from the following definition:
     */
```

On completion, you should see thread_2 fault as follows:
```
thread_2: hallo wereld
thread_2: got a message 0 from 0
Caught cap fault in send phase at address (nil)
while trying to handle:
vm fault on data at address (nil) with status 0x4
in thread 0xffffff801ffb4400 "child of: 'rootserver'" at address (nil)
in thread 0xffffff801ffb4400 "child of: 'rootserver'" at address (nil)
With stack:
```

### Receive a reply

While this task is out of order, since we haven't yet examined the
receive-side of the operation here, it's fairly simple anyway: this task
occurs after the receiver has sent a reply, and it shows the sender now
reading the reply from the receiver. As mentioned before, the
`seL4_GetMR()` calls are simply reading from the calling thread's
designated, single IPC buffer.

- <https://github.com/seL4/seL4/blob/master/libsel4/arch_include/x86/sel4/arch/functions.h>

```

    /* TASK 10: get the reply message */
    /* hint: seL4_GetMR()
     * seL4_Word seL4_GetMR(int i)
     * @param i The message register to retreive
     * @return The message register value
     */
```
On completion, the output should not change.

### Receive an IPC

We're now in the receiving thread. The `seL4_Recv()` syscall performs a
blocking listen on an Endpoint or Notification capability. When new data
is queued (or when the Notification is signalled), the `seL4_Recv`
operation will unqueue the data and resume execution.

Notice how the `seL4_Recv()` operation explicitly makes allowance for
reading the badge value on the incoming message? The receiver is
explicitly interested in distinguishing the sender.

- <https://github.com/seL4/seL4/blob/master/libsel4/sel4_arch_include/aarch32/sel4/sel4_arch/syscalls.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf>

```

    /* TASK 11: wait for a message to come in over the endpoint */
    /* hint 1: seL4_Recv()
     * seL4_MessageInfo_t seL4_Recv(seL4_CPtr src, seL4_Word* sender)
     * @param src The capability to be invoked.
     * @param sender The badge of the endpoint capability that was invoked by the sender is written to this address.
     * @return A seL4_MessageInfo_t structure
     *
     * hint 2: seL4_MessageInfo_t is generated during build.
     * The type definition and generated field access functions are defined in a generated file:
     * build/x86/pc99/libsel4/include/sel4/types_gen.h
     */
```
On completion, the output should change slightly:
```
thread_2: got a message 0 from 0x61
```

### Validate the message

These two calls here are just verification of the fidelity of the
transmitted message. It's very unlikely you'll encounter an error here.
Complete them and proceed to the next step.

- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf>

```

    /* TASK 12: make sure it is what we expected */
    /* hint 1: check the badge. is it EP_BADGE?
     * hint 2: we are expecting only 1 message register
     * hint 3: seL4_MessageInfo_get_length()
     * seL4_Uint32 CONST seL4_MessageInfo_get_length(seL4_MessageInfo_t seL4_MessageInfo)
     * @param seL4_MessageInfo the seL4_MessageInfo_t to extract a field from
     * @return the number of message registers delivered
     * seL4_MessageInfo_get_length() is generated during build. It can be found in:
     * build/x86/pc99/libsel4/include/sel4/types_gen.h
     */
```

On completion, the output should not change.

### Read the message registers

Again, just reading the data from the Message Registers.

- <https://github.com/seL4/seL4/blob/master/libsel4/arch_include/x86/sel4/arch/functions.h>

```

    /* TASK 13: get the message stored in the first message register */
    /* hint: seL4_GetMR()
     * seL4_Word seL4_GetMR(int i)
     * @param i The message register to retreive
     * @return The message register value
     */
```
On completion, the output should change slightly:
```
thread_2: got a message 0x6161 from 0x61
```

### Write the message registers

And writing Message Registers again.

- <https://github.com/seL4/seL4/blob/master/libsel4/arch_include/x86/sel4/arch/functions.h>

```

    /* TASK 14: copy the modified message back into the message register */
    /* hint: seL4_SetMR()
     * void seL4_SetMR(int i, seL4_Word mr)
     * @param i The message register to write
     * @param mr The value of the message register
     */
```
On completion, the output should not change.

### Reply to a message

This is a formal introduction to the `Reply` capability which is
automatically generated by the seL4 kernel, whenever an IPC message is
sent using the `seL4_Call()` syscall. This is unique to the `seL4_Call()`
syscall, and if you send data instead with the `seL4_Send()` syscall, the
seL4 kernel will not generate a Reply capability.

The Reply capability solves the issue of a receiver getting a message
from a sender, but not having a sufficiently permissive capability to
respond to that sender. The "Reply" capability is a one-time capability
to respond to a particular sender. If a sender doesn't want to grant the
target the ability to send to it repeatedly, but would like to allow the
receiver to respond to a specific message once, it can use `seL4_Call()`,
and the seL4 kernel will facilitate this one-time permissive response.
Complete the step and pat yourself on the back.

- <https://github.com/seL4/seL4/blob/master/libsel4/sel4_arch_include/ia32/sel4/sel4_arch/syscalls.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf>

```

    /* TASK 15: send the message back */
    /* hint 1: seL4_ReplyRecv()
     * seL4_MessageInfo_t seL4_ReplyRecv(seL4_CPtr dest, seL4_MessageInfo_t msgInfo, seL4_Word *sender)
     * @param dest The capability to be invoked.
     * @param msgInfo The messageinfo structure for the IPC.  This specifies information about the message to send (such as the number of message registers to send) as the Reply part.
     * @param sender The badge of the endpoint capability that was invoked by the sender is written to this address.  This is a result of the Wait part.
     * @return A seL4_MessageInfo_t structure.  This is a result of the Wait part.
     *
     * hint 2: seL4_MessageInfo_t is generated during build.
     * The type definition and generated field access functions are defined in a generated file:
     * build/x86/pc99/libsel4/include/sel4/types_gen.h
     * It is generated from the following definition:
     */
```
On completion, the output should change, with the fault message replaced with the following:
```
main: got a reply: [0xffff9e9e|0xffffffffffff9e9e]
```
That's it for this tutorial.


---
## Getting help
Stuck? See the resources below. 
* [FAQ](https://docs.sel4.systems/FrequentlyAskedQuestions)
* [seL4 Manual](http://sel4.systems/Info/Docs/seL4-manual-latest.pdf)
* [Debugging guide](https://docs.sel4.systems/DebuggingGuide.html)
* [IRC Channel](https://docs.sel4.systems/IRCChannel)
* [Developer's mailing list](https://sel4.systems/lists/listinfo/devel)


