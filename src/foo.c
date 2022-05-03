#include "foo.h"
#include <stdio.h>

void SayHello(void) {
#ifdef NDEBUG
    printf("hello release\n");
#else
    printf("hello debug\n");
#endif
}
