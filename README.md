---
title: MemoryWell
order: 100
---

# MemoryWell [![Build Status](https://travis-ci.org/siriobalmelli/memorywell.svg?branch=master)](https://travis-ci.org/siriobalmelli/memorywell)

Speed up synchronization and motion of data between computing threads:
	a well-built lock-free circular buffer.

See these docs:

-	[on Github](https://github.com/siriobalmelli/memorywell)
-	[as a web page](https://siriobalmelli.github.io/memorywell/)

## How So? Pray tell

By using a [circular buffer](docs/overview.md#circularbuffer) for both
	"allocation" of memory and synchronizing access to it between threads.

[The overview](docs/overview.md) explains the rationale behind this.

## Code Example

Here is a simplified look at the steps involved:

```c
/*
	Thread writing data into the circular buffer
*/
void *writer_thread(void *args)
{
	struct well *buffer = args;

	/* Reserve up any number of available blocks, up to 42 */
	size_t pos;
	size_t block_count = well_reserve(&buffer->tx, &pos, 42);

	/* Write into blocks.
	NOTE we access blocks one at a time: our set of reserved blocks could
		start at the end of the buffer and loop through the beginning.
	*/
	for (size_t j=0; j < block_count; j++) {
		void *block = well_access(pos, j, buffer);
		memset(block, 0x1, well_blk_size(buffer));
	}

	/* Release reservation into the other side of the buffer.
	We reserved from 'tx' and so release into 'rx'.
	*/
	well_release_single(&buffer->rx, res);
}
```

The "reader" thread would be identical, except it accesses the opposite "side"
	of buffer: `rx` instead of `tx`:

```c
/*
	Thread reading data from circular buffer
*/
void *reader_thread(void *args)
{
	struct well *buffer = args;

	/* Reserve up any number of available blocks, up to 42 */
	size_t pos;
	size_t block_count = well_reserve(&buffer->rx, &pos, 42);

	/* Read from blocks */
	for (size_t j=0; j < block_count; j++)
		consume_data(well_access(pos, j, buffer));

	/* Release data back to 'tx' side (aka: unused, ready to be written).
	NOTE the buffer is **symmetrical**: we could also have written new data
		back into it, which the 'tx' side would then read.
	*/
	well_release_single(&buffer->tx, res);
}
```

Any of the programs in the [test](test/) directory serves a documentation
	purpose in addition to validating correctness.

**TODO**: link to man pages.

## Building and Installing

**TODO**: preamble to nix vs. non-nix; clean up both

### Nix section

`nix-build`

Not all the dependencies (e.g. `nonlibc` at the time of this writing)
	may be in `nix-pkgs` - add a `-I` flag to `nix-build` pointing to (one or more)
	dirs containing the source repos for these packages:

```bash
memorywell$ nix-build
error: file ‘nonlibc’ was not found in the Nix search path
memorywell$ ls ~
nonlibc
memorywell$ nix-build -I ~
```

**NOTE** however that if `nonlibc` were in `nix-pkgs` then that version
	would override the one on your local disk.

### Non-NIX section

You'll need Python ≥ 3.5.

Build and test on your machine by running:

```bash
./bootstrap.py
```

This project uses the [Meson Build System](http://mesonbuild.com/).

For in-depth instructions on how to incorporate a Meson project in your
	build ecosystem, see [the nonlibc library](https://siriobalmelli.github.io/nonlibc/).

This project depends on nonlibc; Meson should handle that for you automatically.

NOTE that nonlibc is included as a [Git Subtree](https://help.github.com/articles/about-git-subtree-merges/);
	if you change anything please commit the nonlibc changes **separately** so
	they can be merged into the nonlibc repo upstream.

## Benchmarks

After running [boostrap.py](./bootstrap.py), run benchmarks with:

```bash
cd build-release
ninja benchmark
```

Benchmarking is done for a varying counts of TX -> RX threads,
	and all combinations of the below:

### Sync techniques

To test validity of the underlying algorithm and give comparative metrics,
	there is a compile-time choice between the following synch techniques:

1. WELL_DO_XCH	:	entirely implemented using C11 atomics
1. WELL_DO_MTX	:	pthread mutex
1. WELL_DO_SPL	:	naive spinlock using `test_set` and `clear` operations

### Fail methods

The test routine being used for benchmarking, [well_test.c](test/well_test.c),
	allows a compile-time choice of actions when a `reserve()` or `release()`
	call is unsuccessful (no blocks available):

1. SPIN		:	loop until the call is successful
1. COUNT	:	atomically increment a global `waits` counter
1. YIELD	:	call `sched_yield()`
1. SLEEP	:	call `nanosleep()` (currently inactive: takes forever)
1. BOUNDED	:	spinlock a few iterations and `sched_yield()` if still failing

## Support

Communication is always welcome, feel free to send a pull request
	or drop me a line at <sirio.bm@gmail.com>.

### Acknowledgements

A big Thank You to:

- [Tony](https://github.com/TonyTheLion) for code, benchmarks, discussions,
	and pairing on the `_release_multi()` bug.
- Brook Z of [Massive Alliance](https://www.massivealliance.com/) for
	very good advice.
- Robert Anderson of [RPA Consultants](https://github.com/rpaboulder) for code
	and time spent going over the memory model.

## What's with the name tho

All the cliché titles like *lock-free*, *atomic*, *supercritical* and *cherenkov*
	were predictably taken, and adding to the roughly 400 search results for
	*circular buffer* would have been a bit of a buzz-kill.

Hence **MemoryWell**, your friendly neighborhood circular buffer:

-	a well from whence to draw memory
-	a well-implemented circular buffer

## le official project [TODO list](docs/TODO.md)
