#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <signal.h>
#include <unistd.h>

#include <trash/utils.h>

#define AUXSIG SIGUSR1

static pid_t parent_pid;

static void auxsig_handler(int signo) {
  pid_t pid = getpid();
  if (parent_pid != pid)
    exit(0);
}

static void sigint_handler(int signo) { kill(-parent_pid, AUXSIG); }

void init(void) {
  struct sigaction si, sa;

  si.sa_handler = sigint_handler;
  sigemptyset(&si.sa_mask);
  si.sa_flags = 0;
  sa.sa_handler = auxsig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  rl_bind_key('\t', rl_menu_complete);
  parent_pid = getpid();
  if (sigaction(SIGINT, &si, NULL) == -1) {
    fputs("trash: unable to set signal handler.\n", stderr);
    exit(1);
  }
  if (sigaction(AUXSIG, &sa, NULL) == -1) {
    fputs("trash: unable to set signal handler.\n", stderr);
    exit(1);
  }
}
