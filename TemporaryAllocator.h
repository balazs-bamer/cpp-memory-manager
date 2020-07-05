#ifndef NOWTECH_RINGBUFFERALLOCATOR
#define NOWTECH_RINGBUFFERALLOCATOR

#include <memory>

namespace nowtech { namespace memory {

/// Only used to provide an allocator with memory
/// class Occupier {
/// public:
///   // This function is assumed to return memory aligned for void*.
///   // @returns nullptr on failure
///   void* occupy(size_t const aSize) noexcept;
///   void release(void* const aPointer) noexcept;
///
///   // May throw exception or do something else.
///   static void badAlloc();
/// };

template<typename tInterface>
class TemporaryAllocatorBase {
protected:
  TemporaryAllocatorBase* mOriginal;
  bool                     mIsOriginal;
  tInterface&              mOccupier;
  size_t                   mMemorySize;
  uint8_t*                 mMemory;
  uint8_t*                 mGetPointer;

public:
  TemporaryAllocatorBase(size_t const aSize, tInterface& aOccupier) noexcept
    : mOriginal(this)
    , mIsOriginal(true)
    , mOccupier(aOccupier)
    , mMemorySize(aSize)
    , mMemory(static_cast<uint8_t*>(mOccupier.occupy(aSize)))
    , mGetPointer(mMemory) {
  }

  TemporaryAllocatorBase(TemporaryAllocatorBase const &aOther) noexcept 
  : mOriginal(aOther.mOriginal)
  , mIsOriginal(false)
  , mOccupier(aOther.mOccupier) {
  }

  ~TemporaryAllocatorBase() noexcept {
    if(mIsOriginal) {
      mOccupier.release(mMemory);
    }
    else { // nothing to do
    }
  }

  size_t getMaxSize() const noexcept {
    return mMemorySize >> 1u;
  }

  void* doAllocate(size_t const aSize) {
    std::uint8_t* pointer;
    if(aSize <= mMemorySize >> 1u) {
      pointer = mGetPointer;
      mGetPointer += aSize;
      if(mGetPointer >= (mMemory + mMemorySize)) {
        pointer = mMemory;
        mGetPointer = mMemory + aSize;
      }
      else { // nothing to do
      }
    }
    else {
      mOccupier.badAlloc();
      pointer = nullptr;
    }
    return static_cast<void*>(pointer);
  }
};

/// This allocator is intentionally NOT complete. It does not define copy and move constructors in a correct way,
/// so the containers using it can't copy, move or swap themselves.
///
/// Currently this allocator does not support any other alignment than what is implicitely provided by aligning
/// to void* type. For embedded applications and most other cases this will suffice. This simplification
/// is required for the small and efficient implementation important for embedded usage.
/// 
/// This implementation is not thread-safe.
/// The allocator returns nullptr if the size of requested item is larger than the half of the memory size.
template<typename tContainerItem, typename tInterface>
class TemporaryAllocator : public TemporaryAllocatorBase<tInterface> {
  template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
  friend bool operator==(TemporaryAllocator<tContainerItemA, tInterfaceA> const &aAllocA, TemporaryAllocator<tContainerItemB, tInterfaceB> const &aAllocB);

  template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
  friend bool operator!=(TemporaryAllocator<tContainerItemA, tInterfaceA> const &aAllocA, TemporaryAllocator<tContainerItemB, tInterfaceB> const &aAllocB);

public:
  using value_type         = tContainerItem;
  using difference_type    = typename std::pointer_traits<tContainerItem*>::difference_type;
  using size_type          = std::make_unsigned_t<difference_type>;

  template <typename tOther>
  struct rebind {
    typedef TemporaryAllocator<tOther, tInterface> other;
  };

  TemporaryAllocator(size_t const aSize, tInterface &aOccupier) noexcept : TemporaryAllocatorBase<tInterface>(aSize, aOccupier) {
  }
  
  template<typename tOther>
  TemporaryAllocator(TemporaryAllocator<tOther, tInterface> const &aOther) noexcept : TemporaryAllocatorBase<tInterface>(aOther) {
  }

  tContainerItem* allocate(std::size_t const aCount) {
    return static_cast<tContainerItem*>(this->mOriginal->doAllocate(aCount * sizeof(tContainerItem)));
  }

  void deallocate(tContainerItem* aPointer, std::size_t aLen) noexcept { // nothing to do
  }

  std::size_t max_size() const noexcept {
    return this->mOriginal->getMaxSize();
  }

  TemporaryAllocator select_on_container_copy_construction() const {
    return *this;
  }

  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap            = std::false_type;
  using is_always_equal                        = std::false_type;
};

template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
bool operator==(TemporaryAllocator<tContainerItemA, tInterfaceA> const &aAllocA, TemporaryAllocator<tContainerItemB, tInterfaceB> const &aAllocB) {
	return aAllocA.mOriginal->mTemporarySize               == aAllocB.mOriginal->mTemporarySize
      && aAllocA.mOriginal->mMemory                 == aAllocB.mOriginal->mMemory
      && aAllocA.mOriginal->mBlockSizeInPointerSize == aAllocB.mOriginal->mBlockSizeInPointerSize;
}

template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
bool operator!=(TemporaryAllocator<tContainerItemA, tInterfaceA> const &aAllocA, TemporaryAllocator<tContainerItemB, tInterfaceB> const &aAllocB) {
	return !(aAllocA == aAllocB);
}

} }

#endif
