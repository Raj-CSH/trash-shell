#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <trash/builtins.h>
#include <trash/interpreter.h>
#include <trash/utils.h>

#define ALLOC_FPATH 256
#define ALLOC_TOKENS 8

#define CL_FPATH 3
#define MV_REDISP 2
#define CL_REDIS 1
#define NO_REDIS 0

#define CBUFLEN 64
#define EBUFLEN 32
#define TBUFLEN 32

#define MODE S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH

typedef struct {
  unsigned envvar : 1;
  unsigned escaped : 1;
  unsigned redirect : 2;
  unsigned tilde : 1;
  unsigned quoted : 1;
  char qchar;
} parser_flags;

static void *redis_init(redirect *redis) {
  if (redis == NULL)
    return NULL;
  redis->fpath = malloc(ALLOC_FPATH);
  if (redis->fpath == NULL)
    return NULL;
  strcpy(redis->fpath, "");
  redis->fd = 1;
  return redis;
}

static void command_init(command *cmd) {
  if (cmd == NULL)
    return;
  cmd->cmd_next = NULL;
  cmd->tokens = calloc(sizeof *cmd->tokens, ALLOC_TOKENS);
  cmd->redis = NULL;
}

static char *expand_envvar(char *buffer) { /* recursive */
  size_t ebuflen = EBUFLEN, cbuflen = CBUFLEN;
  char *bufp = buffer, *dollar = strchr(buffer, '$'), *varp = dollar + 1,
       *ebuffer = malloc(ebuflen), *ebufp = ebuffer, *cbuffer = malloc(cbuflen),
       *cbufp = cbuffer;

  ERR_ALLOC(IS_NULL(cbuffer) || IS_NULL(ebuffer));
  if (dollar == NULL)
    return buffer;
  for (; isalnum(*varp); ++varp) {
    *ebufp++ = *varp;
    CGROW(ebufp, ebuffer, ebuflen);
  }
  *ebufp = '\0';
  ebufp = getenv(ebuffer) == NULL ? "" : getenv(ebuffer);
  free(ebuffer);
  ebuffer = ebufp;
  for (; bufp != dollar; ++bufp) {
    *cbufp++ = *bufp;
    CGROW(cbufp, cbuffer, cbuflen);
  }
  for (; *ebufp != '\0'; ++ebufp) {
    *cbufp++ = *ebufp;
    CGROW(cbufp, cbuffer, cbuflen);
  }
  for (; *varp != '\0'; ++varp) {
    *cbufp++ = *varp;
    CGROW(cbufp, cbuffer, cbuflen);
  }
  *cbufp = '\0';

  return expand_envvar(cbuffer);
}

static char *expand_tilde(char *buffer) {
  size_t cbuflen = CBUFLEN, ebuflen = EBUFLEN;
  char *bufp = buffer + 1, *cbuffer = malloc(cbuflen), *cbufp = cbuffer,
       *ebuffer = malloc(ebuflen), *ebufp = ebuffer;
  struct passwd *p;

  ERR_ALLOC(IS_NULL(cbuffer) || IS_NULL(ebuffer));

  if (*buffer != '~')
    return buffer;

  if (*bufp != '\0' && *bufp != '/') {
    for (; *bufp != '\0' && *bufp != '/'; ++bufp) {
      *ebufp++ = *bufp;
      CGROW(ebufp, ebuffer, ebuflen);
    }
    *ebufp = '\0';
    p = getpwnam(ebuffer);
    if (p == NULL) {
      fprintf(stderr, "trash: no such user or named directory: %s\n", ebuffer);
      return buffer;
    }
    for (char *dirp = p->pw_dir; *dirp != '\0'; ++dirp) {
      *cbufp++ = *dirp;
      CGROW(cbufp, cbuffer, cbuflen);
    }
  } else if (isdigit(*bufp)) {
    if (*bufp - '0' == 0) {
      do {
        errno = 0;
        getcwd(cbuffer, cbuflen);
        if (errno == ERANGE) {
          cbuflen *= 2;
          cbuffer = realloc(cbuffer, cbuflen);
          ERR_ALLOC(IS_NULL(cbuffer));
        }
      } while (errno == ERANGE);
      cbufp = strchr(cbuffer, '\0');
    } else if (*bufp - '0' == 1) {
      p = getpwuid(getuid());
      if (p == NULL) {
        fputs("trash: unable to get user info.\n", stderr);
        exit(1);
      }
      for (char *envp = p->pw_dir; *envp != '\0'; ++envp) {
        *cbufp++ = *envp;
        CGROW(cbufp, cbuffer, cbuflen);
      }
    } else {
      fputs("trash: not enough directory stack entries.\n", stderr);
      return buffer;
    }
    ++bufp;
  } else {
    p = getpwuid(getuid());
    if (p == NULL) {
      fputs("trash: unable to get user info\n", stderr);
      exit(1);
    }
    for (char *envp = p->pw_dir; *envp != '\0'; ++envp) {
      *cbufp++ = *envp;
      CGROW(cbufp, cbuffer, cbuflen);
    }
  }

  for (; *bufp != '\0'; ++bufp) {
    *cbufp++ = *bufp;
    CGROW(cbufp, cbuffer, cbuflen);
  }
  *cbufp = '\0';
  return cbuffer;
}

