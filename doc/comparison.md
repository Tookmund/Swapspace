
How it compares
===============

There are similar programs out there, such as dynswapd and the older swapd
(which is more portable).  They had some shortcomings that swapspace avoids.

First thing is robustness.  Swap managers go to work when the system is short on
memory and perversely, they usually start using more memory when they go to
work.  If there isn't enough available memory, at best they can't help and at
worst they crash.  That's why swapspace does not allocate any additional memory
once it's started up.  It doesn't grab a lot of memory on startup either; the
difference is mostly a matter of different program design.

Next, swapspace aims to be user-friendly.  No configuration is needed.  The
daemon tries to figure out what's needed at runtime.  It balances your memory
needs against disk usage.  It also tries to minimize time lost to writing new
swap files.  There are knobs you can twiddle, but they're mostly for
experimentation and development.

There's one twist to other swap managers: swapspace adapts the sizes of the
swap files it creates.  The Linux kernel may limit the total number of swap
files, but as you need more, the daemon will create larger and larger ones.
That way, creating your first swap file only takes a few seconds and won't slow
down your system much.  As you need more virtual memory, it will start to take
longer but also give you more space--so it happens less often.

The swapspace daemon has been in production use for several months on various
32-bit architectures before it was first released to the public, and has been
tested with swapfiles larger than can be addressed in 32 bits.

Swapspace is a small program: about 50 kilobytes on my system--or even less in a
special version that only accepts the most basic configuration options and
ignores its configuration file.  It also needs very little memory for itself.

