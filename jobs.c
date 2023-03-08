#include "shell.h"

typedef struct proc {
  pid_t pid;    /* process identifier */
  int state;    /* RUNNING or STOPPED or FINISHED */
  int exitcode; /* -1 if exit status not yet received */
} proc_t;

typedef struct job {
  pid_t pgid;            /* 0 if slot is free */
  proc_t *proc;          /* array of processes running in as a job */
  struct termios tmodes; /* saved terminal modes */
  int nproc;             /* number of processes */
  int state;             /* changes when live processes have same state */
  char *command;         /* textual representation of command line */
} job_t;

static job_t *jobs = NULL;          /* array of all jobs */
static int njobmax = 1;             /* number of slots in jobs array */
static int tty_fd = -1;             /* controlling terminal file descriptor */
static struct termios shell_tmodes; /* saved shell terminal modes */

static void sigchld_handler(int sig) {
  int old_errno = errno;
  pid_t pid;
  int status;
  /* TODO: Change state (FINISHED, RUNNING, STOPPED) of processes and jobs.
   * Bury all children that finished saving their status in jobs. */
#ifdef STUDENT
  for (int i = 0; i < njobmax; i++) {
    for (int j = 0; j < jobs[i].nproc; j++) {

      pid = waitpid(jobs[i].proc[j].pid, &status,
                    WNOHANG | WUNTRACED |
                      WCONTINUED); // sprawdzenie czy dany proces działa,jest
                                   // zatrzymany czy też zakończył działanie
      // safe_printf("%d %d\n",jobs[i].proc[j].pid,status);
      // safe_printf("%d\n",status);
      if (pid == jobs[i].proc[j].pid) {

        if (WIFSTOPPED(status)) {
          jobs[i].proc[j].state = STOPPED;
          jobs[i].proc[j].exitcode = -1;
          if (j == jobs[i].nproc - 1)
            jobs[i].state = jobs[i].proc[j].state;
        } else if (WIFEXITED(status)) {
          // printf("pid: %d\n", pid);
          jobs[i].proc[j].state = FINISHED;
          jobs[i].proc[j].exitcode = status; // nadanie exitcode
          if (j == jobs[i].nproc - 1)
            jobs[i].state = jobs[i].proc[j].state;
        } else if (WIFSIGNALED(status)) {
          // safe_printf("SIEMA\n"); - testowanie
          jobs[i].proc[j].state = FINISHED;
          jobs[i].proc[j].exitcode = status; // nadanie exitcode
          if (j == jobs[i].nproc - 1)
            jobs[i].state = jobs[i].proc[j].state;
        } else if (WIFCONTINUED(status)) {
          jobs[i].proc[j].state = RUNNING;
          jobs[i].proc[j].exitcode = -1;
          if (j == jobs[i].nproc - 1)
            jobs[i].state = jobs[i].proc[j].state;
        }
      }
      // jobs[i].state = jobs[i].proc[jobs[i].nproc - 1].state; //ustawienie
      // stanu pipe/joba w zależności od ostatniego procesu

      // poniżej wersja jak pisałem gdy nie myslałem od pipe-ach...
      //  if(jobs[i].nproc == 1){
      //    if(WIFEXITED(status)){
      //      jobs[i].state = FINISHED;
      //    }
      //    else if(WIFSIGNALED(status)){
      //      jobs[i].state = FINISHED;
      //    }
      //    else if(WIFSTOPPED(status)){
      //      jobs[i].state = STOPPED;
      //    }
      //    else if(WIFCONTINUED(status)){
      //      jobs[i].state = RUNNING;
      //    }
      //  }
      //  else{
      //    /*TODO: */
      //    jobs[i].state = jobs[i].proc[jobs[i].nproc - 1].state;
      //  }
    }
  }
#endif /* !STUDENT */
  errno = old_errno;
}

/* When pipeline is done, its exitcode is fetched from the last process. */
static int exitcode(job_t *job) {
  return job->proc[job->nproc - 1].exitcode;
}

static int allocjob(void) {
  /* Find empty slot for background job. */
  for (int j = BG; j < njobmax; j++)
    if (jobs[j].pgid == 0)
      return j;

  /* If none found, allocate new one. */
  jobs = realloc(jobs, sizeof(job_t) * (njobmax + 1));
  memset(&jobs[njobmax], 0, sizeof(job_t));
  return njobmax++;
}

static int allocproc(int j) {
  job_t *job = &jobs[j];
  job->proc = realloc(job->proc, sizeof(proc_t) * (job->nproc + 1));
  return job->nproc++;
}

int addjob(pid_t pgid, int bg) {
  int j = bg ? allocjob() : FG;
  job_t *job = &jobs[j];
  /* Initial state of a job. */
  job->pgid = pgid;
  job->state = RUNNING;
  job->command = NULL;
  job->proc = NULL;
  job->nproc = 0;
  job->tmodes = shell_tmodes;
  return j;
}