command *parse(const char *buffer) { /* recursive */
  size_t tbuflen = TBUFLEN, alloc_fpath = ALLOC_FPATH,
         alloc_tokens = ALLOC_TOKENS;
  command *cmd = malloc(sizeof *cmd);
  char *envvar = NULL, *tbuffer = malloc(tbuflen), *tbufp = tbuffer,
       **tokenp = NULL;
  parser_flags flags = {.envvar = 0,
                        .escaped = 0,
                        .redirect = NO_REDIS,
                        .quoted = 0,
                        .qchar = '\0'};

  command_init(cmd);
  ERR_ALLOC(IS_NULL(tbuffer) || IS_NULL(cmd) || IS_NULL(cmd->tokens));

  tokenp = cmd->tokens;
  for (const char *bufp = buffer; *bufp != '\0'; ++bufp) {
    if (isspace(*bufp)) {
      if (bufp[1] == '\0' || (!flags.escaped && !flags.quoted)) {
        if (flags.redirect == CL_FPATH) {
          *tbufp = '\0';
          cmd->redis->fpath =
              flags.envvar ? strdup(expand_envvar(tbuffer)) : strdup(tbuffer);
          cmd->redis->fpath =
              flags.tilde ? strdup(expand_tilde(tbuffer)) : strdup(tbuffer);
          tbufp = tbuffer;
          flags.envvar = 0;
          flags.tilde = 0;
          continue;
        } else if (flags.redirect == MV_REDISP) {
          *tbufp = '\0';
          if (strlen(tbuffer) > 0) {
            tbuffer = flags.envvar ? expand_envvar(tbuffer) : tbuffer;
            tbuffer = flags.tilde ? expand_tilde(tbuffer) : tbuffer;
            flags.envvar = 0;
            flags.tilde = 0;
            *tokenp++ = strdup(tbuffer);
            CGROW(tokenp, cmd->tokens, alloc_tokens);
          }
          tbufp = tbuffer;
          flags.redirect = CL_FPATH;
          continue;
        } else if (flags.redirect != NO_REDIS) {
          fputs("trash: malformed buffer.\n", stderr);
          exit(1);
        }
        *tbufp = '\0';
        if (strlen(tbuffer) == 0) {
          tbufp = tbuffer;
          continue;
        }
        tbuffer = flags.envvar ? expand_envvar(tbuffer) : tbuffer;
        tbuffer = flags.tilde ? expand_tilde(tbuffer) : tbuffer;
        flags.envvar = 0;
        flags.tilde = 0;
        *tokenp++ = strdup(tbuffer);
        CGROW(tokenp, cmd->tokens, alloc_tokens);
        tbufp = tbuffer;
        continue;
      } else
        flags.escaped = 0;
    } else if (*bufp == '\"' || *bufp == '\'') {
      if (!flags.escaped && flags.quoted && flags.qchar == *bufp) {
        flags.quoted = 0;
        continue;
      } else if (!flags.escaped && !flags.quoted) {
        flags.quoted = 1;
        flags.qchar = *bufp;
        continue;
      } else
        flags.escaped = 0;
    } else if (*bufp == '\\') {
      if (!flags.escaped) {
        flags.escaped = 1;
        continue;
      } else
        flags.escaped = 0;
    } else if (*bufp == '$') {
      if (!flags.escaped)
        flags.envvar = 1;
      else
        flags.escaped = 0;
    } else if (*bufp == '|') {
      if (!flags.escaped && !flags.quoted) {
        *tokenp = NULL;
        if (bufp[1] != '\0')
          cmd->cmd_next = parse(++bufp);
        else {
          fputs("trash: syntax error.\n", stderr);
          continue;
        }
        return cmd;
      } else
        flags.escaped = 0;
    } else if (*bufp == '>' || *bufp == '<') {
      if (!flags.escaped && !flags.quoted) {
        if (IS_NULL(cmd->redis)) {
          cmd->redis = malloc(sizeof *cmd->redis);
          redis_init(cmd->redis);
          ERR_ALLOC(IS_NULL(cmd->redis) || IS_NULL(cmd->redis->fpath));
        }

        if (flags.redirect == CL_REDIS && *(bufp - 1) == *bufp) {
          if (*bufp == '>') {
            cmd->redis->oflag = O_WRONLY | O_APPEND | O_CREAT;
            cmd->redis->fd = cmd->redis->fd == 2 ? 2 : 1;
          } else {
            cmd->redis->oflag = O_RDONLY;
            cmd->redis->fd = 0;
          }
          flags.redirect = MV_REDISP;
        } else if (flags.redirect != NO_REDIS) {
          fputs("trash: syntax error.\n", stderr);
          continue;
        } else if (bufp[1] != '\0') {
          if (bufp[1] != *bufp) {
            cmd->redis->oflag = *bufp == '>' ? O_WRONLY | O_CREAT : O_RDONLY;
            cmd->redis->fd = *bufp == '>' ? cmd->redis->fd : 0;
            flags.redirect = MV_REDISP;
          } else
            flags.redirect = CL_REDIS;
        }
        continue;
      } else
        flags.escaped = 0;
    } else if (*bufp == '~') {
      if (!flags.escaped && !flags.quoted) {
        if (bufp[1] == '\0') {
          flags.tilde = 1;
        } else if (isspace(bufp[1]) || isalnum(bufp[1]) || bufp[1] == '/' ||
                   bufp[1] == '<' || bufp[1] == '>' || bufp[1] == '|') {
          flags.tilde = 1;
        }
      } else
        flags.escaped = 0;
    }

    if (flags.redirect == MV_REDISP) {
      *tbufp = '\0';
      if (strlen(tbuffer) > 0) {
        tbuffer = flags.envvar ? expand_envvar(tbuffer) : tbuffer;
        tbuffer = flags.tilde ? expand_tilde(tbuffer) : tbuffer;
        *tokenp++ = strdup(tbuffer);
        CGROW(tokenp, cmd->tokens, alloc_tokens);
      }
      tbufp = tbuffer;
      flags.redirect = CL_FPATH;
    } else if (flags.redirect == CL_REDIS) {
      fputs("trash: malformed buffer.\n", stderr);
      exit(1);
    }
    if ((0 <= *bufp - '0') && (*bufp - '0' <= 9) && !flags.quoted &&
        bufp[1] != '\0') {
      if (bufp[1] == '>') {
        cmd->redis->fd = (int)*bufp - '0';
        printf("%d", *bufp - '0');
      } else if (bufp[1] == '<') {
        cmd->redis->fd = 0;
      }
    }
    *tbufp++ = *bufp;
    CGROW(tbufp, tbuffer, tbuflen);
    flags.escaped = 0;
  }

  if (flags.redirect == CL_FPATH) {
    *tbufp = '\0';
    cmd->redis->fpath =
        flags.envvar ? strdup(expand_envvar(tbuffer)) : strdup(tbuffer);
    cmd->redis->fpath =
        flags.tilde ? strdup(expand_tilde(tbuffer)) : strdup(tbuffer);
    flags.envvar = 0;
    flags.tilde = 0;
  } else if (flags.redirect == MV_REDISP) {
    fputs("trash: syntax error.\n", stderr);
  } else if (flags.redirect != NO_REDIS) {
    fputs("trash: malformed buffer.\n", stderr);
    exit(1);
  } else {
    *tbufp = '\0';
    if (strlen(tbuffer) > 0) {
      tbuffer = flags.envvar ? expand_envvar(tbuffer) : tbuffer;
      tbuffer = flags.tilde ? expand_tilde(tbuffer) : tbuffer;
      flags.envvar = 0;
      flags.tilde = 0;
      *tokenp++ = strdup(tbuffer);
      CGROW(tokenp, cmd->tokens, alloc_tokens);
    }
  }

  *tokenp = NULL;
  free(tbuffer);

  return cmd;
}

