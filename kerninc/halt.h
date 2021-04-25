#pragma once

#include <printf.h>

static inline void halt(){while(1);}
static inline void __haltmsg(char* s){printf(s); while(1);}

#define HALT(MESSAGE) __haltmsg(MESSAGE)

/* Big sleeps */
static inline void busy_loop(void){
    volatile int counter = 0;
    while(counter < 0xBFFFFFFF) counter += 1;  
}