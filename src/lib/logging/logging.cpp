// SPDX-License-Identifier: GPL-3.0+

#include <Arduino.h>
#include <cstdarg>
#include "logging.hpp"

void debugPrintf(const char* fmt, ...)
{
  char c;
  va_list  vlist;
  va_start(vlist,fmt);
  char buf[80];

  if (vsnprintf(buf, sizeof(buf), fmt, vlist) >= sizeof(buf)) {
      buf[sizeof(buf)-1] = 0;
      buf[sizeof(buf)-2] = '.';
      buf[sizeof(buf)-3] = '.';
      buf[sizeof(buf)-4] = '.';
  }
  LOGGING_UART.print(buf);

  va_end(vlist);
}
