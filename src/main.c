/*
This file is part of Swapspace.

Copyright (C) 2005,2006, Software Industry Industry Promotion Agency (SIPA)
Copyright (C) 2010, Jeroen T. Vermeulen
Written by Jeroen T. Vermeulen.

Swapspace is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Swapspace is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with swapspace; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#include "env.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "main.h"
#include "memory.h"
#include "opts.h"
#include "state.h"
#include "support.h"
#include "swaps.h"

char localbuf[16384];
time_t runclock = 0;

/// Have we received a kill/HUP/power signal?
static volatile bool stop = false;

static char pidfile[PATH_MAX] = "/var/run/swapspace.pid";
static bool make_pidfile = false;
static bool have_pidfile = false;
static int pidfd = -1;

char *set_pidfile(long long dummy)
{
  make_pidfile = true;
  return pidfile;
}

#ifndef NO_CONFIG
static bool godaemon = false;

char *set_daemon(long long dummy)
{
  godaemon = true;
  return NULL;
}

bool quiet = false;
bool verbose = false;

char *set_quiet(long long dummy)
{
  quiet = true;
  return NULL;
}
char *set_verbose(long long dummy)
{
  verbose = true;
  return NULL;
}

bool main_check_config(void)
{
  CHECK_CONFIG_ERR(quiet & verbose);
  return true;
}
#else
#define godaemon true
#endif


static bool erase = false;
char *set_erase(long long dummy)
{
  erase = true;
  return NULL;
}


static void rmpidfile(void)
{
  if (have_pidfile)
  {
    unlink(pidfile);
    have_pidfile = false;
  }
}

/// Write our process identifier to pid file.  Clobbers localbuf.
static bool writepid(pid_t pid)
{
  snprintf(localbuf, sizeof(localbuf), "%d\n", pid);
  const size_t len = strlen(localbuf);
  if (unlikely(write(pidfd, localbuf, len) < len))
  {
    log_perr_str(LOG_ERR, "Could not write pidfile", pidfile, errno);
    return false;
  }
  return true;
}

/// Create pidfile, if requested
/** This is implemented separately from finishpidfile() to allow errors to be
 * reported to stderr before we fork off a daemon process.
 * 
 * @return Success (which is trivially achieved if no pidfile is requested)
 */
static bool startpidfile(void)
{
  assert(pidfd == -1);
  if (!make_pidfile) return true;
  pidfd = open(pidfile, O_WRONLY|O_CREAT|O_EXCL, O_WRONLY);
  if (unlikely(pidfd == -1))
  {
    if (errno == EEXIST)
      logm(LOG_ERR,
	  "Daemon already running, or leftover pidfile: '%s'",
	  pidfile);
    else
      log_perr_str(LOG_ERR, "Could not create pidfile", pidfile, errno);
    return false;
  }
  
  have_pidfile = true;

  // Write temporary process id to pidfile; have to do this again if we fork()
  return writepid(getpid());
}


/// Signal handler that requests clean exit
 /*
  * doc/unix-ipc-sucks.md
 */
static void sighand_exit(int sig)
{
  stop = true;
}


/// Write process id to pidfile if requested, and close it.  Clobbers localbuf.
/**
 * @return Success (which is trivially achieved if no pidfile is requested)
 */
static void finishpidfile(void)
{
  if (likely(make_pidfile))
  {
    atexit(rmpidfile);
    close(pidfd);
  }
}


static pid_t daemonize(void)
{
  const pid_t result = fork();
  if (unlikely(result < 0)) perror("Could not fork off daemon process");
  else if (result == 0) setsid();
  return result;
}


/// Was a dump of statistics requested?
static volatile bool print_status=false;

/// Was an immediate adjustment requested?
static volatile bool adjust_swap=false;


/// Signal handler that requests generation of a status report to stdout
static void sighand_status(int sig)
{
  print_status = true;
}

/// Signal handler that initiates immediate swapspace adjustment
static void sighand_diet(int sig)
{
  adjust_swap = true;
}


static void install_sighandler(int signum, void (*handler)(int))
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(signum, &sa, NULL);
}


