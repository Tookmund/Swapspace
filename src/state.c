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

#include <stdio.h>

#include "log.h"
#include "main.h"
#include "memory.h"
#include "state.h"
#include "support.h"
#include "swaps.h"

/* The allocation/deallocation algorithm is driven by a state machine.
 */

/* TODO: Adaptive cooldown_time?
 * Extend cooldown time if a swapfile is allocated shortly after deallocation in
 * "overfed" state (say, within another timeout period), or if one is
 * deallocated shortly (say, at most two timeout periods) after having been
 * allocated.
 * Reduce cooldown time if "hungry" and/or "overfed" states consistently timeout
 * too soon (say, X times in the first half of the timeout period but never in
 * the second)
 */

/// Minimum cooldown time before returning to "steady state" allocation policy
static time_t cooldown_time = 600;

/// Countdown timer for return to "steady state"
static time_t timer = 0;

static inline void timer_reset(void) { timer = cooldown_time; }
static inline void timer_tick(void) { --timer; }
static inline bool timer_timeout(void) { return timer <= 0; }

#ifndef NO_CONFIG
char *set_cooldown(long long duration)
{
  cooldown_time = (time_t)duration;
  timer_reset();
  return NULL;
}
#endif


/// State machine describing transitions in allocation policy
enum State
{
  st_diet,	// Ran into disk limit; don't allocate
  st_hungry,	// Want more, or at least don't consider deallocation
  st_steady,	// Entirely neutral
  st_overfed	// Waiting to see if it's okay to deallocate
};

static const char *Statenames[] =
{
  "diet",
  "hungry",
  "steady",
  "overfed"
};


static enum State the_state = st_hungry;
static bool need_diet = false;

void request_diet(void) { need_diet = true; }

static void state_to(enum State s)
{
#ifndef NO_CONFIG
  if (verbose) logm(LOG_DEBUG,"%s -> %s",Statenames[the_state],Statenames[s]);
#endif
  the_state = s;
  timer_reset();
}


void handle_requirements(void)
{
  if (unlikely(need_diet))
  {
    need_diet = false;
    state_to(st_diet);
    return;
  }

  const memsize_t reqbytes = memory_target();
  timer_tick();

  if (unlikely(reqbytes > 0) && likely(the_state != st_diet))
  {
    /* In any state except "diet," where allocation is inhibited, a shortage of
     * memory means we forget what state we're in and jump straight to "hungry"
     * mode, allocating a new swapfile along the way.  If the allocation fails,
     * we bail out into "diet" mode next time, on alloc_swapfile()'s request.
     */
    if (likely(alloc_swapfile(reqbytes))) state_to(st_hungry);
  }
  else if (unlikely(timer_timeout()))
  {
    /* All states except "steady" are designed to time out eventually; in every
     * case that leads back to "steady" so we can make it a general rule that
     * timeout leads to "steady."
     * The only other action that accompanies timeout is when timing out of the
     * "overfed" state, which is where we normally deallocate.
     */
    if (the_state == st_overfed) free_swapfile(-reqbytes);
    state_to(st_steady);
  }
  else switch (the_state)
  {
  case st_diet:
    /* If we overallocated and now find ourselves with more swap space than we
     * think we need, deallocate it right away.  Don't leave "diet" state just
     * yet in that case, however, or we may invite thrashing.
     */
    if (unlikely(reqbytes < 0)) free_swapfile(-reqbytes);
    break;
  case st_hungry:
    /* The "hungry" state can either time out, or allocate more swap space and
     * loop back to itself, resetting the timer (or if allocation fails, bail
     * out to "diet").  In other words, all actions for this state have been
     * handled by the general cases above.
     */
    break;
  case st_steady:
    /* If we have more swap space than we need, go to "overfed" state which may
     * eventually lead to deallocation.
     */
    if (unlikely(reqbytes < 0)) state_to(st_overfed);
    break;
  case st_overfed:
    /* There are two ways out of "overfed" state: either we find that we no
     * longer have more memory than we need, in which case we default back to
     * steady state; or we time out as per the general case described above,
     * having had excess swap space for an entire timer period and therefore
     * deallocating swap space.
     */
    if (unlikely(reqbytes >= 0)) state_to(st_steady);
    break;
  }
}


void dump_state(void)
{
  logm(LOG_INFO, "state: %s", Statenames[the_state]);
  if (timer > 0) logm(LOG_INFO, "timer: %ld", (long)timer);
}

