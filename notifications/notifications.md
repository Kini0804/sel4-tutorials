
# Notifications and shared memory

## Prerequisites

1. [Set up your machine](https://docs.sel4.systems/HostDependencies).
1. [Capabilities tutorial](https://docs.sel4.systems/Tutorials/capabilities)
1. [Mapping tutorial](https://docs.sel4.systems/Tutorials/mapping)
1. [Threads tutorial](https://docs.sel4.systems/Tutorials/threads)

## Outcomes

1. Understand how to set up shared memory between tasks.
2. Be able to use notification objects for synchronisation between tasks.
4. Know how to use badges to differentiate notifications.

## Background

Notifications allow processes to send asynchronous signals to each other, and are primarily used for interrupt handling
and to synchronise access to shared data buffers.

### Notification objects

Signals are sent and received with invocations on capabilities to notification objects.
A notification object consists of a data word, which acts as an array of binary semaphores, and a queue of
TCBs waiting to for notifications.

Notification objects can be in three states:
* Waiting - there are TCBs queued on this notification waiting for to be signalled.
* Active - TCBs have signalled data on this notification,
* Idle - no TCBs are queued and no TCBs have signalled this object since it was last set to idle.

#### Signalling

When a task signals a notification object (using `seL4_Signal`), what occurs depends on the state of the object:
* Idle - the data word is set to the badge of the capability used to send the signal, and the object is converted
 to active.
* Active - the badge of the capability used to signal the notification object is bitwise-orred with the notifications data word.
* Waiting - the head of the queue of TCBs is woken and the badge sent to that TCB. If the queue is empty, the notification
object is transitioned to idle.

In this way notification objects can be seen as a binary array of semaphores - if the signallers all use a
different bit in the badge, they can set different badge bits and waiters can observe which bits have been set.

#### Waiting

Tasks can wait on a notification object using `seL4_Wait`, which does the following:

* Idle - the TCB is queued, and the notification transitioned to waiting.
* Active - the TCB receives the data word, the data word is reset to 0 and the notification transitioned to idle,
* Waiting - the TCB is appended to the queue.

#### Polling

Notification objects can also be polled with `seL4_Poll`, which is a non-blocking version of `seL4_Wait` that returns
immediately regardless of the state.

## Interrupts and IPC

Notification objects can be used to receive signals of interrupt delivery, and can also be bound to TCBs
such that signals and IPC can be received by the same thread. This is explained in more detail in the
timer tutorial. <!--TODO link to timer tutorial -->

## Exercises

These exercises guide you through a basic producer consumer set up using notifications and shared memory. The 
tutorial uses the capDL loader, and already has 2 producer processes (`producer_1.c` and `producer_2`) and 1 consumer
 process running (`consumer.c`). Each has access to a number of capabilities. 
 
Each producer shares a buffer with the consumer, and the consumer processes data from both producers when it is
available.

When you start the tutorial, the output will look something like this:
```
Booting all finished, dropped to user space
Waiting for producer
```
### Set up shared memory

Both producers start and block immediately, waiting for the consumer to send an IPC with the address of the shared 
mapping. We provide code below that sets up the shared page between producer 1 and the consumer:

```c
    /* set up shared memory for consumer 1 */
    /* first duplicate the cap */
    error = seL4_CNode_Copy(cnode, mapping_1, seL4_WordBits, 
                          cnode, buf1_frame_cap, seL4_WordBits, seL4_AllRights);
    ZF_LOGF_IFERR(error, "Failed to copy cap");
    /* now do the mapping */
    error = seL4_ARCH_Page_Map(mapping_1, producer_1_vspace, BUF_VADDR, 
                               seL4_AllRights, seL4_ARCH_Default_VMAttributes);
    ZF_LOGF_IFERR(error, "Failed to map frame");
```

However, we do not map the second buffer in, so producer 2 crashes immediately.

**Exercise** Understand the above code, and create a second shared page between `producer_2` and `consumer`. 

```c
    // TODO share buf2_frame_cap with producer_2
```

Whether this is successful will be visible after the next exercise when the consumers access their buffers. If the shared page setup for producer 2 is not correct, it will fail with a vm fault.

### Signal the producers to go

At this point, both producers are waiting on the `empty` notification for a signal that the buffer is ready 
to be written to.

**Exercise** signal both producers via the `buf1_empty` and `buf2_empty` notification objects.

```c
    // TODO signal both producers
```

### Differentiate signals

Now you should see something like the following:

```
Booting all finished, dropped to user space
Waiting for producer
2: produce
1: produce
Got badge: 2
Got badge: 1
```

At this point, the consumer should consume data from the appropriate buffer(s) and signal to the appropriate consumer(s)
that the buffer is empty again. The capability to the `full` notification object has already been badged: `producer_1`s
copy has a badge of `0b1` and `producer_2` a badge of `0b10`. By checking the bits in the badge, you can see
which of the producers (it may be both) has produced data.

**Exercise** Check the badge and signal the empty notification for the producers according to the bits set in the badge
 value.
 
```c
    // TODO, use the badge to check which producer has signalled you, and signal it back. Note that you 
    // may recieve more than 1 signal at a time.
```

At this point, you should see signals from both producers being processed, and the final `Success!` message printed.
 
### Further exercises

That's all for the detailed content of this tutorial. Below we list other ideas for exercises you can try,
to become more familiar with IPC.

* Create a counting semaphore implementation using notification objects.
* Create a bounded-buffer producer consumer with a buffer size greater than 1. 


---
## Getting help
Stuck? See the resources below. 
* [FAQ](https://docs.sel4.systems/FrequentlyAskedQuestions)
* [seL4 Manual](http://sel4.systems/Info/Docs/seL4-manual-latest.pdf)
* [Debugging guide](https://docs.sel4.systems/DebuggingGuide.html)
* [IRC Channel](https://docs.sel4.systems/IRCChannel)
* [Developer's mailing list](https://sel4.systems/lists/listinfo/devel)
