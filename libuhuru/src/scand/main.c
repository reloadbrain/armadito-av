#include "libuhuru-config.h"

#include <libuhuru/core.h>

#include "server.h"
#include "utils/getopt.h"
#ifdef linux
#include "os/linux/daemonize.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

struct uhuru_daemon_options {
  int no_daemon;
  int use_tcp;
  int port_number;
};

struct opt daemon_opt_defs[] = {
  { .long_form = "help", .short_form = 'h', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "no-daemon", .short_form = 'n', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "tcp", .short_form = 't', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "port", .short_form = 'p', .need_arg = 1, .is_set = 0, .value = NULL},
  { .long_form = NULL, .short_form = '\0', .need_arg = 0, .is_set = 0, .value = NULL},
};

static void usage(void)
{
  fprintf(stderr, "usage: uhuru-daemon [options]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Uhuru antivirus scanner daemon\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --help  -h               print help and quit\n");
  fprintf(stderr, "  --no-daemon -n           do not fork and go to background\n");
  fprintf(stderr, "  --tcp -t                 use TCP socket\n");
  fprintf(stderr, "  --port -p                TCP port number\n");
  fprintf(stderr, "\n");

  exit(1);
}

static void parse_options(int argc, const char **argv, struct uhuru_daemon_options *opts)
{
  int r = opt_parse(daemon_opt_defs, argc, argv);
  const char *s_port;

  if (r < 0|| r > argc)
    usage();

  opts->no_daemon = opt_is_set(daemon_opt_defs, "no-daemon");
  opts->use_tcp = opt_is_set(daemon_opt_defs, "tcp");
  s_port = opt_value(daemon_opt_defs, "port", "15444");
  opts->port_number = atoi(s_port);
}

int main(int argc, const char **argv)
{
  struct uhuru_daemon_options opts;

  g_thread_init(NULL);
  g_type_init();

  parse_options(argc, argv, &opts);

#ifdef linux
  if (!opts.no_daemon)
    daemonize();
#endif

  server_loop(server_new(0));

  return 0;
}
