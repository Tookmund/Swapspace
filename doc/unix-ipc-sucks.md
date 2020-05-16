
Unix IPC sucks
==============

It's all very powerful and flexible, but the primitives are too many and
vastly overweight.  One-size-fits-all can be very useful, but in the case of
signals it also causes a lot of problems.  We can be interrupted at any
moment, so we really can't assume anything about the current state of the
program--yet the compiler has to optimize with one hand tied behind its back
because it must respect sequence points no matter when the signal comes in.
Our programs could be running much faster than they are, if only the compiler
weren't obliged to assume that signal handlers may interact with the program
and keep even non-volatile variables synchronized for this purpose.

This helped kill the Alpha architecture, by the way.  The Alpha AXP was a
beautiful, clean 64-bit RISC processor family designed by DEC (the Digital
Equipment Corporation, later acquired by Compaq, which in turn was later
acquired by HP) that was almost endian-neutral because it simply didn't have
single-byte or 16-bit memory accesses.  Everything went to and from memory in
larger chunks, so ECC checking could be moved off the motherboard and into
the processor itself; loops over text and such could be vectorized to read
and write entire 64-bit registers, i.e. 8 bytes at a time.  Both of these
ideas were expected to make a huge difference, since memory traffic is the
most stringent system performance bottleneck today.  But it didn't work out,
and the 8-bit and 16-bit load/store instructions were eventually added to the
architecture, increasing performance on the order of 20%.

One reason for the change was that device drivers sometimes needed 8-bit or
16-bit memory-mapped hardware accesses.  Windows NT introduced wrapper macros
for such accesses that enabled an MMU-based workaround on Alpha systems.  But
since these macros were no-ops on Intel x86-compatible processors, device
driver writers often failed to use them.  If Linux had hit the big time a bit
earlier, DEC might have survived.

The other reason why these instructions were added were signal handlers.  The
idea of vectorizing common loops was great; it was a precursor to today's
generation of vector instruction set extensions (first SPARC's VIS, later
Intel's MMX and PowerPC's Altivec/VMX).  It got such great performance that
compiler engineers at Intel wouldn't stop programming until they could do the
same trick.  Alpha was so fast at the time that it outran the top-of-the-line
Pentium Pro on the still-popular SPECint92 benchmarks... even while emulating
the x86 version of the code!  Intel finally thought they nailed the problem,
until Motorola engineers discovered a bug in the generated code: a vital loop
was vectorized so it did multiple iterations at a time, but it no longer
looped.  The affected benchmark was giving the correct answers through sheer
coincidence.  It turned out this mistake had inflated their compound
performance rating for the entire SPECint suite by an embarassing 15%.

Unfortunately, DEC's compiler engineers found that they could not use their
aggressive optimizations in real life.  The compiler never knows when a
signal is going to arrive, and yet it must synchronize the in-memory state of
most variables when it happens.  Which means practically all of the time!
This perhaps goes some way towards explaining that 20% performance benefit of
adding the extra load/store instructions: the optimizations that would have
made them unnecessary could be stunningly effective--but they could almost
never be used because they might break signal handling!

Yet despite all the conservative compiler care in the world it remains
terribly hard and complex to avoid serious bugs in signal handlers: you may
be logging to an output sink that isn't quite ready for use yet, or has just
been closed, or was just doing something else.  You may want to change a
variable that the main process was changing at the same time.  It's got all
the problems of multithreading, but without the aid of synchronization
primitives (okay, so you get race conditions in place of deadlocks).

Now that we're here, feel free to build your own lightweight mechanism on top
of this that does something actually useful!

All we really want is to set a flag saying "an exit was requested."  We don't
even need to know when, how many times, or by whom.  A modern kernel would
provide a primitive for just that, and build signal functionality on top of
it--not to mention select() and poll() and such.

Look at the Amiga OS for a shining example:
 - A "signal" is just a bit in the process descriptor that can be set
   atomically by another process, the process itself, or even an interrupt
   handler.  As with Unix select(), a process can atomically retrieve and
   clear any set of incoming signals, optionally sleeping first if nothing in
   the set has arrived yet.  Delivery is asynchronous.
 - All interrupt handling is done in the "bottom half;" signals are efficient
   enough to guarantee that the handler is woken up in time.  The decision to
   reschedule the signaled process is a simple matter of comparing the signal
   to a sleep mask.
 - Other forms of IPC build on top of the signal interface, using it to flag
   message arrival and such.  But many basic forms of event notification,
   such as a timer going off, or modification of a watched file, require only
   that simple single-bit signal.
 - Unix-style signal handlers can be registered as "exceptions."  These are
   all managed in terms of signal masks.  As it turned out this was not what
   anybody actually wanted; it was documented in the late 1980's but not
   properly implemented until 1992 because nobody ever tried to use it for
   anything anyway.  Or nobody apart from a Unix emulation library, perhaps.
   Everybody else just uses the single bit for communication and does the
   rest of the work in the event loop.
 - The graphical subsystem communicates with applications through these
   lightweight mechanisms, allowing a single process to provide effectively
   hard-realtime GUI responsiveness even while the application is busy.  You
   want to resize a window?  If the owner doesn't signal its willingness to
   repaint within 15 milliseconds, the system lets you know it's not going to
   happen.  You click a button while the application doesn't have time for
   you?  Either it's set up to send a signal, which will happen on cue, or it
   will "feel dead" from the start and it won't suddenly wake up and "become
   clicked" later.  You select a different radio button or move the mouse
   into a sensitive region?  The application isn't even woken up unless it
   registered an interest.  It's all stunningly efficient.

Instead, we need the system to schedule and/or interrupt our process, mask
the signal and jump through hoops to deal with various types of other
signals that may come in before we're finished handling this one, interrupt
any system call that we may be in (which must of course be able to deal with
this correctly), save execution context on the stack, invoke our handler
function (making the signal number available to us just in case we're
interested, which we're not), note completion of the handler, check for and
deal with any other incoming signals, unmask our signal and restore normal
running state, and return our process to where it was when it all happened. 
The program pays the cost of expecting that any system call may return
unexpectedly with an error code of "oops, a signal arrived."

And all this just so we can write one lousy Boolean to memory.  One single
stinking bit.
