/*
This file is part of Swapspace.

Copyright (C) 2005,2006, Software Industry Industry Promotion Agency (SIPA)
Written by Jeroen T. Vermeulen <jtv@xs4all.nl>.

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
  pidfd = open(pidfile, O_WRONLY|O_CREAT|O_EXCL);
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
/** Unix IPC sucks.
 *
 * It's all very powerful and flexible, but the primitives are too many and
 * vastly overweight.  One-size-fits-all can be very useful, but in the case of
 * signals it also causes a lot of problems.  We can be interrupted at any
 * moment, so we really can't assume anything about the current state of the
 * program--yet the compiler has to optimize with one hand tied behind its back
 * because it must respect sequence points no matter when the signal comes in.
 * Our programs could be running much faster than they are, if only the compiler
 * weren't obliged to assume that signal handlers may interact with the program
 * and keep even non-volatile variables synchronized for this purpose.
 *
 * This helped kill the Alpha architecture, by the way.  The Alpha AXP was a
 * beautiful, clean 64-bit RISC processor family designed by DEC (the Digital
 * Equipment Corporation, later acquired by Compaq, which in turn was later
 * acquired by HP) that was almost endian-neutral because it simply didn't have
 * single-byte or 16-bit memory accesses.  Everything went to and from memory in
 * larger chunks, so ECC checking could be moved off the motherboard and into
 * the processor itself; loops over text and such could be vectorized to read
 * and write entire 64-bit registers, i.e. 8 bytes at a time.  Both of these
 * ideas were expected to make a huge difference, since memory traffic is the
 * most stringent system performance bottleneck today.  But it didn't work out,
 * and the 8-bit and 16-bit load/store instructions were eventually added to the
 * architecture, increasing performance on the order of 20%.
 *
 * One reason for the change was that device drivers sometimes needed 8-bit or
 * 16-bit memory-mapped hardware accesses.  Windows NT introduced wrapper macros
 * for such accesses that enabled an MMU-based workaround on Alpha systems.  But
 * since these macros were no-ops on Intel x86-compatible processors, device
 * driver writers often failed to use them.  If Linux had hit the big time a bit
 * earlier, DEC
 *
 * The other reason why these instructions were added were signal handlers.  The
 * idea of vectorizing common loops was great; it was a precursor to today's
 * generation of vector instruction set extensions (first SPARC's VIS, later 
 * Intel's MMX and PowerPC's Altivec/VMX).  It got such great performance that
 * compiler engineers at Intel wouldn't stop programming until they could do the
 * same trick.  Alpha was so fast at the time that it outran the top-of-the-line
 * Pentium Pro on the still-popular SPECint92 benchmarks... even while emulating
 * the x86 version of the code!  Intel finally thought they nailed the problem,
 * until Motorola engineers discovered a bug in the generated code: a vital loop
 * was vectorized so it did multiple iterations at a time, but it no longer
 * looped.  The affected benchmark was giving the correct answers through sheer
 * coincidence.  It turned out this mistake had inflated their compound
 * performance rating for the entire SPECint suite by an embarassing 15%.
 *
 * Unfortunately, DEC's compiler engineers found that they could not use their
 * aggressive optimizations in real life.  The compiler never knows when a
 * signal is going to arrive, and yet it must synchronize the in-memory state of
 * most variables when it happens.  Which means practically all of the time!
 * This perhaps goes some way towards explaining that 20% performance benefit of
 * adding the extra load/store instructions: the optimizations that would have
 * made them unnecessary could be stunningly effective--but they could almost
 * never be used because they might break signal handling!
 *
 * Yet despite all the conservative compiler care in the world it remains
 * terribly hard and complex to avoid serious bugs in signal handlers: you may
 * be logging to an output sink that isn't quite ready for use yet, or has just
 * been closed, or was just doing something else.  You may want to change a
 * variable that the main process was changing at the same time.  It's got all
 * the problems of multithreading, but without the aid of synchronization
 * primitives (okay, so you get race conditions in place of deadlocks).
 *
 * Now that we're here, feel free to build your own lightweight mechanism on top
 * of this that does something actually useful!
 *
 * All we really want is to set a flag saying "an exit was requested."  We don't
 * even need to know when, how many times, or by whom.  A modern kernel would
 * provide a primitive for just that, and build signal functionality on top of
 * it--not to mention select() and poll() and such.
 *
 * Look at the Amiga OS for a shining example:
 *  - A "signal" is just a bit in the process descriptor that can be set
 *    atomically by another process, the process itself, or even an interrupt
 *    handler.  As with Unix select(), a process can atomically retrieve and
 *    clear any set of incoming signals, optionally sleeping first if nothing in
 *    the set has arrived yet.  Delivery is asynchronous.
 *  - All interrupt handling is done in the "bottom half;" signals are efficient
 *    enough to guarantee that the handler is woken up in time.  The decision to
 *    reschedule the signaled process is a simple matter of comparing the signal
 *    to a sleep mask.
 *  - Other forms of IPC build on top of the signal interface, using it to flag
 *    message arrival and such.  But many basic forms of event notification,
 *    such as a timer going off, or modification of a watched file, require only
 *    that simple single-bit signal.
 *  - Unix-style signal handlers can be registered as "exceptions."  These are
 *    all managed in terms of signal masks.  As it turned out this was not what
 *    anybody actually wanted; it was documented in the late 1980's but not
 *    properly implemented until 1992 because nobody ever tried to use it for
 *    anything anyway.  Or nobody apart from a Unix emulation library, perhaps.
 *    Everybody else just uses the single bit for communication and does the
 *    rest of the work in the event loop.
 *  - The graphical subsystem communicates with applications through these
 *    lightweight mechanisms, allowing a single process to provide effectively
 *    hard-realtime GUI responsiveness even while the application is busy.  You
 *    want to resize a window?  If the owner doesn't signal its willingness to
 *    repaint within 15 milliseconds, the system lets you know it's not going to
 *    happen.  You click a button while the application doesn't have time for
 *    you?  Either it's set up to send a signal, which will happen on cue, or it
 *    will "feel dead" from the start and it won't suddenly wake up and "become
 *    clicked" later.  You select a different radio button or move the mouse
 *    into a sensitive region?  The application isn't even woken up unless it
 *    registered an interest.  It's all stunningly efficient.
 *
 *  Instead, we need the system to schedule and/or interrupt our process, mask
 *  the signal and jump through hoops to deal with various types of other
 *  signals that may come in before we're finished handling this one, interrupt
 *  any system call that we may be in (which must of course be able to deal with
 *  this correctly), save execution context on the stack, invoke our handler
 *  function (making the signal number available to us just in case we're
 *  interested, which we're not), note completion of the handler, check for and
 *  deal with any other incoming signals, unmask our signal and restore normal
 *  running state, and return our process to where it was when it all happened. 
 *  The program pays the cost of expecting that any system call may return
 *  unexpectedly with an error code of "oops, a signal arrived."
 *
 *  And all this just so we can write one lousy Boolean to memory.  One single
 *  stinking bit.
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
    if (print_status)		print_status = false,	dump_stats();
    else if (adjust_swap)	adjust_swap = false,	request_diet();
    else 			handle_requirements();

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
