# cpp-memory-manager
Crossplatform memory manager providing temporary and long-term pool allocators and new/delete-like support for isolated RAM regions

## Requirements

The memory manager was originally intended to supplement the FreeRTOS memory manager for MCUs with medium to large amount of SRAM. Larger MCUs like STM32H7 series come with non-continuous SRAM blocks, of which only one is accessible to the FreeRTOS and the C++ runtime library.

* The general memory manager provides access to these isolated regions. A custom class with static methods for basic `new` and `delete` functionality.
* The long-term pool allocator provides efficient pools for STL containers to avoid the default allocator using `new` and `delete` here. These pools
  * use space-efficient storage for internal nodes of one or more containers with node size tailored for the contents.
  * provide allocate and deallocate operations with O(1) time and space complexity.
* The temporary pool allocator is a ringbuffer allocator for temporary containers with
  * O(1) time and space complexity for allocatewithout any space overhead
  * zero-cost deallocate

## Use cases

### General memory manager

The general memory manager is primarily intended to provide access to otherwise unreachable regions, because of the OS and/or the C++ runtime library does not fully support an MCU architecture. This is **not** a full-featured memory manager, because it does not

* support concurrency without application-level locking (see the interface below)
* support address obfuscation
* reach the time performance of the C++ runtime library. On average, allocating and freeing a small (1k) block on my Intel(R) Xeon(R) CPU E31220L @ 2.20GHz takes **1.7** us, while the C++ runtime library took **0.6** us.

However, it provides some tuning parameters and additional diagnostics, which may be essential for memory-constrained platforms. Moreover, it provides known allocate and deallocate time: **O(log n)** in term of total memory provided for the manager.

Generally, the general memory manager is intended for not too frequent allocations.

### Long-term pool allocator

This may be allocated on the heap or using the general memory manager on and independent memory region. Its only purpose is to be embedded in STL containers (see below) to achieve blazing-fast node allocation and deallocation. It uses fixed block size given in its constructor. It only requires updating 2 pointers on each operation.

### Temporary pool allocator

This is similar to the above one, but even faster: deallocation costs nothing. Moreover, it can serve blocks of arbitrary size without any fragmentation. It is implemented as a ringbuffer, so if it once gets full, the oldest contents are corrupted, hence only suitable as a workspace for short-term computations.

## Design

### General memory manager