static int interp(command *cmd, bool new_proc) {
  int exit_status = 0;

  if (builtins(cmd, &exit_status) < 0) {
    if (new_proc && (fork() == 0)) {
      execvp(*cmd->tokens, cmd->tokens);
      fprintf(stderr, "trash: command not found: %s\n", *cmd->tokens);
      exit(1);
    } else if (new_proc) {
      wait(&exit_status);
    } else {
      execvp(*cmd->tokens, cmd->tokens);
      fprintf(stderr, "trash: command not found: %s\n", *cmd->tokens);
      exit_status = 1;
    }
  }

  return exit_status;
}

int interpret(command *cmd) {
  int exit_status = 0, redirect_fd = -1, target_fd = -1, stdin_fd = 0,
      stdout_fd = 1, pipefd[2];
  if (cmd->redis != NULL) {
    redirect_fd = open(cmd->redis->fpath, cmd->redis->oflag, MODE);
    if (redirect_fd < 0) {
      fprintf(stderr, "trash: %s: %s\n", strerror(errno), cmd->redis->fpath);
      return 1;
    }
    target_fd = dup(cmd->redis->fd);
    dup2(redirect_fd, cmd->redis->fd);
  }
  if (*cmd->tokens == NULL)
    return 1;

  if (cmd->cmd_next == NULL)
    exit_status = interp(cmd, true);
  else {
    pipe(pipefd);
    if (fork() == 0) {
      dup2(pipefd[1], 1);
      close(pipefd[0]);
      exit(interp(cmd, false));
    }
    if (fork() == 0) {
      dup2(pipefd[0], 0);
      close(pipefd[1]);
      if (exit_status == 0)
        exit(interpret(cmd->cmd_next));
      else
        exit(exit_status);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    wait(&exit_status);
    wait(&exit_status);
  }

  if (target_fd > 0) {
    dup2(target_fd, cmd->redis->fd);
    close(redirect_fd);
    close(target_fd);
  }
  return exit_status;
}
