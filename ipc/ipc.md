# IPC


## Prerequisites

1. [Set up your machine](https://docs.sel4.systems/HostDependencies).
2. [Capabilities tutorial](https://docs.sel4.systems/Tutorials/capabilities)

## Initialising

```sh
# For instructions about obtaining the tutorial sources see https://docs.sel4.systems/Tutorials/#get-the-code
#
# Follow these instructions to initialise the tutorial
# initialising the build directory with a tutorial exercise
./init --tut ipc
# building the tutorial exercise
cd ipc_build
ninja
```


## Outcomes

1. Be able to use IPC to send data and capabilities between processes.
2. Learn the jargon *cap transfer*.
3. Be able to differentiate requests via badged capabilities.
4. Know how to design protocols that use the IPC fastpath.

## Background

Interprocess communication (IPC) is the microkernel mechanism for synchronous transmission of small amounts of data
and capabilities between processes. In seL4, IPC is facilitated by small kernel objects known as *endpoints*, which
act as general communication ports. Invocations on endpoint objects are used to send and receive IPC messages.

Endpoints consist of a queue of threads waiting to send, or waiting to receive messages. To understand
this, consider an example where *n* threads are waiting for a message on an endpoint. If *n* threads
send messages on the endpoint, all *n* waiting threads will receive the message and wake up. If an *n+1*th
sender sends a message, that sender is now queued.

### System calls

Threads can send messages on endpoints with the system call `seL4_Send`, which blocks until the message has been
consumed by another thread. `seL4_NBSend` can also be used, which performs a polling send: the send is only
successful if a receiver is already blocked waiting for a message, and otherwise fails. To avoid a
back channel, `seL4_NBSend` does not return a result indicating if the message was sent or not.

`seL4_Recv` can be used to receive messages, and `seL4_NBRecv` can be used to poll for messages.

`seL4_Call` is a system call that essentially combines an `seL4_Send` and an `seL4_Recv` with one
major difference: in the receive phase, thread which uses this function is blocked on a one-time capability termed a
*reply capability*, and not the endpoint itself. In a client-server scenario, where clients use
`seL4_Call` to make requests, the server can explicitly reply to the correct client.

The reply capability is stored internally in the thread control block (TCB) of the receiver. The system
call `seL4_Reply` invokes this capability, which sends an IPC to the client and wakes it up. `seL4_ReplyRecv`
does the same, except it sends the reply and blocks on the provided endpoint in a combined system call.

Since TCBs have a single space to store a reply capability, if servers need to service multiple
requests (e.g saving requests to reply at a later time, after hardware operations have been completed),
[`seL4_CNode_SaveCaller`](https://docs.sel4.systems/ApiDoc.html#save-caller) can be used to save
the reply capability to an empty slot in the receivers CSpace.

### IPC Buffer

Each thread has a buffer (referred to as the *IPC buffer*), which contains the payload of the IPC message,
consisting of data and capabilities. Senders specify a message length and the kernel copies this (bounded)
amount between the sender and receiver IPC buffer.

### Data transfer

The IPC buffer contains a bounded area of message registers (MR) used to transmit data on IPC. Each
register is the machine word size, and the maximum message size is available in the
`seL4_MsgMaxLength` constant provided by `libsel4`.

Messages can be loaded into the IPC buffer using `seL4_SetMR` and extracted using `seL4_GetMR`.
Small messages are sent in registers and do not require a copy operation. The amount of words
that fit in registers is available in the `seL4_FastMessageRegisters` constant.

The amount of data being transferred, in terms of the number of message registers used, must be
set in as the `length` field in the `seL4_MessageInfo_t` data structure.

### Cap transfer

Along with data, IPC can be used to send capabilities between processes per message. This is referred to as
 *cap transfer*. The number of capabilities being transferred is encoded in the `seL4_MessageInfo_t`
structure as `extraCaps`. Below is an example for sending a capability via IPC:

```c
   seL4_MessageInfo info = seL4_MessageInfo_new(0, 0, 1, 0);
   seL4_SetCap(0, free_slot);
   seL4_Call(endpoint, info);
```

To receive a capability, the receiver must specify a cspace address to place the capability in. This is
shown in the code example below:

```c
    seL4_SetCapReceivePath(cnode, badged_endpoint, seL4_WordBits);
    seL4_Recv(endpoint, &sender);
```

The access rights of the received capability are the same as  by the rights that the receiver has to the endpoint.
Note that while senders can send multiple capabilities, receivers can only receive one at a time.

### Capability unwrapping

seL4 can also *unwrap* capabilities on IPC.
If the n-th capability in the message refers to the endpoint through which the message
is sent, the capability is unwrapped: its badge is placed into the n-th position of the
receiver's IPC buffer (in the field `caps_or_badges`), and the kernel sets the n-th bit
(counting from the least significant) in the `capsUnwrapped` field of `seL4_MessageInfo_t`.

### Message Info

The `seL4_MessageInfo_t` data structure is used to encode the description of an IPC message into a single word.
It is used to describe a message to be sent to seL4, and for seL4 to describe the message that was
sent to the receiver.
It contains the following fields:

* `length` the amount of message registers (data) in the message (`seL4_MsgMaxLength` maximum),
* `extraCaps` the number of capabilities in the message (`seL4_MsgMaxExtraCaps`)
* `capsUnwrapped` marks any capabilities unwrapped by the kernel.
* `label` data that is transferred unmodified by the kernel from sender to receiver,

### Badges

Along with the message the kernel additionally delivers the badge of the endpoint capability
that the sender invoked to send the message. Endpoints can be badged using
[`seL4_CNode_Mint`](https://docs.sel4.systems/ApiDoc.html#mint) or
 [`seL4_CNode_Mutate`](https://docs.sel4.systems/ApiDoc.html#mutate). Once an endpoint is badged,
the badge of the endpoint is transferred to any receiver that receives messages on that endpoint.
The code example below demonstrates this:

```c
seL4_Word badge;
seL4_Recv(endpoint, &badge);
// once a message is received, the badge value is set by seL4 to the
// badge of capability used by the sender to send the message
```

### Fastpath

Fast IPC is essential to microkernel-based systems, as services are often separated from each other
for isolation, with IPC one of the core mechanisms for communication between clients and services.
Consequently, IPC has a fastpath -- a heavily optimised path in the kernel -- which allows these operations
to be very fast. In order to use the fastpath, an IPC must meet the following conditions:

* `seL4_Call` or `seL4_ReplyRecv` must be used.
* The data in the message must fit into the `seL4_FastMessageRegisters` registers.
* The processes must have valid address spaces.
* No caps should be transferred.
* No other threads in the scheduler of higher priority than the thread unblocked by the IPC can be running.

## Exercises

This tutorial has several processes set up by the capDL loader, two clients and a server. All processes have
access to a single endpoint capability, which provides access to the same endpoint object.

In this tutorial, you will construct a server which echos the contents of messages sent by clients. You
will also alter the ordering of replies from the clients to get the right message.

When you run the tutorial, the output should be something like this:

```
Client 2: waiting for badged endpoint
Badged 2
Assertion failed: seL4_MessageInfo_get_extraCaps(info) == 1 (../ipcCkQ6Ub/client_2.c: main: 22)
Client 1: waiting for badged endpoint
Badged 1
Assertion failed: seL4_MessageInfo_get_extraCaps(info) == 1 (../ipcCkQ6Ub/client_1.c: main: 22)
```


On initialisation, both clients use the following protocol: they wait on the provided endpoint for
a badged endpoint to be sent to them via cap transfer. All following messages sent by the client
uses the badged endpoint, such that the server can identify the client. However, the server does not
currently send the badged capability! We have provided code to badge the endpoint capability, and
reply to the client.

**Exercise** Your task is to set up the cap transfer such that the client successfully
receives the badged endpoint.

```c
             /* No badge! give this sender a badged copy of the endpoint */
             seL4_Word badge = seL4_GetMR(0);
             seL4_Error error = seL4_CNode_Mint(cnode, free_slot, seL4_WordBits,
                                                cnode, endpoint, seL4_WordBits,
                                                seL4_AllRights, badge);
             printf("Badged %lu\n", badge);

             // TODO use cap transfer to send the badged cap in the reply

             /* reply to the sender and wait for the next message */
             seL4_Reply(info);

             /* now delete the transferred cap */
             error = seL4_CNode_Delete(cnode, free_slot, seL4_WordBits);
             assert(error == seL4_NoError);

             /* wait for the next message */
             info = seL4_Recv(endpoint, &sender);
```

Now the output should look something like:
```bash
Booting all finished, dropped to user space
Client 2: waiting for badged endpoint
Badged 2
Client 1: waiting for badged endpoint
Badged 1
Client 2: received badged endpoint
Client 1: received badged endpoint
```

Depending on timing, the messages may be different, the result is the same: the system hangs.
This is because one of the clients has hit the else case, where the badge is set, and the server
does not respond, or wait for new messages from this point.

**Exercise** Your next task is to implement the echo part of the server.

```c
             // TODO use printf to print out the message sent by the client
             // followed by a new line
```

At this point, you should see a single word output to the console in a loop.
```
the
the
the
```

This is because the server does not reply to the client, and continues to spin in a loop
 repeating the last message.
**Exercise**  Update the code to reply to the clients after printing the message.

```c
             // TODO reply to the client and wait for the next message
```

Now the output should be something like this:

```
Client 2: received badged endpoint
the
brown
jumps
the
dog
Client 1: received badged endpoint
quick
fox
over
lazy
```

**Exercise** Currently each client is scheduled for its full timeslice until it is preempted. Alter
your server to only print one message from each client, alternating. You will need to use
[`seL4_CNode_SaveCaller`](https://docs.sel4.systems/ApiDoc.html#save-caller)  to save the reply
capability for each sender. You can use `free_slot` to store the reply capabilities.

Depending on your approach, successful output should look something like this:
```
Client 2: received badged endpoint
the
Client 1: received badged endpoint
quick
fox
brown
jumps
over
lazy
the
dog
```

### Further exercises

That's all for the detailed content of this tutorial. Below we list other ideas for exercises you can try,
to become more familiar with IPC.

* Try using `seL4_Send` and `seL4_Recv`.
* Try the non-blocking variants, `seL4_NBSend` and `seL4_NBRecv`.


---
## Getting help
Stuck? See the resources below. 
* [FAQ](https://docs.sel4.systems/FrequentlyAskedQuestions)
* [seL4 Manual](http://sel4.systems/Info/Docs/seL4-manual-latest.pdf)
* [Debugging guide](https://docs.sel4.systems/DebuggingGuide.html)
* [IRC Channel](https://docs.sel4.systems/IRCChannel)
* [Developer's mailing list](https://sel4.systems/lists/listinfo/devel)
