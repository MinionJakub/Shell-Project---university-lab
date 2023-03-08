#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#define DEBUG 0
#include "shell.h"

sigset_t sigchld_mask;

static void sigint_handler(int sig) {
  /* No-op handler, we just need break read() call with EINTR. */
  (void)sig;
}

/* Rewrite closed file descriptors to -1,
 * to make sure we don't attempt do close them twice. */
static void MaybeClose(int *fdp) {
  if (*fdp < 0)
    return;
  Close(*fdp);
  *fdp = -1;
}

/* Consume all tokens related to redirection operators.
 * Put opened file descriptors into inputp & output respectively. */
static int do_redir(token_t *token, int ntokens, int *inputp, int *outputp) {
  token_t mode = NULL; /* T_INPUT, T_OUTPUT or NULL */
  int n = 0;           /* number of tokens after redirections are removed */

  for (int i = 0; i < ntokens; i++) {
    /* TODO: Handle tokens and open files as requested. */
#ifdef STUDENT
    if (token[i] == NULL) {
      int j = i + 1;
      // for(j;token[j] == NULL && j < ntokens; j++){j=j;}
      while (token[j] == NULL && j < ntokens) {
        j++;
      }
      if (token[j] == NULL)
        break;
      token[i] = token[j];
      token[j] = NULL;
    }
    if (token[i] == T_INPUT) {
      MaybeClose(inputp);
      int what_to_open = i + 1;
      // for(what_to_open;token[what_to_open] == NULL && what_to_open < ntokens;
      // what_to_open++){what_to_open = what_to_open;}
      while (token[what_to_open] == NULL && what_to_open < ntokens) {
        what_to_open++;
      }
      *inputp = Open(token[what_to_open], O_RDONLY, 0);
      token[i] = mode;
      token[what_to_open] = mode;
      i--;
    } else if (token[i] == T_OUTPUT) {
      MaybeClose(outputp);
      int what_to_open = i + 1;
      // for(what_to_open;token[what_to_open] == NULL && what_to_open < ntokens;
      // what_to_open++){what_to_open = what_to_open;}
      while (token[what_to_open] == NULL && what_to_open < ntokens) {
        what_to_open++;
      }
      *outputp =
        Open(token[what_to_open], O_APPEND | O_CREAT | O_WRONLY | O_TRUNC,
             S_IRWXU | S_IRWXG | S_IRWXO);
      token[i] = mode;
      token[what_to_open] = mode;
      i--;
    } else {
      n++;
      //(void)MaybeClose;
    }

#endif /* !STUDENT */
  }

  token[n] = NULL;
  return n;
}

/* Execute internal command within shell's process or execute external command
 * in a subprocess. External command can be run in the background. */
static int do_job(token_t *token, int ntokens, bool bg) {
  int input = -1, output = -1;
  int exitcode = 0;

  ntokens = do_redir(token, ntokens, &input, &output);

  if (!bg) {
    if ((exitcode = builtin_command(token)) >= 0)
      return exitcode;
  }

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Start a subprocess, create a job and monitor it. */
#ifdef STUDENT

  pid_t forkling = Fork(); // tworzenie subprocesu

  if (forkling > 0) {
    setpgid(forkling, forkling); // ustawienie grupy nowo utworzonemu procesowi
                                 // w rodzicu by uniknac wyscigu
    if (bg == 0)
      setfgpgrp(forkling);
    int jobling = addjob(forkling, bg); // dodanie joba do joblisty
    addproc(jobling, forkling, token);
    if (bg == 0)
      monitorjob(&mask);
    if (bg == 1) {
      /*Obserwacja nie używać strncat ani realloc-a stos się szybko wywala,
       * warto również pamiętać o funkcjach danych odgórnie...*/
      safe_printf("[%d] running '%s'\n", jobling, jobcmd(jobling));
    }
  }
  if (forkling == 0) {
    setpgid(forkling,
            forkling); // ustawienie grupy w samym procesie by uniknac wyscigu
    if (bg == 0) {
      setfgpgrp(getpid());
    }
    signal(SIGINT, SIG_DFL);
    // signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    // signal(SIGCHLD,SIG_DFL); // flagi są ustawiane po to by nie robiło błędów
    // jak np.
    //  cat & daje bez nich /usr/bin/cat: -: Input/output error
    // Sigprocmask(SIG_UNBLOCK, &mask, NULL);
    if (input != -1) // jesli input inny niz standardowy to nalezy przepiac
    {
      Dup2(input, STDIN_FILENO);
      MaybeClose(&input);
    }
    if (output != -1) // jesli output inny niz standardowy to nalezy przepiac
    {
      Dup2(output, STDOUT_FILENO);
      MaybeClose(&output);
    }
    // if(bg == 0) safe_printf("Hello");
    external_command(token); // wywolanie funkcji
  }

#endif /* !STUDENT */

  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return exitcode;
}

/* Start internal or external command in a subprocess that belongs to pipeline.
 * All subprocesses in pipeline must belong to the same process group. */
