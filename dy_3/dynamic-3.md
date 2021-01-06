

# seL4 Dynamic Libraries: Processes & Elf loading

This tutorial shows how a separate ELF file can be loaded and expanded into a
VSpace, and subsequently executed, while facilitating IPC between the
two modules. It also shows how threads with different CSpaces have to
maneuver in order to pass capabilities to one another.

Don't gloss over the globals declared before `main()` -- they're declared
for your benefit so you can grasp some of the basic data structures.
Uncomment them one by one as needed when going through the tasks.

You'll observe that the things you've already covered in the second
tutorial are filled out and you don't have to repeat them: in much the
same way, we won't be repeating conceptual explanations on this page, if
they were covered by a previous tutorial in the series.

## Learning outcomes

- Once again, repeat the spawning of a thread: however, this time
        the two threads will only share the same vspace, but have
        different CSpaces. This is an automatic side effect of the way
        that sel4utils creates new "processes".
- Learn how the init thread in an seL4 system performs some types
        of initialization that aren't traditionally left to userspace.
- Observe and understand the effects of creating thread that do
        not share the same CSpace.
- Understand IPC across CSpace boundaries.
- Understand how minting a capability to a thread in another
        CSpace works.


## Initialising

```sh
# For instructions about obtaining the tutorial sources see https://docs.sel4.systems/Tutorials/#get-the-code
#
# Follow these instructions to initialise the tutorial
# initialising the build directory with a tutorial exercise
./init --tut dynamic-3
# building the tutorial exercise
cd dynamic-3_build
ninja
```


## Prerequisites