This is called `FibonacciMemoryManager` and is an implementation of the buddy allocator using generalized Fibonacci numbers instead of powers of 2. This yields much less internal and external fragmentation and lets the user control the fragmentation level vs manager overhead ratio more precisely. See for example [here](https://www.geeksforgeeks.org/operating-system-allocating-kernel-memory-buddy-system-slab-system/).

Generalized Fibonacci numbers are formed by adding the last one and the one _D_ steps before last one. Starting sequence is 1 … 1+_D_. So For the ordinary Fibonacci numbers _D_ = 1 and it looks like

```C++
1, 2, 3, 5, 8, 13, 21, 45, 55...
---- these 2 are not splittable
```

When _D_ = 3, it looks like

```C++
1, 2, 3, 4, 5, 7, 10, 14, 19...
---------- these 4 are not splittable
```

Later on, I mean generalized Fibonacci numbers on the term Fibonacci. The memory manager gets these information:

Type           | Name             | Where         | Description
---------------|------------------|---------------|------------------------
`void*`        |_memory_          |constructor    |The start address of the block to use for internal accounting and as memory to serve. This must be aligned to `std::max_align_t`. The memory used by `FibonacciMemoryManager` internal fields can be placed here using placement new.
`bool`         |_exactAllocation_ |constructor    |If true, the system strives to avoid internal fragmentation. If false, the system tries to save bigger blocks for possibly bigger allocations later.
class          |_interface_       |template       |A user-defined interface to sign allocation errors.
`size_t`       |_memorySize_      |template       |Length of the available memory in bytes. 16384 <= _memorySize_
`size_t`       |_minimalBlockSize_|template       |Minimum length of an internal block will be a multiple of this and a possible Fibonacci number configured for the system. However, due to internal accounting, only an amount reduced by _alignment_ will be available for user data. Must be a multiple of _alignment_ and at least 2 * _alignment_. The system will choose the real value such that the memory to be served will be maximized.
`size_t`       |_alignment_       |template       |The alignment of user data to serve, at least 4 bytes.
`size_t`       |_D_               |template       |See above. 1 <= _D_ < 9

Static assertions will check the above conditions, and part of the internal configuration will be performed compile-time. The interface looks like:

```C++
class Interface {
public:
  static void badAlloc();
  static void lock();
  static void unlock();
};
```

Function        | Description
----------------|-----------------------
`badAlloc()`    |Application hook to sign allocation failure or throw exception.
`lock()`        |Can be used to start a mutual exclusion path to prevent other threads from concurrent modifications.
`unlock()`      |Can be used to finish the mutual exclusion path.

#### Internal quantities

Quantity | Description
---------|----------------------------
_P_      |_F[N-D-2]_ is the maximum amount of free nodes in the buddy tree. For _D_ = 1 and _N_ = 10 it happens when all the _F[0]_ leaves in the buddy tree are occupied and the _F[1]_ are free. Then, _P_ is 34.
_T_      |Internal blocksize of the pool, calculated.
_N_      |Number of Fibonacci numbers, the largest one corresponding to serving the whole memory in one block.
_R_      |Real blocksize corresponding to _F[0]_, calculated from the above parameters and the space requirement of the internal accounting below.
_F[i]_   |The ith Fibonacci number, being _F[0]_  = 1 and _F[1]_ = 2.

#### Internal data structures

```C++
enum class FibonacciDirection : uint8_t {
  cHere  = 0u << 5u, // assume the index is i
  cLeft  = 1u << 5u, // direction to smaller son, index is i-D-1
  cRight = 2u << 5u  // direction to larger son,  index is i-1  
};

class FibonacciCell {
private:
  uint8_t mValue;
public:
  FibonacciDirection(bool aExact, FibonacciDirection aDir);
  bool isExact() const;
  FibonacciDirection getDirection() const;
};
```

The `FibonacciMemoryManager` uses the following main data structures:

Type                  | Name            | Description
----------------------|-----------------|------------------------------
`std::set<void*> [N]` |_freeSets_       |Each set is representing the free leaves of size _F[i]_, 0 <= _i_ < _N_. The sets are implemented using a `PoolAllocator`. The sets store the start of the corresponding blocks.
`size_t [N]`          |_fibonaccis_     |Cache array holding the Fibonacci numbers.
`FibonacciCell [N, N]`|_allocDirections_|Table to help allocation algorithm.
unspecified           |_pool_           |Pool for storing the set nodes. It stores at most _P_ nodes alltogether in all the sets.
unspecified           |_data_           |Blocks to serve.

The system substracts the above data structures from _memorySize_ and uses the remaining space for block storage.

Moreover, each block contains the following information in its header:

* The index of the Fibonacci number representing the size of this block, so _i_ from _F[i]_.
* Two bits, _B_ and _M_ representing the path from a node to the root. For more info, see [here](https://www.researchgate.net/publication/220427431_A_Simplified_Recombination_Scheme_for_the_Fibonacci_Buddy_System).

The system returns the pointer to the application without the header.

#### Algorithms

Initially, the set of Fibonacci numbers and corresponding block sizes are calculated:

```C++
First, the pool block size is measured -> T
Calculation of N such that memorySize/F[N-1] >= minimalBlockSize
Calculation of the total header including alignments and the above data structures -> H
while(header does not fit in memory or memorySize - F[N0-1]*minimalBlockSize >= H) {
  --N
  recalculate H
  blockSize = (memorySize - H)/F[N-1] rounded down to alignment
}
```

The data structures are initialized such that there is only free block of the largest possible size. A fundamental principe of a buddy allocator is to maintain the largest possible free blocks by coalescing the just deallocated ones, if possible.

This implementation is **not thread safe**.

The `FibonacciMemoryManager` class may be instantiated on the beginning of the memory region it is given in the constructor. It is at least prepared to leave the start of the memory intact to let this happen.

##### Allocation

The allocation algorithm supports two strategies:

* Exact allocation requires each requested block to be fulfilled with the smallest possible block available, or obtained by dividing a larger one. This minimizes internal fragmentation on the expense of sacrificing larger block possibly important for bigger requests.
* Cautious allocation saves the larger blocks for the future, but may return bigger blocks than desired. However, this effect can happen only for small blocks.

I use a helper table called _allocDirections_ to decide how the blocks should be divided or choosed. This is an _N * N_ matrix, with the first index is the Fibonacci index of the block to be divided, the second one is the required block size. This is calculated during initialization:

```C++
First, the main diagonal is filled by the info <here, exact>.
Second, the allocDirections[i,j] are filled by the info <here, inexact> for 0<=j<i<=D.
if(exactAllocation) {
  for(i=D+1; D<N; i++) {
    for(every j < i) {
      if allocDirections[i-D-1, j] is exact and exists, mark this as exact and set dir to left
      else if allocDirections[i-1, j] is exact, mark this as exact and set dir to right
      else if allocDirections[i-D-1, j] exists, mark this as inexact and set dir to left
      else reference allocDirections[i-1, j], mark this as inexact and set dir to right
    }
  }
}
else {
  for(i=D+1; D<N; i++) {
    for(every j < i) {
      f allocDirections[i-D-1, j] is enough, reference it, mark this with its exactness and set dir to left
      else reference allocDirections[i-1, j], mark this with its exactness and set dir to right
    }
  }
}
```

Allocating a size of 0 or larger than the available space results in a call to `tInterface::badAlloc()` and returning a `nullptr`. This is **not the standard** C++ `new` behavior. The allocation is performed using this algorithm:

```C++
if(exactAllocation) {
  search for the smallest Fibonacci index i with exact match for the requested block size
}
else { // nothing to do
}
if none found, search for the smallest available and large enough i
if(found an i) {
  remove the smallest pointer from the freeSets[i]
  split the block if needed according to the chain in allocDirections[i, requested index]
  // splitting happens using the algorithm in the above 1975 paper
  return the final block
}
else {
  sign bad alloc error
}
```

##### Deallocation

Deallocation happens by putting back the returned block into the corresponding set and potentially coalescing it with its free buddies, if any:

```C++
Recover the block address from the application pointer (substract header size).
while (the actual block has a free buddy and is not on the N-1 level) {
  remove the buddy from the free list
  make the coalesced block the current one
}
Put the current block in the free list.
```

#### Error handling

The interface may throw any exception, if the application decides to use exceptions, or use any other way to handle errors, if the application is compiled without exception handling.

#### API

The `FibonacciMemoryManager` class is not intended for end-use, although it can provide a `malloc-free`-like C level API. The public API of the memory manager is the templated `NewDelete` class, which uses exactly the same template parameters as the `FibonacciMemoryManager` class. Its public methods are. Examples are shown in the usage section.

Function                                                                                                  | Description
----------------------------------------------------------------------------------------------------------|------------------------------------------------
`template<typename tClass, typename ...tParameters> static tClass* _new(tParameters... aParameters)`      |Like `void* operator new()` it creates an object and calls its constructor using the given parameters. It uses the template parameter as alignment.
`template<typename tClass> static tClass* _newArray(size_t const aCount)`                                 |Like `void* operator new(size_t)` it creates an object and calls its default constructor. It uses the template parameter as the alignment of the array start.
`template<typename tClass> static void _delete(tClass* aPointer)`                                         |Like `void delete void*` it calls the object destructor and deallocates the object.
`template<typename tClass> static void _deleteArray(tClass* aPointer)`                                    |Like `void delete[] void*` it calls the object destructor and deallocates the object.
`static size_t getFreeSpace() noexcept`                                                                   |Returns the total remaining space. Note, due to external fragmentation, it is probably not available in one block or in a size the application would desire.
`static size_t getMaxUserBlockSize()`                                                                     |Returns the size of the largest block when nothing has been allocated.
`static size_t getMaxFreeUserBlockSize() noexcept`                                                        |Returns the size of the largest available block.
`static size_t getAlignment() noexcept`                                                                   |Returns the alignment used for block allocation.
`static bool isCorrectEmpty() noexcept`                                                                   |Checks if the memory manager is empty and its internal accounting corresponds the empty state. Should be called when every content is considered to be freed.

### Long-term pool allocator

This is called `PoolAllocator` and operates using user-supplied memory. It uses a pool of fixed-size blocks linked in a single linked list. It supports `std::forward_list`, `std::list`, `std::map` and `std::set` - so containers with fixed-size allocations.

Generally, an allocator operates roughly like this:

1. It has at least one template parameter, initially the stored type of the container. This has rather a theoretical significance, at least for the above containers. As I saw, it is also instantiated.
1. A practical one with the template parameter for the container node (internal to the STL implementation) is derived using template magic and instantiated using the previous one. Therefore, it needs at least a copy constructor for an other type.
1. This is then copied or moved when the container gets copied or moved.
1. Upon container destruction the allocator will be required to free its contents.

The `PoolAllocator` must know the block size in advance. However, the STL containers can’t be queried for that info. The user will provide it either

* by knowing an upper limit for it
* or obtaining this size using the `AllocatorBlockGauge` class. This uses a special allocator designed to measure the required block size.

The constructor gets this information along with the maximal node count to store and the `Occupier` instance (see later).

The `PoolAllocator` is **not** a complete C++ allocator. It does not define copy and move constructors in a correct way, so the containers using it can't copy, move or swap themselves. Using these operations **will result in system hang**. I have made this deceison to make the implementation predictable in time consumption and avoid additional memory requirements.

Currently this allocator does not support any other alignment than what is implicitely provided by aligning to `void*` type. For embedded applications and most other cases this will suffice. This simplification is required for the small and efficient implementation important for embedded usage.

The `PoolAllocator` uses a custom class to manage its pool allocation and deallocation:

```C++
class Occupier {
public:
  // This function is assumed to return memory aligned for void*.
  // @returns nullptr on failure
  void* occupy(size_t const aSize) noexcept;
  void release(void* const aPointer) noexcept;
  
  // May throw exception or do something else.
  static void badAlloc();
};
```

The `PoolAllocator` manages its memory area in blocks. Beginning of each block is a pointer to the next one the rest is data the node. For sake of simplicity, _node count_ + 1 blocks will be used internally, so one free always remains. The `PoolAllocator` constructor calculates how much memory it needs to occupy via `Occupier` and then initializes the linked list. This will take O(_node count_) time.

The allocator returns `nullptr` if it runs out of space or the size of requested item is larger than the node size.
This implementation is not thread-safe.
Error handling

The Occupier may throw any exception, if the application decides to use exceptions, or use any other way to handle errors, if the application is compiled without exception handling.

### Temporary pool allocator

TBD

## Usage

### General memory manager

#### Allocating objects requiring no internal allocators (no STL containers)

```C++
class Test final {
  int    mI = 1u;
  double mD = 2.2;

public:
  Test() = default;
  Test(int aI, double aD) : mI(aI), mD(aD) {}
  ~Test() = default;
  void print();
};

class Interface final {
public:
  static void badAlloc() {
    throw std::bad_alloc();
  }
  static void lock() {
    // place to start mutual exclusion if used in multithreaded environemnt
  }
  static void unlock() {
    // place to finish mutual exclusion if used in multithreaded environemnt
  }
};

constexpr size_t cMemorySize          =  1024u * 512u;
constexpr size_t cMinBlockSize        =   128u;
constexpr size_t cUserAlign           =     8u;
constexpr size_t cFibonacciDifference =     3u;
constexpr uintptr_t cAddress          = 0x12345678u;
typedef NewDelete<Interface, cMemorySize, cMinBlockSize, cUserAlign, cFibonacciDifference> ExampleNewDelete;

int main() {
  ExampleNewDelete::init(reinterpret_cast<void*>(cAddress), false); // will return addresses in [cAddress, cAddress+cMemorySize]
  
  int* int1 = ExampleNewDelete::_new<int>();
  Test* test1 = ExampleNewDelete::_new<Test>(2, 3.3);
  Test* test2 = ExampleNewDelete::_newArray<Test>(2u);

  // use the pointers
  
  ExampleNewDelete::_delete<int>(int1);
  ExampleNewDelete::_delete<Test>(test1);
  ExampleNewDelete::_deleteArray<Test>(test2);
}
```

#### Allocating and using STL containers

This is a complex example, please also refer the below chapter on `PoolAllocator`. This example uses the custom memory region starting at `cAddress` for everything.

```C++
class Interface final {
public:
  static void badAlloc() {
    throw std::bad_alloc();
  }
  static void lock() {
    // place to start mutual exclusion if used in multithreaded environemnt
  }
  static void unlock() {
    // place to finish mutual exclusion if used in multithreaded environemnt
  }
};

constexpr size_t cMemorySize          =  1024u * 512u;
constexpr size_t cMinBlockSize        =   128u;
constexpr size_t cUserAlign           =     8u;
constexpr size_t cFibonacciDifference =     3u;
constexpr uintptr_t cAddress          = 0x12345678u;
constexpr size_t cPoolSize            =   111u;

typedef NewDelete<Interface, cMemorySize, cMinBlockSize, cUserAlign, cFibonacciDifference> ExampleNewDelete;

// Used by the PoolAllocator to allocate nodes in the custom memory region.
class NewDeleteOccupier final {
public:
  NewDeleteOccupier() noexcept = default;

  void* occupy(size_t const aSize) {
    return reinterpret_cast<void*>(ExampleNewDelete::_newArray<uint8_t>(aSize));
  }

  void release(void* const aPointer) {
    ExampleNewDelete::_deleteArray<uint8_t>(reinterpret_cast<uint8_t*>(aPointer));
  }

  void badAlloc() {
    throw std::bad_alloc();
  }
} gOccupier;

int main() {
  ExampleNewDelete::init(reinterpret_cast<void*>(cAddress), false); // will return addresses in [cAddress, cAddress+cMemorySize]

  // Calculate node size of a forward list of uint32_t items.
  // The 0u is the value to insert during the measurement. Perhaps some template magic could
  // make it optional, but does not worth.
  size_t nodeSize = AllocatorBlockGauge<std::set<int>>::getNodeSize(0u);

  // Create an allocator by allocating the PoolAllocator object and its underlying pool in the custom memory region.
  PoolAllocator<int, NewDeleteOccupier>* allocator = ExampleNewDelete::_new<PoolAllocator<int, NewDeleteOccupier>>(cPoolSize, nodeSize, gOccupier);
  
  // Creates the set object in the custom memory region.
  std::set<int, std::less<int>, PoolAllocator<int, NewDeleteOccupier>>* set1 = ExampleNewDelete::_new<std::set<int, std::less<int>, PoolAllocator<int, NewDeleteOccupier>>>(*allocator);
  
  // These two calls allocate nodes from the pool.
  set1->insert(1);
  set1->insert(2);
  std::cout << "set1->size() " << set1->size() << '\n';
  
  // Destroys the set object. The pool still might be used by other containers.
  ExampleNewDelete::_delete<std::set<int, std::less<int>, PoolAllocator<int, NewDeleteOccupier>>>(set1);
  
  // Destroys the pool by freeing the pool itself and the PoolAllocator object.
  ExampleNewDelete::_delete<PoolAllocator<int, NewDeleteOccupier>>(allocator);
}
```

### Long-term pool allocator

#### std::forward_list

```C++
constexpr size_t cPoolSize = 5u;

// Designate a memory region to use. Here it is shared memory:
constexpr std::uintptr_t cMem1 = 0x7fdb6d830000;

// Define an Occupier for using it:
class FixedOccupier final {
private:
  void *mMemory;

public:
  FixedOccupier(void* aMemory) noexcept : mMemory(aMemory) {
  }

  void* occupy(size_t const aSize) noexcept {
    return mMemory;
  }

  void release(void* const aPointer) noexcept {
    // nothing to do
  }
  
  void badAlloc() {
    throw std::bad_alloc();
  }
} gOccupier(reinterpret_cast<void*>(cMem1));

// Calculate node size of a forward list of uint32_t items.
// The 0u is the value to insert during the measurement. Perhaps some template magic could
// make it optional, but does not worth.
// Or one may use the node size (greater than that of forward_list) of an std::set<uint32_t> instead, and give the same
// allocator for both the list and the set.
size_t nodeSize = AllocatorBlockGauge<std::forward_list<uint32_t>>::getNodeSize(0u);

// Create an allocator for the shared memory
PoolAllocator<uint32_t, FixedOccupier> alloc(cPoolSize, nodeSize, gOccupier);

// Create the container. This must not be copied or moved.
std::forward_list<uint32_t, PoolAllocator<uint32_t, FixedOccupier>> list1(alloc);
```

#### std::map

```C++
// Only the different parts:
size_t nodeSize = AllocatorBlockGauge<std::map<uint32_t, uint32_t>>::getNodeSize(std::pair<uint32_t, uint32_t>{0u, 0u});
PoolAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier> alloc2(cPoolSize, nodeSize, gOccupier);
std::map<uint32_t, uint32_t, std::less<uint32_t>, PoolAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier>> tree1(alloc);

### Temporary pool allocator

TBD
```
