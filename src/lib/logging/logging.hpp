// SPDX-License-Identifier: GPL-3.0+

#pragma once


#ifndef LOGGING_UART
#define LOGGING_UART Serial
#endif

extern void debugPrintf(const char* fmt, ...);


#if defined(DEBUG_LOG) 
    #define DBG(msg, ...)   debugPrintf(msg, ##__VA_ARGS__)
    #define DBGLN(msg, ...) { \
      debugPrintf(msg, ##__VA_ARGS__); \
      LOGGING_UART.println(); \
    }

#else
  #define DBG(...)
  #define DBGLN(...)

#endif

