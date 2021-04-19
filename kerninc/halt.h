#pragma once

#include <printf.h>

static inline void halt(){while(1);}
static inline void __haltmsg(char* s){printf(s); while(1);}

#define HALT(MESSAGE) __haltmsg(MESSAGE)