#ifndef __DEBUG_H
#define __DEBUG_H

#define  RTT_DEBUG 0

#if RTT_DEBUG
    #include "RTT/SEGGER_RTT.h"
    #define DEBUG(sFormat, ...) SEGGER_RTT_printf(0, (sFormat), ##__VA_ARGS__)
#else
    #define DEBUG(sFormat, ...) 
#endif

#endif // debug.h
