
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
 * seL4 tutorial part 4: application to be run in a process
 */

#include <stdio.h>
#include <assert.h>

#include <sel4/sel4.h>
#include <sel4utils/process.h>

#include <utils/zf_log.h>
#include <sel4utils/sel4_zf_logif.h>

/* constants */
#define MSG_DATA 0x6161 //  arbitrary data to send

int main(int argc, char **argv)
{
    seL4_MessageInfo_t tag;
    seL4_Word msg;

    printf("process_2: hey hey hey\n");

    /* set the data to send. We send it in the first message register */
    tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, MSG_DATA);

    /* TASK 8: send and wait for a reply */
    ZF_LOGF_IF(argc < 1,
               "Missing arguments.\n");
    seL4_CPtr ep = (seL4_CPtr)atol(argv[0]);
    tag = seL4_Call(ep, tag);
    /* check that we got the expected reply */
    ZF_LOGF_IF(seL4_MessageInfo_get_length(tag) != 1,
               "Length of the data send from root thread was not what was expected.\n"
               "\tHow many registers did you set with seL4_SetMR, within the root thread?\n");

    msg = seL4_GetMR(0);
    ZF_LOGF_IF(msg != ~MSG_DATA,
               "Unexpected response from root thread.\n");

    printf("process_2: got a reply: %#" PRIxPTR "\n", msg);

    return 0;
}