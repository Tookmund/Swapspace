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
#ifndef SWAPSPACE_SWAPS_H
#define SWAPSPACE_SWAPS_H

#include "memory.h"

/// Dump statistics to stdout
void dump_stats(void);


/// Read /proc/swaps and set up swapfiles[] accordingly.  Clobbers localbuf.
/**
 * @return whether this succeeded
 */
bool read_proc_swaps(void);


/// Was /proc/swaps in the expected format?
/** This assumes that read_proc_swaps() has been called successfully at least
 * once.  If no swaps were active during any of those calls, /proc/swaps may
 * have been completely empty so we cannot verify that the header line conforms
 * to the expected format.
 */
bool proc_swaps_parsed(void);


/// Scan for pre-existing swapfiles in the current directory, and activate them
/** Read /proc/swaps once before doing this, to ensure that we don't attempt to
 * re-enable any swapfiles that are already active (which shouldn't normally be
 * possible, but abnormal situations do occur sometimes).
 *
 * @return true unless a fatal error occurred
 */
bool activate_old_swaps(void);


/// Create a new swapfile.  Clobbers localbuf.
/**
 * @param size number of bytes to allocate
 * @return success
 */
bool alloc_swapfile(memsize_t size);

/// Free swap space
/**
 * @param maxsize maximum amount of memory that may be freed
 */
void free_swapfile(memsize_t maxsize);


/// Attempt to get rid of all our swapfiles right now
bool retire_all(void);


/// Total space on swap directory's filesystem
memsize_t swapfs_size(void);

/// Amount of free space on swap directory's filesystem
memsize_t swapfs_free(void);


#ifndef NO_CONFIG
/// Try to guard against attacker getting unguarded access to the disk?
extern bool paranoid;

char *set_min_swapsize(long long size);
char *set_max_swapsize(long long size);
char *set_swappath(long long dummy);
char *set_paranoid(long long dummy);

/// Verify configuration for swaps module; cd into swappath
bool swaps_check_config(void);
#endif

bool to_swapdir(void);

#endif