1. [Set up your machine](https://docs.sel4.systems/HostDependencies).
1. [dynamic-2](https://docs.sel4.systems/Tutorials/dynamic-2)

## Exercises

Tasks in this tutorial are in `main.c` and `app.c`.

When you first run this tutorial, you should see the following output:

```
Booting all finished, dropped to user space
Node 0 of 1
IOPT levels:     4294967295
IPC buffer:      0x57f000
Empty slots:     [488 --> 4096)
sharedFrames:    [0 --> 0)
userImageFrames: [16 --> 399)
userImagePaging: [12 --> 15)
untypeds:        [399 --> 488)
Initial thread domain: 0
Initial thread cnode size: 12
dynamic-3: vspace_reserve_range_aligned@vspace.h:621 Not implemented
dynamic-3: main@main.c:117 [Cond failed: virtual_reservation.res == NULL]
	Failed to reserve a chunk of memory.
```

### Virtual memory management

Aside from receiving information about IRQs in the IRQControl object
capability, and information about available IO-Ports, and ASID
availability, and several other privileged bits of information, the init
thread is also responsible, surprisingly, for reserving certain critical
ranges of memory as being used, and unavailable for applications.

This call to `sel4utils_bootstrap_vspace_with_bootinfo_leaky()` does
that. For an interesting look at what sorts of things the init thread
does, see:
`static int reserve_initial_task_regions(vspace_t *vspace, void *existing_frames[])`,
which is eventually called on by
`sel4utils_bootstrap_vspace_with_bootinfo_leaky()`. So while this
function may seem tedious, it's doing some important things.
- <https://github.com/seL4/seL4_libs/blob/master/libsel4utils/include/sel4utils/vspace.h>
- <https://github.com/seL4/seL4_libs/blob/master/libsel4utils/include/sel4utils/vspace.h>

```c

    /* TASK 1: create a vspace object to manage our vspace */
    /* hint 1: sel4utils_bootstrap_vspace_with_bootinfo_leaky()
     * int sel4utils_bootstrap_vspace_with_bootinfo_leaky(vspace_t *vspace, sel4utils_alloc_data_t *data, seL4_CPtr page_directory, vka_t *vka, seL4_BootInfo *info)
     * @param vspace Uninitialised vspace struct to populate.
     * @param data Uninitialised vspace data struct to populate.
     * @param page_directory Page directory for the new vspace.
     * @param vka Initialised vka that this virtual memory allocator will use to allocate pages and pagetables. This allocator will never invoke free.
     * @param info seL4 boot info
     * @return 0 on succes.
     */
```
On success, you should see a different error:
```
<<seL4(CPU 0) [handleUnknownSyscall/106 T0xffffff801ffb5400 "dynamic-3" @40139e]: SysDebugNameThread: cap is not a TCB, halting>>
halting...
```

### Configure a process

`sel4utils_configure_process_custom` took a large amount of the work
out of creating a new "processs". We skipped a number of steps. Take a
look at the source for `sel4utils_configure_process_custom()` and
notice how it spawns the new thread with its own CSpace by
automatically. This will have an effect on our tutorial! It means that
the new thread we're creating will not share a CSpace with our main
thread.
- <https://github.com/seL4/seL4_libs/blob/master/libsel4utils/include/sel4utils/process.h>

```c

    /* TASK 2: use sel4utils to make a new process */
    /* hint 1: sel4utils_configure_process_custom()
     * hint 2: process_config_default_simple()
     * @param process Uninitialised process struct.
     * @param vka Allocator to use to allocate objects.
     * @param vspace Vspace allocator for the current vspace.
     * @param priority Priority to configure the process to run as.
     * @param image_name Name of the elf image to load from the cpio archive.
     * @return 0 on success, -1 on error.
     *
     * hint 2: priority is in APP_PRIORITY and can be 0 to seL4_MaxPrio
     * hint 3: the elf image name is in APP_IMAGE_NAME
     */
 ```
On success, you should see a different error:
```
 dynamic-3: main@main.c:196 [Cond failed: new_ep_cap == 0]
	Failed to mint a badged copy of the IPC endpoint into the new thread's CSpace.
	sel4utils_mint_cap_to_process takes a cspacepath_t: double check what you passed.
```

### Get a `cspacepath`

Now, in this particular case, we are making the new thread be the
sender. Recall that the sender must have a capability to the endpoint
that the receiver is listening on, in order to send to that listener.
But in this scenario, our threads do **not** share the same CSpace!
The only way the new thread will know which endpoint it needs a
capability to, is if we tell it. Furthermore, even if the new thread
knows which endpoint object we are listening on, if it doesn't have a
capability to that endpoint, it still can't send data to us. So we must
provide our new thread with both a capability to the endpoint we're
listening on, and also make sure, that that capability we give it has
sufficient privileges to send across the endpoint.

There is a number of ways we could approach this, but in this tutorial
we decided to just pre-initialize the sender's CSpace with a sufficient
capability to enable it to send to us right from the start of its
execution. We could also have spawned the new thread as a listener
instead, and made it wait for us to send it a message with a sufficient
capability.

So we use `vka_cspace_make_path()`, which locates one free capability
slot in the selected CSpace, and returns a handle to it, to us. We then
filled that free slot in the new thread's CSpace with a **badged**
capability to the endpoint we are listening on, so as so allow it to
send to us immediately. We could have filled the slot with an unbadged
capability, but then if we were listening for multiple senders, we
wouldn't know who was whom.
- <https://github.com/seL4/seL4_libs/blob/master/libsel4vka/include/vka/vka.h>
```c

    /* TASK 3: make a cspacepath for the new endpoint cap */
    /* hint 1: vka_cspace_make_path()
     * void vka_cspace_make_path(vka_t *vka, seL4_CPtr slot, cspacepath_t *res)
     * @param vka Vka interface to use for allocation of objects.
     * @param slot A cslot allocated by the cspace alloc function
     * @param res Pointer to a cspacepath struct to fill out
     *
     * hint 2: use the cslot of the endpoint allocated above
     */
    cspacepath_t ep_cap_path;
    seL4_CPtr new_ep_cap = 0;
 ```
On success, the output should not change.

### Badge a capability

As discussed above, we now just mint a badged copy of a capability to
the endpoint we're listening on, into the new thread's CSpace, in the
free slot that the VKA library found for us.
- <https://github.com/seL4/seL4_libs/blob/master/libsel4utils/include/sel4utils/process.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/types_32.bf>

```c

    /* TASK 4: copy the endpont cap and add a badge to the new cap */
    /* hint 1: sel4utils_mint_cap_to_process()
     * seL4_CPtr sel4utils_mint_cap_to_process(sel4utils_process_t *process, cspacepath_t src, seL4_CapRights rights, seL4_CapData_t data)
     * @param process Process to copy the cap to
     * @param src Path in the current cspace to copy the cap from
     * @param rights The rights of the new cap
     * @param data Extra data for the new cap (e.g., the badge)
     * @return 0 on failure, otherwise the slot in the processes cspace.
     *
     * hint 2: for the rights, use seL4_AllRights
     * hint 3: for the badge use seL4_CapData_Badge_new()
     * seL4_CapData_t CONST seL4_CapData_Badge_new(seL4_Uint32 Badge)
     * @param[in] Badge The badge number to use
     * @return A CapData structure containing the desired badge info
     *
     * seL4_CapData_t is generated during build.
     *
     * hint 4: for the badge value use EP_BADGE
     */
```
On success, the output should look something like:
```
NEW CAP SLOT: 6ac.
main: hello world
dynamic-3: main@main.c:247 [Cond failed: sender_badge != EP_BADGE]
	The badge we received from the new thread didn't match our expectation
```

### Spawn a process

So now that we've given the new thread everything it needs to
communicate with us, we can let it run. Complete this step and proceed.
- <https://github.com/seL4/seL4_libs/blob/master/libsel4utils/include/sel4utils/process.h>

```c

    /* TASK 5: spawn the process */
    /* hint 1: sel4utils_spawn_process_v()
     * int sel4utils_spawn_process(sel4utils_process_t *process, vka_t *vka, vspace_t *vspace, int argc, char *argv[], int resume)
     * @param process Initialised sel4utils process struct.
     * @param vka Vka interface to use for allocation of frames.
     * @param vspace The current vspace.
     * @param argc The number of arguments.
     * @param argv A pointer to an array of strings in the current vspace.
     * @param resume 1 to start the process, 0 to leave suspended.
     * @return 0 on success, -1 on error.
     */
```
On success, you should be able to see the second process running. The output should
be as follows:
```
NEW CAP SLOT: 6ac.
process_2: hey hey hey
main@app.c:67 [Cond failed: msg != ~MSG_DATA]
	Unexpected response from root thread.
main: hello world
dynamic-3: main@main.c:255 [Cond failed: sender_badge != EP_BADGE]
	The badge we received from the new thread didn't match our expectation.
```

### Receive a message

We now wait for the new thread to send us data using `seL4_Recv()`...
Then we verify the fidelity of the data that was transmitted.
- <https://github.com/seL4/seL4/blob/master/libsel4/sel4_arch_include/ia32/sel4/sel4_arch/syscalls.h>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf>

```c

    /* TASK 6: wait for a message */
    /* hint 1: seL4_Recv()
     * seL4_MessageInfo_t seL4_Recv(seL4_CPtr src, seL4_Word* sender)
     * @param src The capability to be invoked.
     * @param sender The badge of the endpoint capability that was invoked by the sender is written to this address.
     * @return A seL4_MessageInfo_t structure
     *
     * hint 2: seL4_MessageInfo_t is generated during build.
     * hint 3: use the badged endpoint cap that you minted above
     */
```
On success, the badge error should no longer be visible.

### Send a reply

Another demonstration of the `sel4_Reply()` facility: we reply to the
message sent by the new thread.
- <https://github.com/seL4/seL4/blob/master/libsel4/sel4_arch_include/ia32/sel4/sel4_arch/syscalls.h#L359>
- <https://github.com/seL4/seL4/blob/master/libsel4/include/sel4/shared_types_32.bf#L15>

```c

    /* TASK 7: send the modified message back */
    /* hint 1: seL4_ReplyRecv()
     * seL4_MessageInfo_t seL4_ReplyRecv(seL4_CPtr dest, seL4_MessageInfo_t msgInfo, seL4_Word *sender)
     * @param dest The capability to be invoked.
     * @param msgInfo The messageinfo structure for the IPC.  This specifies information about the message to send (such as the number of message registers to send) as the Reply part.
     * @param sender The badge of the endpoint capability that was invoked by the sender is written to this address. This is a result of the Wait part.
     * @return A seL4_MessageInfo_t structure.  This is a result of the Wait part.
     *
     * hint 2: seL4_MessageInfo_t is generated during build.
     * hint 3: use the badged endpoint cap that you used for Call
     */
```
On success, the output should not change.

### Client Call

In the new thread, we initiate communications by using `seL4_Call()`. As
outlined above, the receiving thread replies to us using
`sel4_ReplyRecv()`. The new thread then checks the fidelity of the data
that was sent, and that's the end.

- <https://github.com/seL4/seL4/blob/master/libsel4/sel4_arch_include/ia32/sel4/sel4_arch/syscalls.h>

```c

 /* TASK 8: send and wait for a reply */
    /* hint 1: seL4_Call()
     * seL4_MessageInfo_t seL4_Call(seL4_CPtr dest, seL4_MessageInfo_t msgInfo)
     * @param dest The capability to be invoked.
     * @param msgInfo The messageinfo structure for the IPC.  This specifies information about the message to send (such as the number of message registers to send).
     * @return A seL4_MessageInfo_t structure.  This is information about the repy message.
     *
     * hint 2: send the endpoint cap using argv (see TASK 6 in the other main.c)
     */
    ZF_LOGF_IF(argc < 1,
               "Missing arguments.\n");
    seL4_CPtr ep = (seL4_CPtr) atol(argv[0]);
```
On success, you should see the following:
```
process_2: hey hey hey
main: got a message 0x6161 from 0x61
process_2: got a reply: 0xffffffffffff9e9e
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
