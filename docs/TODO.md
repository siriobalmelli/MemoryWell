---
title: TODO
order: 0
---

# TODO

- implement futexes
- EVENTING as a failure method
- speed differential if combining cache lines (actual impact of false sharing)?
- generic nmath functions so 32-bit size_t case is cared for
- no safety checking or locking on init/deinit - unsure of the best approach here;
	maybe a strenuous warning to the caller not to shoot themselves in the foot?
- Python bindings
- C++ extensions?
- non-contention cost of operations
	(reserving and releasing buffer blocks one by one)
- contention-ONLY cost (no operation on underlying memory)
- example of stack allocation
- example of underlying file access
- example of returning data to producers
- example of using zero-copy I/O (split nmem from nonlibc?)
- man pages
- return both 'res' and 'pos' by value in registers??

## Benchmark against other implementations

- <https://github.com/Nyufu/LockFreeRingBuffer/blob/master/unittests/EnqueueDequeueOrder4Thread.cpp>
- <https://github.com/shramov/ring>
- <https://github.com/ixtli/ringbuffer>

## BUGS

- OS X valgrind anomaly
