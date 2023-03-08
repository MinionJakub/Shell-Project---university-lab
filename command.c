#include "shell.h"

typedef int (*func_t)(char **argv);

typedef struct {
  const char *name;
  func_t func;
} command_t;

static int do_quit(char **argv) {
  shutdownjobs();
  exit(EXIT_SUCCESS);
}

/*
 * Change current working directory.
 * 'cd' - change to $HOME
 * 'cd path' - change to provided path
 */
static int do_chdir(char **argv) {
  char *path = argv[0];
  if (path == NULL)
    path = getenv("HOME");
  int rc = chdir(path);
  if (rc < 0) {
    msg("cd: %s: %s\n", strerror(errno), path);
    return 1;
  }
  return 0;
}

/*
 * Displays all stopped or running jobs.
 */
static int do_jobs(char **argv) {
  watchjobs(ALL);
  return 0;
}

/*
 * Move running or stopped background job to foreground.
 * 'fg' choose highest numbered job
 * 'fg n' choose job number n
 */
static int do_fg(char **argv) {
  int j = argv[0] ? atoi(argv[0]) : -1;

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);
  if (!resumejob(j, FG, &mask))
    msg("fg: job not found: %s\n", argv[0]);
  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return 0;
}

/*
 * Make stopped background job running.
 * 'bg' choose highest numbered job
 * 'bg n' choose job number n
 */
static int do_bg(char **argv) {
  int j = argv[0] ? atoi(argv[0]) : -1;

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);
  if (!resumejob(j, BG, &mask))
    msg("bg: job not found: %s\n", argv[0]);
  Sigprocmask(SIG_SETMASK, &mask, NULL);
  return 0;
}

/*
 * Make stopped background job running.
 * 'bg' choose highest numbered job
 * 'bg n' choose job number n
 */
static int do_kill(char **argv) {
  if (!argv[0])
    return -1;
  if (*argv[0] != '%')
    return -1;

  int j = atoi(argv[0] + 1);

  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);
  if (!killjob(j))
    msg("kill: job not found: %s\n", argv[0]);
  Sigprocmask(SIG_SETMASK, &mask, NULL);

  return 0;
}

static command_t builtins[] = {
  {"quit", do_quit}, {"cd", do_chdir},  {"jobs", do_jobs}, {"fg", do_fg},
  {"bg", do_bg},     {"kill", do_kill}, {NULL, NULL},
};

int builtin_command(char **argv) {
  for (command_t *cmd = builtins; cmd->name; cmd++) {
    if (strcmp(argv[0], cmd->name))
      continue;
    return cmd->func(&argv[1]);
  }

  errno = ENOENT;
  return -1;
}

noreturn void external_command(char **argv) {
  const char *path = getenv("PATH");

  if (!index(argv[0], '/') && path) {
    /* TODO: For all paths in PATH construct an absolute path and execve it. */
#ifdef STUDENT

    unsigned long int len = 0;
    char *actual_path;
    char *function = (char *)calloc(2, sizeof(char));
    function[0] = '/';
    strapp(&function, argv[0]);
    unsigned long int max_len = strlen(path); // maksymalna długość ścieżki
    while (len < max_len - 1) {
      unsigned long int divide_path_len = strcspn(path + len, ":");
      actual_path = strndup(path + len, divide_path_len);
      strapp(&actual_path, function);
      // safe_printf("%s\n",actual_path);
      (void)execve(actual_path, argv, environ);
      len += divide_path_len + 1;
    }

    // unsigned long long int len = 0; // laczna dlugosc wczytanych juz path-ow
    // char *rest_path = strchr(path, ':');
    // const unsigned long long int length_of_command =
    //   strlen(argv[0]);      // dlugosc stringa co ma byc wywolane
    // char *argv_0 = argv[0]; // zapisanie tego co ma byc wykonane
    // unsigned long long int diffrence;
    // while (rest_path) {
    //   diffrence = rest_path - path -
    //               len; // roznica miedzy start szukania, a znalezionym
    //               znakiem :
    //   char *one_path = strndup(path + len, diffrence + length_of_command +
    //                                          1); // kopiowanie sciezki
    //   one_path[diffrence] = '/';
    //   for (int i = 0; i < length_of_command; i++)
    //     one_path[i + diffrence + 1] =
    //       argv_0[i];                   // nadpisywanie oraz korekta by
    //       uzyskac
    //                                    // path/process_do_wykonania
    //   argv[0] = one_path;              // wymagania execve
    //   execve(one_path, argv, environ); // wrocimy sie jedynie wtedy gdy nie
    //                                    // bedzie pliku w danej sciezce
    //   free(one_path);                  // dealokowanie uzytej pamieci
    //   len += diffrence + 1; // dodanie do dlugosci przebytych juz sciezek
    //   rest_path = strchr(path + len, ':');
    // }
    // diffrence = strlen(path) -
    //             len; // roznica miedzy start szukania, a znalezionym znakiem
    //             :
    // char *one_path = malloc(sizeof(char) * (diffrence + length_of_command +
    // 1)); for (unsigned long long int i = 0; i < diffrence; i++)
    //   one_path[i] = path[len + i];
    // one_path[diffrence] = '/';
    // for (int i = 0; i < length_of_command; i++)
    //   one_path[i + diffrence + 1] =
    //     argv_0[i];                   // nadpisywanie oraz korekta by uzyskac
    //                                  // path/process_do_wykonania
    // argv[0] = one_path;              // wymagania execve
    // execve(one_path, argv, environ); // wrocimy sie jedynie wtedy gdy nie
    // bedzie
    //                                  // pliku w danej sciezce
    // free(one_path);                  // dealokowanie uzytej pamieci
    // argv[0] = argv_0;

#endif /* !STUDENT */
  } else {
    (void)execve(argv[0], argv, environ);
  }

  msg("%s: %s\n", argv[0], strerror(errno));
  exit(EXIT_FAILURE);
}
