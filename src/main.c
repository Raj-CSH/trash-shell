#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include <trash/interpreter.h>
#include <trash/utils.h>

#define ALLOC_STATUS 2
#define PROMPT_BASE "trash $ "
#define BUFLEN 128
#define PROMPTLEN 64

int main(void) {
  int exit_status = 0;
  char *prompt = malloc(PROMPTLEN), *line;

  ERR_ALLOC(IS_NULL(prompt));
  snprintf(prompt, PROMPTLEN, "%s", PROMPT_BASE);

  init();

  while ((line = readline(prompt))) {
    if (strlen(line) == 0)
      continue;
    add_history(line);
    exit_status = interpret(parse(line));
    if (exit_status != 0)
      snprintf(prompt, PROMPTLEN, "(%d) %s", exit_status, PROMPT_BASE);
    else
      snprintf(prompt, PROMPTLEN, "%s", PROMPT_BASE);
    free(line);
  }

  free(prompt);
}
