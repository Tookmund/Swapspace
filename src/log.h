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
#ifndef SWAPSPACE_LOG_H
#define SWAPSPACE_LOG_H

#include <syslog.h>

#include "main.h"
#include "memory.h"

/// Open log, with given program name (string storage must remain available!)
void log_start(const char myname[]);

/// Stop logging, e.g. because storage containing log name goes out of scope
void log_close();

/// Log formatted message to log, or stdout/sterr as appropriate
void logm(int priority, const char fmt[], ...);

/// Log message with given errno (or ignore error number if it is zero)
void log_perr(int priority, const char msg[], int fault_errno);

/// Log message, perror()-style, but appending a quoted string
void log_perr_str(int priority, const char msg[], const char arg[], int err);

#ifndef NO_CONFIG
/// Log logfile size discrepancy
void log_discrep(int priority,
    const char msg[],
    int swapfile,
    memsize_t expected,
    memsize_t found);
#endif

#endif

