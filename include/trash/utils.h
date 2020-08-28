#ifndef TRASH_UTILS_H
#define TRASH_UTILS_H

#include <unistd.h>

#define ERR_ALLOC_MSG "trash: allocation error.\n"

#define IS_NULL(PTR) (PTR == NULL)

#define ERR_ALLOC(COND)                                                        \
  if (COND) {                                                                  \
    fputs(ERR_ALLOC_MSG, stderr);                                              \
    exit(1);                                                                   \
  }

#define CGROW(BUFPTR, BUFFER, BUFLEN)                                          \
  do {                                                                         \
    if ((BUFPTR - BUFFER) >= BUFLEN - 1) {                                     \
      BUFLEN *= 2;                                                             \
      BUFFER = realloc(BUFFER, BUFLEN);                                        \
      BUFPTR = BUFFER + (BUFLEN / 2) - 1;                                      \
      ERR_ALLOC(IS_NULL(BUFFER));                                              \
    }                                                                          \
  } while (0);

void init(void);
void restore_signal_handler(void);

#endif
