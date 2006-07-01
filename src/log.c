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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

static bool logging = false;

#ifndef LOG_PERROR
/// If available (and nonzero), causes log output to be echoed to stderr
#define LOG_PERROR 0
#endif


void log_start(const char myname[])
{
  if (!logging)
  {
    openlog(myname, LOG_PERROR, LOG_DAEMON);
    logging = true;
  }
}


/// Stop logging, e.g. because storage containing log name goes out of scope
void log_close()
{
  closelog();
  logging = false;
}


static const char *prefix(int priority)
{
  switch (priority)
  {
  case LOG_ERR:		return "Error: ";
  case LOG_WARNING:	return "Warning: ";
  case LOG_NOTICE:	return "Notice: ";
  }
  return "";
}


static FILE *stream(int priority)
{
  switch (priority)
  {
  case LOG_ERR:
  case LOG_WARNING:
    return stderr;
  }
  return stdout;
}


static void vlog(int priority, const char fmt[], va_list ap)
{
  if (logging)
  {
    vsyslog(priority, fmt, ap);
  }
  else
  {
    FILE *const out = stream(priority);
    fprintf(out, "%s", prefix(priority));
    vfprintf(out, fmt, ap);
    fprintf(out, "\n");
  }
}


void logm(int priority, const char fmt[], ...)
{
  va_list ap;
  va_start(ap, fmt);
  vlog(priority, fmt, ap);
  va_end(ap);
}


void log_perr(int priority, const char msg[], int fault_errno)
{
  if (!fault_errno) logm(priority, "%s", msg);
  else logm(priority, "%s: %s", msg, strerror(fault_errno));
}


void log_perr_str(int priority, const char msg[], const char arg[], int err)
{
  if (!err) logm(priority, "%s '%s'", msg, arg);
  else logm(priority, "%s '%s': %s", msg, arg, strerror(err));
}


#ifndef NO_CONFIG
void log_discrep(int priority,
    const char msg[],
    int swapfile,
    memsize_t expected,
    memsize_t found)
{
  logm(priority,
      "Discrepancy in swapfile %d: %s (%lld bytes vs. %lld)",
      swapfile,
      msg,
      (long long)expected,
      (long long)found);
}
#endif