static pid_t do_stage(pid_t pgid, sigset_t *mask, int input, int output,
                      token_t *token, int ntokens, bool bg) {
  ntokens = do_redir(token, ntokens, &input, &output);

  if (ntokens == 0)
    app_error("ERROR: Command line is not well formed!");

  /* TODO: Start a subprocess and make sure it's moved to a process group. */
  pid_t pid = Fork();
#ifdef STUDENT

  /*Okrojony job nie majacy kilku elementow*/
  pid_t forkling = pid;
  if (forkling > 0) {
    setpgid(forkling, pgid); // ustawienie grupy nowo utworzonemu procesowi w
                             // rodzicu by uniknac wyscigu
    // if(bg == 0)setfgpgrp(pgid);
    // if(bg == 0)monitorjob(&mask);
  }
  if (forkling == 0) {

    // printf("bg %d\n", bg);
    setpgid(forkling,
            pgid); // ustawienie grupy w samym procesie by uniknac wyscigu
    if (bg == 0) {
      setfgpgrp(getpid());
    }
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD,
           SIG_DFL); // flagi są ustawiane po to by nie robiło błędów jak np.
                     // cat & daje bez nich /usr/bin/cat: -: Input/output error
    if (input != -1) // jesli input inny niz standardowy to nalezy przepiac
    {
      Dup2(input, STDIN_FILENO);
      MaybeClose(&input);
    }
    if (output != -1) // jesli output inny niz standardowy to nalezy przepiac
    {
      Dup2(output, STDOUT_FILENO);
      MaybeClose(&output);
    }
    external_command(token); // wywolanie funkcji
  }

#endif /* !STUDENT */

  return pid;
}

static void mkpipe(int *readp, int *writep) {
  int fds[2];
  Pipe(fds);
  fcntl(fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(fds[1], F_SETFD, FD_CLOEXEC);
  *readp = fds[0];
  *writep = fds[1];
}

/* Pipeline execution creates a multiprocess job. Both internal and external
 * commands are executed in subprocesses. */
static int do_pipeline(token_t *token, int ntokens, bool bg) {
  pid_t pid, pgid = 0;
  int job = -1;
  int exitcode = 0;

  int input = -1, output = -1, next_input = -1;

  mkpipe(&next_input, &output);

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Start pipeline subprocesses, create a job and monitor it.
   * Remember to close unused pipe ends! */
#ifdef STUDENT
  unsigned long long int len = 0;
  // bool first_time = true;
  int i = 0;
  while (token[len] != NULL && len <= ntokens) {
    i++;
    unsigned long long int divide = 0;
    while (token[divide + len] != NULL && token[divide + len] != T_PIPE) {
      if (token[divide + len] == NULL)
        break;
      divide++;
    }
    char **argv = malloc((sizeof(char *)) * (divide + 1));
    for (unsigned long long int i = 0; i < divide; i++) {
      argv[i] = token[len + i];
    }
    argv[divide] = NULL;

    if (len + divide + 1 > ntokens) {
      MaybeClose(&next_input);
      MaybeClose(&output);
      // output = -1;
      pid = do_stage(pgid, &mask, input, output, argv, divide, bg);
      MaybeClose(&input);
      addproc(job, pid, argv);
      free(argv);
      if (bg == 0) {
        setfgpgrp(pid);
        monitorjob(&mask);
      }
      if (bg == 1) {
        safe_printf("[%d] running '%s'\n", job, jobcmd(job));
      }
      break;
    } else {

      pid = do_stage(pgid, &mask, input, output, argv, divide, 1);
      if (!len) {
        pgid = pid;
        job = addjob(pgid, bg);
      }
      addproc(job, pid, argv);
      free(argv);
      MaybeClose(&input);
      MaybeClose(&output);
      input = next_input;
      // MaybeClose(&next_input);
      mkpipe(&next_input, &output);
    }
    len = len + divide + 1;
  }
#endif /* !STUDENT */

  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return exitcode;
}

static bool is_pipeline(token_t *token, int ntokens) {
  for (int i = 0; i < ntokens; i++)
    if (token[i] == T_PIPE)
      return true;
  return false;
}

static void eval(char *cmdline) {
  bool bg = false;
  int ntokens;
  token_t *token = tokenize(cmdline, &ntokens);

  if (ntokens > 0 && token[ntokens - 1] == T_BGJOB) {
    token[--ntokens] = NULL;
    bg = true;
  }

  if (ntokens > 0) {
    if (is_pipeline(token, ntokens)) {
      do_pipeline(token, ntokens, bg);
    } else {
      do_job(token, ntokens, bg);
    }
  }

  free(token);
}

#ifndef READLINE
static char *readline(const char *prompt) {
  static char line[MAXLINE]; /* `readline` is clearly not reentrant! */

  write(STDOUT_FILENO, prompt, strlen(prompt));

  line[0] = '\0';

  ssize_t nread = read(STDIN_FILENO, line, MAXLINE);
  if (nread < 0) {
    if (errno != EINTR)
      unix_error("Read error");
    msg("\n");
  } else if (nread == 0) {
    return NULL; /* EOF */
  } else {
    if (line[nread - 1] == '\n')
      line[nread - 1] = '\0';
  }

  return strdup(line);
}
#endif

int main(int argc, char *argv[]) {
  /* `stdin` should be attached to terminal running in canonical mode */
  if (!isatty(STDIN_FILENO))
    app_error("ERROR: Shell can run only in interactive mode!");

#ifdef READLINE
  rl_initialize();
#endif

  sigemptyset(&sigchld_mask);
  sigaddset(&sigchld_mask, SIGCHLD);

  if (getsid(0) != getpgid(0))
    Setpgid(0, 0);

  initjobs();

  struct sigaction act = {
    .sa_handler = sigint_handler,
    .sa_flags = 0, /* without SA_RESTART read() will return EINTR */
  };
  Sigaction(SIGINT, &act, NULL);

  Signal(SIGTSTP, SIG_IGN);
  Signal(SIGTTIN, SIG_IGN);
  Signal(SIGTTOU, SIG_IGN);

  while (true) {
    char *line = readline("# ");

    if (line == NULL)
      break;

    if (strlen(line)) {
#ifdef READLINE
      add_history(line);
#endif
      eval(line);
    }
    free(line);
    watchjobs(FINISHED);
  }

  msg("\n");
  shutdownjobs();

  return 0;
}
