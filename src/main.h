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
#ifndef SWAPSPACE_MAIN_H
#define SWAPSPACE_MAIN_H

#include <time.h>

/// I thought C99 had this built in... Maybe I'm doing something wrong.
typedef int bool;
enum { false=0, true=1 };

#define KILO (1024LL)
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)
#define TERA (KILO*GIGA)

/// Reusable local buffer space
/** Can be used for composing command lines or error messages, and other
 * function-local temporary storage.  This is not pretty, but the program must
 * get its work done under low-memory conditions.  Avoiding dynamic memory
 * allocation (and even wild fluctuations in stack use) may really matter here.
 *
 * Size is best kept to an even multiple of page size, and is guaranteed to be
 * no less than one page in size.
 */
extern char localbuf[16384];

/// Timestamp counter
extern time_t runclock;

/// Is the swapdir on a filesystem large enough for useful swap files?
/** Also prints a warning if the filesystem is large enough, but does not have
 * sufficient free space (but does not fail in that case).
 */
bool swapfs_large_enough(void);

#ifndef NO_CONFIG
/// Suppress informational output and warnings?
extern bool quiet;
/// Print debug output when changing state and such?
extern bool verbose;

char *set_quiet(long long dummy);
char *set_verbose(long long dummy);

bool main_check_config(void);
#endif

char *set_daemon(long long dummy);
char *set_pidfile(long long dummy);
char *set_erase(long long dummy);

#endif
