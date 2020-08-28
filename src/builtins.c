#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <trash/interpreter.h>
#include <trash/utils.h>

#define CBUFLEN 64

static void b_cd(command *cmd, int *exit_status) {
  char *var;
  if (cmd->tokens[1] == NULL) {
    var = getenv("HOME");
    if (var == NULL) {
      fputs("cd: too few arguments\n", stderr);
      return;
    }
    cmd->tokens[1] = strdup(var);
  }
  *exit_status = chdir(cmd->tokens[1]);
  if (*exit_status < 0) {
    fprintf(stderr, "cd: %s: %s\n", strerror(errno), cmd->tokens[1]);
    errno = 0;
  }
}

static void b_echo(command *cmd, int *exit_status) {
  size_t cbuflen = CBUFLEN;
  char *cbuffer = malloc(cbuflen), *cbufp = cbuffer, *tbufp;

  ERR_ALLOC(IS_NULL(cbuffer));

  if (cmd->tokens[1] == NULL) {
    cmd->tokens[1] = "";
  }
  for (tbufp = cmd->tokens[1]; *tbufp != '\0'; ++tbufp) {
    *cbufp++ = *tbufp;
    CGROW(cbufp, cbuffer, cbuflen);
  }
  *cbufp = '\0';
  puts(cbuffer);
  *exit_status = 0;
}

static void b_exit(command *cmd, int *exit_status) {
  *exit_status = 0;
  exit(0);
}

static void b_export(command *cmd, int *exit_status) {
  char *varname, *varval;

  for (char **tokenp = cmd->tokens + 1; *tokenp != NULL; ++tokenp) {
    varname = strtok(strdup(*tokenp), "=");
    varval = strtok(NULL, "=");
    if (varval == NULL) {
      fprintf(stderr, "export: syntax error: %s\n", *tokenp);
      *exit_status = 1;
      return;
    }
    setenv(varname, varval, 1);
  }

  *exit_status = 0;
}

ssize_t builtins(command *cmd, int *exit_status) {
  if (strcmp(*cmd->tokens, "cd") == 0)
    b_cd(cmd, exit_status);
  else if (strcmp(*cmd->tokens, "echo") == 0)
    b_echo(cmd, exit_status);
  else if (strcmp(*cmd->tokens, "exit") == 0)
    b_exit(cmd, exit_status);
  else if (strcmp(*cmd->tokens, "export") == 0)
    b_export(cmd, exit_status);
  else
    return -1;
  return 0;
}
