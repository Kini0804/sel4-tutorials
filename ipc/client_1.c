

#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <utils/util.h>

extern seL4_CPtr endpoint;
extern seL4_CPtr cnode;
extern seL4_CPtr badged_endpoint;

const char *messages[] = {"quick", "fox", "over", "lazy"};

int main(int c, char *argv[]) {

    int id = 1;
    
    printf("Client %d: waiting for badged endpoint\n", id);
    seL4_SetCapReceivePath(cnode, badged_endpoint, seL4_WordBits);
    seL4_SetMR(0, id);
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 1, 0, 1);
    info = seL4_Call(endpoint, info);
    assert(seL4_MessageInfo_get_extraCaps(info) == 1);
    /* wait for the server to send us an endpoint */
    printf("Client %d: received badged endpoint\n", id);

    for (int i = 0; i < ARRAY_SIZE(messages); i++) {
        int j;
        for (j = 0; messages[i][j] != '\0'; j++) {
            seL4_SetMR(j, messages[i][j]);
        }
        info = seL4_MessageInfo_new(0, 0, 0, j);
        seL4_Call(badged_endpoint, info);
    }
    return 0;
}