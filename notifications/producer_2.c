

#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <utils/util.h>
#include <sel4utils/util.h>

// caps to notification objects
extern seL4_CPtr empty;
extern seL4_CPtr full;
extern seL4_CPtr endpoint;

int main(int c, char *argv[]) {
    int id = 2;
    
    seL4_Recv(endpoint, NULL);
    volatile long *buf = (volatile long *) seL4_GetMR(0);
    
    for (int i = 0; i < 100; i++) {
        seL4_Wait(empty, NULL);
        printf("%d: produce\n", id);
        *buf = id;
        seL4_Signal(full);
    }
    return 0;
}