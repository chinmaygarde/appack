#pragma once

#define PACK_TEMP_FAILURE_RETRY(expression)          \
  ({                                                 \
    intptr_t __result;                               \
    do {                                             \
      __result = (expression);                       \
    } while ((__result == -1L) && (errno == EINTR)); \
    __result;                                        \
  })
