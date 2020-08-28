#ifndef TRASH_INTERPRETER_H
#define TRASH_INTERPRETER_H

#include <stddef.h>

typedef struct {
  char *fpath;
  int fd;
  int oflag;
} redirect;

typedef struct command {
  struct command *cmd_next;
  char **tokens;
  redirect *redis;
} command;

command *parse(const char *);
int interpret(command *cmd);

#endif