/// Install signal handlers
static void install_sigs(void)
{
#ifdef SIGXFSZ
  // Some systems may send signal if our swapfiles get too large, but we have
  // our own ways of dealing with that eventuality.  Ignore the signal.
  signal(SIGXFSZ, SIG_IGN);
#endif

  // Install exit handler.  These may not be repeatable, i.e. if the same signal
  // comes in again, we will probably just be terminated.  But that's not
  // unreasonable, come to think of it.
  signal(SIGTERM, sighand_exit);
  signal(SIGHUP, sighand_exit);
#ifdef SIGPWR
  signal(SIGPWR, sighand_exit);
#endif

  // These handlers must be repeatable.
  install_sighandler(SIGUSR1, sighand_status);
  install_sighandler(SIGUSR2, sighand_diet);
}


bool swapfs_large_enough(void)
{
  const memsize_t minswapfile = minimal_swapfile();

  if (minswapfile == MEMSIZE_ERROR) return false;

  if (swapfs_size() < minswapfile)
  {
    logm(LOG_CRIT, 
	"The filesystem holding swapspace's swap directory isn't big enough "
	"to hold useful swapfiles.");
    logm(LOG_CRIT,
	"Please try to expand this partition or relocate it to a larger one, "
	"if possible; or if all else fails, choose a different swap directory "
	"in your swapspace configuration.");
    return false;
  }

  if (swapfs_free() < minswapfile)
    logm(LOG_WARNING,
	"Not enough free space on swap directory.  As things stand now, "
	"swapspace will not be able to create swap files.");

  return true;
}


int main(int argc, char *argv[])
{
  assert(sizeof(localbuf) >= getpagesize());

  close(STDIN_FILENO);
  setlocale(LC_ALL, "C");

  if (!configure(argc, argv)) return EXIT_FAILURE;

  if (unlikely(!read_proc_swaps()) ||
      unlikely(!activate_old_swaps()) ||
      unlikely(!check_memory_status()))
    return EXIT_FAILURE;

  if (erase) return retire_all() ? EXIT_SUCCESS : EXIT_FAILURE;

  install_sigs();

  if (unlikely(!startpidfile())) return EXIT_FAILURE;

  /* Do a first iteration here so we can report any startup errors, and if we're
   * going to run as a daemon, we can do so in a steady state.
   */
  handle_requirements();

  /* If we're going to fork(), this is the last chance to read /proc/swaps in a
   * nonempty state before we do so.  Make one last attempt to check its format.
   */
  if (!proc_swaps_parsed())
  {
    if (!read_proc_swaps()) return EXIT_FAILURE;
#ifndef NO_CONFIG
    if (!quiet && !proc_swaps_parsed())
      fputs("[/proc/swaps is empty, so cannot check its format]\n", stderr);
#endif
  }

  if (godaemon)
  {
#ifndef NO_CONFIG
    if (verbose) logm(LOG_DEBUG, "daemonizing...");
#endif
    const pid_t pid = daemonize();
    if (unlikely(pid < 0))
    {
      rmpidfile();
      return EXIT_FAILURE;
    }
    if (pid > 0)
    {
      /* New process, so new pid.  Parent process rewrites pidfile.  We do this
       * from the parent, not the child process so that we're sure that the
       * pidfile is in a stable state when the parent exits.
       */
      lseek(pidfd, 0, SEEK_SET);
#ifndef NO_CONFIG
      if (verbose) logm(LOG_DEBUG, "got process id %d", pid);
#endif
      return writepid(pid) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  }

  finishpidfile();

  if (godaemon)
  {
    close(STDERR_FILENO);
    close(STDOUT_FILENO);

    // From here on std output is pointless in daemon mode.  Use syslog instead.
    log_start(argv[0]);
  }

  // Central loop
  for (++runclock; !stop; ++runclock)
  {
    if (unlikely(print_status))		print_status = false,	dump_stats();
    else if (unlikely(adjust_swap))	adjust_swap = false,	request_diet();
    else 			        handle_requirements();

    sleep(1);
  }

  int result = EXIT_SUCCESS;

#ifndef NO_CONFIG
  /* If we're worried about attackers getting unguarded access to the disk, we
   * need to retire and erase all swap files to keep them secret.
   */
  if (paranoid && !retire_all()) result = EXIT_FAILURE;
#endif

  rmpidfile();
  log_close();

  return result;
}
