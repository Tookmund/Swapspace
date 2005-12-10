/*
This file is part of Swapspace.

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef SWAPSPACE_MEMORY_H
#define SWAPSPACE_MEMORY_H

#include "main.h"

/// Type used for swap sizes and such.  Must be signed.
/** All internal accounting of file and memory sizes is done entirely in bytes,
 * which is why we need a very wide type for these values.  This way we avoid
 * nasty double-conversion or missing-conversion bugs; incorrect results due to
 * unexpected relationships between page size, disk block size, and kilobytes;
 * and most hidden overflow cases.  Using an unnaturally wide variable type is a
 * small price to pay.
 */
typedef long long memsize_t;

/// Singular value for memsize_t, analogous to the null pointer
#define MEMSIZE_ERROR LLONG_MIN

/// Round (through truncation) a size (in bytes) to multiple of page size
#define TRUNC_TO_PAGE(n) (((memsize_t)(n)) & ~((memsize_t)PAGE_SIZE-1))

/// Round upwards to multiple of page size
#define EXT_TO_PAGE(n) (TRUNC_TO_PAGE((n)+PAGE_SIZE-1))

/// Check if we can access memory status etc.  Clobbers localbuf.
bool check_memory_status(void);

/// Recommend change in available swap space.  Clobbers localbuf.
/** This is where policy on the total available memory size is formulated.
 * @return recommended increase in swap size (negative for a recommended
 * decrease)
 */
memsize_t memory_target(void);

/// What is the minimum swapfile size we can expect to allocate?
/** This has nothing to do with the minimal permissible swapfile size; it
 * depends on system memory size and the threshold settings currently in effect.
 *
 * The value is computed by assuming we hit the lower_freelimit without falling
 * below it, and computing the size of the swapfile we'd need in that case.
 * We can use this to determine whether the swapdir is in a usefully-sized
 * partition, and to warn if there is not enough space free on that partition
 * for swapspace to do useful work.
 */
memsize_t minimal_swapfile(void);

/// Log memory statistics
void dump_memory(void);

#ifndef NO_CONFIG
char *set_lower_freelimit(long long pct);
char *set_upper_freelimit(long long pct);
char *set_freetarget(long long pct);
char *set_buffer_elasticity(long long pct);
char *set_cache_elasticity(long long pct);

bool memory_check_config(void);
#endif

#endif