static void deljob(job_t *job) {
  assert(job->state == FINISHED);
  free(job->command);
  free(job->proc);
  job->pgid = 0;
  job->command = NULL;
  job->proc = NULL;
  job->nproc = 0;
}

static void movejob(int from, int to) {
  assert(jobs[to].pgid == 0);
  memcpy(&jobs[to], &jobs[from], sizeof(job_t));
  memset(&jobs[from], 0, sizeof(job_t));
}

static void mkcommand(char **cmdp, char **argv) {
  if (*cmdp)
    strapp(cmdp, " | ");

  for (strapp(cmdp, *argv++); *argv; argv++) {
    strapp(cmdp, " ");
    strapp(cmdp, *argv);
  }
}

void addproc(int j, pid_t pid, char **argv) {
  assert(j < njobmax);
  job_t *job = &jobs[j];

  int p = allocproc(j);
  proc_t *proc = &job->proc[p];
  /* Initial state of a process. */
  proc->pid = pid;
  proc->state = RUNNING;
  proc->exitcode = -1;
  mkcommand(&job->command, argv);
}

/* Returns job's state.
 * If it's finished, delete it and return exitcode through statusp. */
static int jobstate(int j, int *statusp) {
  assert(j < njobmax);
  job_t *job = &jobs[j];
  int state = job->state;

  /* TODO: Handle case where job has finished. */
#ifdef STUDENT
  if (state == FINISHED) {
    // int exitcode = jobs[j].proc[jobs[j].nproc-1].exitcode;
    // proc_t processling = jobs[j].proc[jobs[j].nproc-1];
    // int status = processling.exitcode;
    int status = exitcode(&jobs[j]); // wczytanie kodu wejścia
    *statusp = status;               // ustawienie odpowiednio wyjścia
    deljob(&jobs[j]);                // sprzątanie po trupie
  } else {
    if (statusp != NULL)
      *statusp =
        -1; // jeśli jest stopped lub running to exitcode ma się równać -1
  }
#endif /* !STUDENT */

  return state;
}

char *jobcmd(int j) {
  assert(j < njobmax);
  job_t *job = &jobs[j];
  return job->command;
}

/* Continues a job that has been stopped. If move to foreground was requested,
 * then move the job to foreground and start monitoring it. */
bool resumejob(int j, int bg, sigset_t *mask) {
  if (j < 0) {
    for (j = njobmax - 1; j > 0 && jobs[j].state == FINISHED; j--)
      continue;
  }

  if (j >= njobmax || jobs[j].state == FINISHED)
    return false;

    /* TODO: Continue stopped job. Possibly move job to foreground slot. */
#ifdef STUDENT
  if (!bg) {
    // safe_printf("continue '%s'",jobs[j].command); - testowanie
    if (jobs[0].pgid != 0)
      kill(-jobs[0].pgid, SIGSTOP);
    safe_printf("continue '%s'\n", jobs[j].command); // wymaganie testów
    movejob(j, 0);                                   // ustawienie joba na FG
    setfgpgrp(jobs[0].pgid); // powiedzenie kto ma dostep do terminala
    Tcsetattr(tty_fd, TCSADRAIN,
              &jobs[0].tmodes);   // ustawienie atrybutow shell-a pod process
                                  // ktory bedzie w pierwszoplanowy
    kill(-jobs[0].pgid, SIGCONT); // wznowienie go
                                  // while (jobs[0].state != RUNNING) {
    Sigsuspend(mask);
    //}
    monitorjob(mask); // monitorowanie (PS. Wielki Brat patrzy)
  } else {
    safe_printf("continue '%s'\n", jobs[j].command);
    kill(-jobs[j].pgid, SIGCONT);
  }
  /*Zmuszenie obijających się pracowników do pracy czasem wraz z promocją na
   * tymczasowego szefa*/
#endif /* !STUDENT */

  return true;
}

/* Kill the job by sending it a SIGTERM. */
bool killjob(int j) {
  if (j >= njobmax || jobs[j].state == FINISHED)
    return false;
  debug("[%d] killing '%s'\n", j, jobs[j].command);

  /* TODO: I love the smell of napalm in the morning. */
#ifdef STUDENT
  // kill(-jobs[j].pgid,SIGCONT); // pierwotnie tak napisałem ale zapompniałem o
  // głupim edge case...
  kill(-jobs[j].pgid, SIGTERM); // morderstwo całego joba (i jego grupy)
  kill(-jobs[j].pgid, SIGCONT); // wskrzeszenie joba aby potem zlozyc go w
                                // ofierze mrocznym bogom Linuxa

#endif /* !STUDENT */

  return true;
}

