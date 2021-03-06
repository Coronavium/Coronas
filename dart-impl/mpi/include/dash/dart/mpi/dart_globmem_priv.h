#include <stdint.h>

extern int16_t dart_memid;
extern int16_t dart_registermemid;

#define DART_GPTR_COPY(gptr_, gptrt_)                       \
  do {                                                      \
    gptr_.unitid = gptrt_.unitid;                           \
    gptr_.segid  = gptrt_.segid;                            \
    gptr_.flags  = gptrt_.flags;                            \
    gptr_.addr_or_offs.offset = gptrt_.addr_or_offs.offset; \
  } while(0)
