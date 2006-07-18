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
#ifndef SWAPSPACE_SUPPORT_H
#define SWAPSPACE_SUPPORT_H

#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"

#ifndef HAVE_SWAPON
int swapon(const char path[], int flags);
#endif
#ifndef HAVE_SWAPOFF
int swapoff(const char path[]);
#endif

/// Run given shell command with given argument.  Clobbers localbuf.
/** A convenient front-end for system(), this composes a command line consisting
 * of a command followed by a single argument (which will be quoted).
 * @return -1 on failure to execute (check errno), or the command's return
 * value.
 */
int runcommand(const char cmd[], const char arg[]);


#define INTERNAL_STRINGIFY(VALUE) #VALUE
#define STRINGIFY(VALUE) INTERNAL_STRINGIFY(VALUE)
/// PATH_MAX value as a string constant
#define PMS STRINGIFY(PATH_MAX)


#ifndef likely
/// As in Linux kernel: evaluate condition, which is likely to be true
#define likely(x) __builtin_expect(x,1)
#endif

#ifndef unlikely
/// As in Linux kernel: evaluate condition, which is likely to be false
#define unlikely(x) __builtin_expect(x,0)
#endif

#endif