/* Report state of requested background jobs. Clean up finished jobs. */
void watchjobs(int which) {
  for (int j = BG; j < njobmax; j++) {
    if (jobs[j].pgid == 0)
      continue;

      /* TODO: Report job number, state, command and exit code or signal. */
#ifdef STUDENT
    //(void)deljob;
    job_t jobling = jobs[j];
    char *command = strdup(jobling.command);
    int exitcodling;
    int status = jobstate(j, &exitcodling);
    // safe_printf("%d\n",status);
    // if(status == which) safe_printf("Job number: %d \t Status: %d \t Command:
    // %s \t Exitcode: %d\n",i,status,command,exitcodling);
    if (status == FINISHED && (which == FINISHED || which == ALL)) {
      if (WIFSIGNALED(exitcodling)) {
        safe_printf("[%d] killed '%s' by signal %d\n", j, command,
                    WTERMSIG(exitcodling));
      } else if (WIFEXITED(exitcodling)) {
        safe_printf("[%d] exited '%s', status=%d\n", j, command,
                    WEXITSTATUS(exitcodling));
      }
    } else if (status == STOPPED && (which == STOPPED || which == ALL)) {
      safe_printf("[%d] suspended '%s'\n", j, command);
    } else if (status == RUNNING && (which == RUNNING || which == ALL)) {
      safe_printf("[%d] running '%s'\n", j, command);
    }
    free(command);
    /*Obserwacja przez Wielkiego Brata (shell-a) pracowników czy są w miejscu
     * gdzie powinni być*/
#endif /* !STUDENT */
  }
}

/* Monitor job execution. If it gets stopped move it to background.
 * When a job has finished or has been stopped move shell to foreground. */
int monitorjob(sigset_t *mask) {
  int exitcode = 0, state;

  /* TODO: Following code requires use of Tcsetpgrp of tty_fd. */
#ifdef STUDENT
  // (void)jobstate;
  // (void)exitcode;
  // (void)state;

  // printf("job[0].pgid: %d\n", jobs[0].pgid);
  // printf("job[0].pgid: %d\n", jobs[0].pgid);
  while (true) {
    state = jobstate(0, &exitcode);
    if (state == STOPPED) {
      // for(int i = 0; i < jobs[0].nproc; i++) safe_printf("%d
      // %d\n",jobs[0].proc[i].pid,jobs[0].proc[i].state );
      Tcgetattr(tty_fd, &(jobs[0].tmodes));
      movejob(0, allocjob());
      Tcsetpgrp(tty_fd, getpid());
      Tcsetattr(tty_fd, TCSADRAIN, &shell_tmodes);
      // fflush(STDIN_FILENO);
      break;
    }
    if (state == FINISHED) {
      // for(int i = 0; i < jobs[0].nproc; i++) safe_printf("%d
      // %d\n",jobs[0].proc[i].pid,jobs[0].proc[i].state );
      Tcsetpgrp(tty_fd, getpid());
      Tcsetattr(tty_fd, TCSADRAIN, &shell_tmodes);
      // fflush(STDIN_FILENO);
      break;
    }
    /*IF-y służą do tego by Wielki Brat(shell) patrzył i w odpowiednim momencie
    przejął kontrole a następnie zsyła niepoprawny proces do pokoju 101 (stopped
    odpowiednia komórka w joblist w background) albo nie ma co wysyłać*/

    // safe_printf("%d %d",jobs[0].pgid,jobs[0].state);

    Sigsuspend(mask);
  }

#endif /* !STUDENT */

  return exitcode;
}

/* Called just at the beginning of shell's life. */
void initjobs(void) {
  struct sigaction act = {
    .sa_flags = SA_RESTART,
    .sa_handler = sigchld_handler,
  };

  /* Block SIGINT for the duration of `sigchld_handler`
   * in case `sigint_handler` does something crazy like `longjmp`. */
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  Sigaction(SIGCHLD, &act, NULL);

  jobs = calloc(sizeof(job_t), 1);

  /* Assume we're running in interactive mode, so move us to foreground.
   * Duplicate terminal fd, but do not leak it to subprocesses that execve. */
  assert(isatty(STDIN_FILENO));
  tty_fd = Dup(STDIN_FILENO);
  fcntl(tty_fd, F_SETFD, FD_CLOEXEC);

  /* Take control of the terminal. */
  Tcsetpgrp(tty_fd, getpgrp());

  /* Save default terminal attributes for the shell. */
  Tcgetattr(tty_fd, &shell_tmodes);
}

/* Called just before the shell finishes. */
void shutdownjobs(void) {
  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Kill remaining jobs and wait for them to finish. */
#ifdef STUDENT
  // Sigprocmask(SIG_UNBLOCK, &sigchld_mask, &mask);
  for (int i = 0; i < njobmax; i++) {
    if (jobs[i].state) {
      killjob(i);
      Sigsuspend(&mask);
    } // morderstwo na zlecenie i czekanie na koronera
  }
#endif /* !STUDENT */

  watchjobs(FINISHED);

  Sigprocmask(SIG_SETMASK, &mask, NULL);

  Close(tty_fd);
}

/* Sets foreground process group to `pgid`. */
void setfgpgrp(pid_t pgid) {
  Tcsetpgrp(tty_fd, pgid);
}
