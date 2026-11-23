// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
enum BootDumpCursorAction : uint8_t {
  BD_NORMAL, BD_HEADER, BD_FIRST, BD_LAST, BD_PREVIOUS_PAGE, BD_DISABLE
};
// Virtual header is never an index into the firmware's answers array.
inline BootDumpCursorAction bootDumpCursorAction(bool onHeader, uint8_t event,
    uint8_t choice, uint8_t count, bool firstPage, bool lastPage) {
  if(onHeader) {
    if(event==3) return BD_DISABLE;
    if(event==1) return BD_FIRST;
    if(event==2) return firstPage ? BD_LAST : BD_PREVIOUS_PAGE;
  } else if((event==2 && choice==0)
      || (event==1 && count && choice==count-1 && lastPage)) return BD_HEADER;
  return BD_NORMAL;
}
