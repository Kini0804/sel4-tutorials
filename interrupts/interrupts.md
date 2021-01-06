# Interrupts


## Prerequisites

1. [Set up your machine](https://docs.sel4.systems/HostDependencies).
2. [Notification tutorial](https://docs.sel4.systems/Tutorials/notifications)

# Initialising

```sh
# For instructions about obtaining the tutorial sources see https://docs.sel4.systems/Tutorials/#get-the-code
#
# Follow these instructions to initialise the tutorial
# initialising the build directory with a tutorial exercise
./init --tut interrupts
# building the tutorial exercise
cd interrupts_build
ninja
```


## Outcomes

* Understand the purpose of the IRQControl capability.
* Be able to obtain capabilities for specific interrupts.
* Learn how to handle interrupts and their relation with notification objects.

## Background

### IRQControl

The root task is given a single capability from which capabilities to all irq numbers
in the system can be derived, `seL4_CapIRQControl`. This capability can be moved between CSpaces
and CSlots but cannot be duplicated. Revoking this capability results in all access to all
irq capabilities being removed.

### IRQHandlers

IRQHandler capabilities give access to a single irq and are standard seL4 capabilities: they
*can* be moved and duplicated according to system policy. IRQHandlers are obtained by
invoking the IRQControl capability, with architecture specific parameters. Below is an
example of obtaining an IRQHandler.

```bash
// Get a capability for irq number 7 and place it in cslot 10 in a single-level cspace.
error = seL4_IRQControl_Get(seL4_IRQControl, 7, cspace_root, 10, seL4_WordBits);
```

There are a variety of different invocations to obtain irq capabilities which are hardware
dependent, including:

* [`seL4_IRQControl_GetIOAPIC`](https://docs.sel4.systems/ApiDoc.html#get-io-apic) (x86)
* [`seL4_IRQControl_GetMSI`](https://docs.sel4.systems/ApiDoc.html#get-msi) (x86)
* [`seL4_IRQControl_GetTrigger`](https://docs.sel4.systems/ApiDoc.html#gettrigger) (ARM)

### Receiving interrupts

Interrupts are received by registering a capability to a notification object
with the IRQHandler capability for that irq, as follows:
```bash
seL4_IRQHandler_setNotification(irq_handler, notification);
```
On success, this call will result in signals being delivered to the notification object when
an interrupt occurs. To handle multiple interrupts on the same notification object, you
can set different badges on the notification capabilities bound to each IRQHandler.
 When an interrupt arrives,
the badge of the notification object bound to that IRQHandler is bitwise orred with the data
word in the notification object.
Recall the badging technique for differentiating signals from the
 [notification tutorial](https://docs.sel4.systems/Tutorials/notifications).

Interrupts can be polled for using `seL4_Poll` or waited for using `seL4_Wait`. Either system
call results in the data word of the notification object being delivered as the badge of the
message, and the data word cleared.

[`seL4_IRQHandler_Clear`](https://docs.sel4.systems/ApiDoc.html#clear) can be used to unbind
the notification from an IRQHandler.

### Handling interrupts

Once an interrupt is received and processed by the software, you can unmask the interrupt
using [`seL4_IRQHandler_Ack`](https://docs.sel4.systems/ApiDoc.html#ack) on the IRQHandler.
seL4 will not deliver any further interrupts after an IRQ is raised until that IRQHandler
has been acked.

## Exercises

In this tutorial you will set up interrupt handling for a provided timer driver
on the zynq7000 ARM platform. This timer driver can be located inside the
`projects/sel4-tutorials/zynq_timer_driver` folder from the root of the
projects directory, i.e. where the `.repo` folder can be found and where the
initial `repo init` command was executed. The tutorial has been set up with two
processes: `timer.c`, the timer driver and RPC server, and `client.c`, which
makes a single request.

On successful initialisation of the tutorial, you will see the following:

```
timer client: hey hey hey
timer: got a message from 61 to sleep 2 seconds
<<seL4(CPU 0) [decodeInvocation/530 T0xe8265600 "tcb_timer" @84e4]: Attempted to invoke a null cap #9.>>
main@timer.c:78 [Cond failed: error]
	Failed to ack irq
```

The timer driver we are using emits an interrupt in the `TTC0_TIMER1_IRQ` number.

**Exercise** Invoke `irq_control`, which contains the `seL4_IRQControl` capability,
the place the `IRQHandler` capability for `TTC0_TIMER1_IRQ` into the `irq_handler` CSlot.

```
    /* TODO invoke irq_control to put the interrupt for TTC0_TIMER1_IRQ in
       cslot irq_handler (depth is seL4_WordBits) */
```

On success, you should see the following output, without the error message that occurred earlier,
as the irq_handle capability is now valid:

```
Undelivered IRQ: 42
```

This is a warning message from the kernel that an IRQ was recieved for irq number 42, but no
notification capability is set to sent a signal to.

**Exercise** Now set the notification capability (`ntfn`) by invoking the irq handler.

```
     /* TODO set ntfn as the notification for irq_handler */
```

Now the output will be:

```
Tick
```

Only one interrupt is delivered, as the interrupt has not been acknowledged. The timer driver is
programmed to emit an interrupt every millisecond, so we need to count 2000 interrupts
before replying to the client.

**Exercise** Acknowledge the interrupt after handling it in the timer driver.

```
        /* TODO ack the interrupt */
```

Now the timer interrupts continue to come in, and the reply is delivered to the client.

```
timer client wakes up
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
