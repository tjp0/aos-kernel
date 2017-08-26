#include <utils/page.h>
#include <stdlib.h>
#include <stdio.h>
#define NPAGES 17
#define TEST_ADDRESS 0x20000000
#define MALLOC_SIZE 100

/* called from pt_test */
static void
do_pt_test(char *buf)
{
    /* set */
    for (int i = 0; i < NPAGES; i++) {
	    buf[i * PAGE_SIZE_4K] = i;
    }

    /* check */
    for (int i = 0; i < NPAGES; i++) {
	    assert(buf[i * PAGE_SIZE_4K] == i);
    }
}

void
pt_test( void )
{
    /* need a decent sized stack */
    char buf1[NPAGES * PAGE_SIZE_4K], *buf2 = NULL;

    /* check the stack is above phys mem */
    assert((void *) buf1 > (void *) TEST_ADDRESS);

    /* stack test */
    do_pt_test(buf1);

    printf("Mallocing\n");
    /* heap test */
    buf2 = malloc(100);
    assert(buf2 != NULL);

    for(char i=0;i<MALLOC_SIZE;++i) {
        buf2[(int) i] = i;
    }

    for(char i=0;i<MALLOC_SIZE;++i) {
        assert(buf2[(int) i] == i);
    }

    free(buf2);
}