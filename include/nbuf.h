#ifndef nbuf_h_
#define nbuf_h_

/* shortlist of advantages:
	- don't need a memory allocator
	- symmetric: can send data both ways
	- any block size
	- multiple blocks per reservation (allows efficient "max possible blocks" scenario)
	- any combination of single/multiple producer/consumer
	- fast (split cache lines)
*/

#include <stddef.h>
#include <stdint.h>
#include <nonlibc.h>
#include <pthread.h>

#include "config.h"

/* in theoretical order of performance */
#ifndef NBUF_TECHNIQUE
#define NBUF_TECHNIQUE NBUF_DO_CAS
//#define NBUF_TECHNIQUE NBUF_DO_XCH
//#define NBUF_TECHNIQUE NBUF_DO_MTX
/* TODO: Spinlock? */
#endif


/*	nbuf_const
Data which should not change after initializiation; goes on it's own
	cache line so it's never invalid.
*/
struct nbuf_const {
	void		*buf;
	size_t		overflow;	/* Used for quick masking of `pos` variables.
					It's also `buf_sz -1`, and is used
						in lieu of a dedicated `buf_sz`
						variable to keep struct size <=64B.
				       */
	size_t		blk_sz;		/* Block size is a power of 2 */
	uint8_t		blk_shift;	/* multiply by blk_sz using a shift */
};


/*	nbuf_sym
a (symmetrical) half of a circular buffer
*/
struct nbuf_sym {
	size_t		pos;	/* head/tail of buffer */
	size_t		avail;	/* can be reserved */

#if (NBUF_TECHNIQUE == NBUF_DO_MTX)
	pthread_mutex_t lock;
#else
	/* non-locking multi-read or multi-write contention only */
	size_t		reserved;
	size_t		uncommitted;
#endif
};


/* TODO: determine this at compile time for a given architecture */
#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

/*	nbuf
circular buffer with proper alignment
*/
struct nbuf {
	/* cache line 1: all the unchanging stuff that should NEVER be invalidated */
	struct nbuf_const	ct;
	unsigned char		pad_ln1[CACHE_LINE - sizeof(struct nbuf_const)];
	/* cache line 2: tx side */
	struct nbuf_sym		tx;
	unsigned char		pad_ln2[CACHE_LINE - sizeof(struct nbuf_sym)];
	/* cache line 3: rx side */
	struct nbuf_sym		rx;
	unsigned char		pad_ln3[CACHE_LINE - sizeof(struct nbuf_sym)];
};


/*	nbuf_size()
Returns the size of the underlying buffer.
*/
NLC_INLINE size_t nbuf_size(const struct nbuf *nb)
{
	return nb->ct.overflow + 1;
}

/*	nbuf_blk_sz()
Returns the size of one buffer block.
*/
NLC_INLINE size_t nbuf_blk_sz(const struct nbuf *nb)
{
	return nb->ct.blk_sz;
}

/*	nbuf_blk_div()
Divide by the size of a block.
*/
NLC_INLINE size_t nbuf_blk_div(const struct nbuf *nb, size_t bytes)
{
	return bytes >> nb->ct.blk_shift;
}
/*	nbuf_blk_mult()
Multiply by the size of a block.
*/
NLC_INLINE size_t nbuf_blk_mult(const struct nbuf *nb, size_t bytes)
{
	return bytes << nb->ct.blk_shift;
}

/*	nbuf_access()
Access a block inside of a reservation;
	returns a pointer to to the beginning of the block.
ALWAYS use this function to access blocks - a particular reservation may
	actually be split; with a portion of it looping around the end of the buffer!

It's conceptually AND computationally cheaper to allow reservations with variable
	numbers of blocks, and then just accessing each block through this function.
The alternative would be to either:
	- only allow single-block reservation calls
	- perform computational gymnastics to figure out if a requested
		reservation would loop past the end of the block and return
		a SHORTER reservation, including communicating that the reservation
		size has changed.
Both are inefficient.
*/
NLC_INLINE void *nbuf_access(size_t pos, size_t i, const struct nbuf *nb)
{
	pos += i << nb->ct.blk_shift;
	return nb->ct.buf + (pos & nb->ct.overflow);
}

/*	NBUF_DEREF()
Helper macro to combine an nbuf_access() with a typecast and a dereference;
	in a neat, presentable fashion.
It's main purpose is to avoid giving library users cancer when accessing
	buffers which contain scalar types such as integers or pointers.
*/
#define NBUF_DEREF(type, pos, i, nb) (*((type*)nbuf_access(pos, i, nb)))


int	nbuf_params(		size_t		blk_sz,
				size_t		blk_cnt,
				struct nbuf	*out);

int	nbuf_init(		struct nbuf	*nb,
				void		*mem);

void	nbuf_deinit(		struct nbuf	*nb);


size_t __attribute__((const))
	nbuf_reservation_size(const struct nbuf		*nb,
				size_t			blk_cnt);

/* TODO: function to reserve as many blocks as possible */

size_t	nbuf_reserve_single(const struct nbuf_const	*ct,
				struct nbuf_sym		*from,
				size_t			size);
size_t nbuf_reserve_single_var(const struct nbuf_const	*ct,
				struct nbuf_sym		*from,
				size_t			*out_size);

int	nbuf_release_single(const struct nbuf_const	*ct,
				struct nbuf_sym		*to,
				size_t			size);

#endif /* nbuf_h_ */