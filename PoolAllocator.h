#ifndef NOWTECH_POOLALLOCATOR
#define NOWTECH_POOLALLOCATOR

#include <set>
#include <map>
#include <list>
#include <memory>
#include <forward_list>

namespace nowtech { namespace memory {

class MeasureAllocatorBase {
protected:
  MeasureAllocatorBase* mOriginal;
  size_t                mNodeSize  = 0u;
  size_t                mNodeCount = 0u;
  uint8_t*              mMemory;

  MeasureAllocatorBase(void* aMemory) noexcept 
  : mOriginal(this)
  , mMemory(static_cast<uint8_t*>(aMemory)) {
  }

  MeasureAllocatorBase(MeasureAllocatorBase const &aOther) noexcept 
  : mOriginal(aOther.mOriginal) {
  }

public:
  void* allocate(std::size_t const aCount, size_t const aLength) {
    mOriginal->mNodeSize = std::max<size_t>(mOriginal->mNodeSize, aLength);
    mOriginal->mNodeCount = std::max<size_t>(mOriginal->mNodeCount, aCount);
    auto result = mOriginal->mMemory;
    mOriginal->mMemory += aCount * aLength;
    return result;
  }

public:
  size_t getNodeSize() const noexcept {
    return mNodeSize * mNodeCount;
  }
};

template<typename tContainer>
class AllocatorBlockGauge;

template <typename tMeasureItem>
class MeasureAllocator : public MeasureAllocatorBase {
  template<typename tContainerItem> friend class AllocatorBlockGauge;

protected:
  MeasureAllocator(void* aMemory) noexcept : MeasureAllocatorBase(aMemory) {
  }

  size_t getNodeSize() const noexcept {
    return mOriginal->getNodeSize();
  }

public:
  using value_type = tMeasureItem;

  template <typename tOther>
  struct rebind {
    typedef MeasureAllocator<tOther> other;
  };

  template<typename tOther>
  MeasureAllocator(MeasureAllocator<tOther> const &aOther) noexcept : MeasureAllocatorBase(aOther) {
  }

  tMeasureItem* allocate(std::size_t const aCount) {
    return static_cast<tMeasureItem*>(mOriginal->allocate(aCount, sizeof(tMeasureItem)));
  }

  void deallocate(value_type*, std::size_t const) noexcept {
    // nothing to do
  }

  std::size_t max_size() const noexcept {
    return 1u;
  }
};

class AllocatorBlockGaugeBase {
protected:
  static constexpr size_t cMeasureBufferSize = 128u;
};

template<typename tContainerItem>
class AllocatorBlockGauge<std::forward_list<tContainerItem>> : public AllocatorBlockGaugeBase {
public:
  static size_t getNodeSize(void* aMemory, tContainerItem const &aValue) noexcept {
    MeasureAllocator<tContainerItem> gauge(aMemory);
    std::forward_list<tContainerItem, MeasureAllocator<tContainerItem>> container(gauge);
    container.push_front(aValue);
    return gauge.getNodeSize();
  }

  static size_t getNodeSize(tContainerItem const &aValue) noexcept {
    uint8_t buffer[cMeasureBufferSize];
    return getNodeSize(buffer, aValue);
  }
};

template<typename tContainerItem>
class AllocatorBlockGauge<std::list<tContainerItem>> : public AllocatorBlockGaugeBase {
public:
  static size_t getNodeSize(void* aMemory, tContainerItem const &aValue) noexcept {
    MeasureAllocator<tContainerItem> gauge(aMemory);
    std::list<tContainerItem, MeasureAllocator<tContainerItem>> container(gauge);
    container.push_front(aValue);
    return gauge.getNodeSize();
  }

  static size_t getNodeSize(tContainerItem const &aValue) noexcept {
    uint8_t buffer[cMeasureBufferSize];
    return getNodeSize(buffer, aValue);
  }
};

template<typename tKey, typename tValue>
class AllocatorBlockGauge<std::map<tKey, tValue>> : public AllocatorBlockGaugeBase {
public:
  static size_t getNodeSize(void* aMemory, std::pair<tKey, tValue> const &aValue) noexcept {
    MeasureAllocator<std::pair<tKey, tValue>> gauge(aMemory);
    std::map<tKey, tValue, std::less<tKey>, MeasureAllocator<std::pair<tKey, tValue>>> container(gauge);
    container.insert(aValue);
    return gauge.getNodeSize();
  }

  static size_t getNodeSize(std::pair<tKey, tValue> const &aValue) noexcept {
    uint8_t buffer[cMeasureBufferSize];
    return getNodeSize(buffer, aValue);
  }
};

template<typename tKey, typename tValue>
class AllocatorBlockGauge<std::multimap<tKey, tValue>> : public AllocatorBlockGaugeBase {
public:
  static size_t getNodeSize(void* aMemory, std::pair<tKey, tValue> const &aValue) noexcept {
    MeasureAllocator<std::pair<tKey, tValue>> gauge(aMemory);
    std::multimap<tKey, tValue, std::less<tKey>, MeasureAllocator<std::pair<tKey, tValue>>> container(gauge);
    container.insert(aValue);
    return gauge.getNodeSize();
  }

  static size_t getNodeSize(std::pair<tKey, tValue> const &aValue) noexcept {
    uint8_t buffer[cMeasureBufferSize];
    return getNodeSize(buffer, aValue);
  }
};

template<typename tContainerItem>
class AllocatorBlockGauge<std::multiset<tContainerItem>> : public AllocatorBlockGaugeBase {
public:
  static size_t getNodeSize(void* aMemory, tContainerItem const &aValue) noexcept {
    MeasureAllocator<tContainerItem> gauge(aMemory);
    std::multiset<tContainerItem, std::less<tContainerItem>, MeasureAllocator<tContainerItem>> container(gauge);
    container.insert(aValue);
    return gauge.getNodeSize();
  }

  static size_t getNodeSize(tContainerItem const &aValue) noexcept {
    uint8_t buffer[cMeasureBufferSize];
    return getNodeSize(buffer, aValue);
  }
};

template<typename tContainerItem>
class AllocatorBlockGauge<std::set<tContainerItem>> : public AllocatorBlockGaugeBase {
public:
  static size_t getNodeSize(void* aMemory, tContainerItem const &aValue) noexcept {
    MeasureAllocator<tContainerItem> gauge(aMemory);
    std::set<tContainerItem, std::less<tContainerItem>, MeasureAllocator<tContainerItem>> container(gauge);
    container.insert(aValue);
    return gauge.getNodeSize();
  }

  static size_t getNodeSize(tContainerItem const &aValue) noexcept {
    uint8_t buffer[cMeasureBufferSize];
    return getNodeSize(buffer, aValue);
  }
};

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
class PoolAllocatorBase {
protected:
  PoolAllocatorBase* mOriginal;
  bool               mIsOriginal;
  tInterface&        mOccupier;
  size_t             mPoolSize;
  size_t             mNodeSize;
  size_t             mBlockSizeInPointerSize;
  
  /// This contains the linked free blocks, initially all free.
  /// A block begins with the pointer to the pointer part of the next block,
  /// then comes the data intended for the set node.
  void*  mMemory;
  void** mFirst;
  void** mProhibited;

public:
  PoolAllocatorBase(size_t const aPoolSize, size_t aNodeSize, tInterface& aOccupier) noexcept
    : mOriginal(this)
    , mIsOriginal(true)
    , mOccupier(aOccupier)
    , mPoolSize(aPoolSize)
    , mNodeSize(aNodeSize)
    , mBlockSizeInPointerSize((mNodeSize + sizeof(void*) - 1u) / sizeof(void*))
    , mMemory(mOccupier.occupy(mBlockSizeInPointerSize * sizeof(void*) * (mPoolSize + 1u)))
    , mFirst(reinterpret_cast<void**>(mMemory))
    , mProhibited(mFirst + mPoolSize * mBlockSizeInPointerSize) {
    for(size_t i = 0u; i < mPoolSize; ++i) {
      mFirst[i * mBlockSizeInPointerSize] = static_cast<void*>(mFirst + (i + 1u) * mBlockSizeInPointerSize);
    }
    mFirst[mPoolSize * mBlockSizeInPointerSize] = nullptr;
  }

  PoolAllocatorBase(PoolAllocatorBase const &aOther) noexcept 
  : mOriginal(aOther.mOriginal)
  , mIsOriginal(false)
  , mOccupier(aOther.mOccupier) {
  }

  ~PoolAllocatorBase() noexcept {
    if(mIsOriginal) {
      mOccupier.release(mMemory);
    }
    else { // nothing to do
    }
  }

  bool hasFree() noexcept {
    return mOriginal->mFirst != mOriginal->mProhibited;
  }

  void* allocate(std::size_t const, size_t) {
    void* result;
    if(mOriginal->mFirst != mOriginal->mProhibited) {
      result = mOriginal->mFirst;
      mOriginal->mFirst = static_cast<void**>(*mOriginal->mFirst);
    }
    else {
      result = nullptr;
      mOccupier.badAlloc();
    }
    return result;
  }

  /// I suppose this receives only valid pointers and each one only once.
  void deallocate(void* aPointer, std::size_t) noexcept {
    void **incoming = reinterpret_cast<void**>(aPointer);
    *incoming = static_cast<void*>(mOriginal->mFirst);
    mOriginal->mFirst = incoming;
  }
};

/// This allocator is intentionally NOT complete. It does not define copy and move constructors in a correct way,
/// so the containers using it can't copy, move or swap themselves.
///
/// Currently this allocator does not support any other alignment than what is implicitely provided by aligning
/// to void* type. For embedded applications and most other cases this will suffice. This simplification
/// is required for the small and efficient implementation important for embedded usage.
/// 
/// This instance contains mPoolSize pieces of blocks, each mBlockSizeInPointerSize long.
/// Beginning of each block is a pointer to the next one, and the rest is the node data.
/// For sake of simplicity, mPoolSize + 1 blocks will be used internally, so one free always remains.
/// Thus the last one will never be allocated.
/// This implementation is not thread-safe.
/// The allocator returns nullptr if it runs out of space or the size of requested item is larger than the default node size.
template<typename tContainerItem, typename tInterface>
class PoolAllocator : public PoolAllocatorBase<tInterface> {
  template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
  friend bool operator==(PoolAllocator<tContainerItemA, tInterfaceA> const &aAllocA, PoolAllocator<tContainerItemB, tInterfaceB> const &aAllocB);

  template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
  friend bool operator!=(PoolAllocator<tContainerItemA, tInterfaceA> const &aAllocA, PoolAllocator<tContainerItemB, tInterfaceB> const &aAllocB);

public:
  using value_type         = tContainerItem;
  using difference_type    = typename std::pointer_traits<tContainerItem*>::difference_type;
  using size_type          = std::make_unsigned_t<difference_type>;

  template <typename tOther>
  struct rebind {
    typedef PoolAllocator<tOther, tInterface> other;
  };

  PoolAllocator(size_t const aPoolSize, size_t aNodeSize, tInterface &aOccupier) noexcept : PoolAllocatorBase<tInterface>(aPoolSize, aNodeSize, aOccupier) {
  }
  
  template<typename tOther>
  PoolAllocator(PoolAllocator<tOther, tInterface> const &aOther) noexcept : PoolAllocatorBase<tInterface>(aOther) {
  }

  tContainerItem* allocate(std::size_t const aCount) {
    return static_cast<tContainerItem*>(this->mOriginal->allocate(aCount, sizeof(tContainerItem)));
  }

  /// I suppose this receives only valid pointers and each one only once.
  void deallocate(tContainerItem* aPointer, std::size_t aLen) noexcept {
    this->mOriginal->deallocate(aPointer, aLen);
  }

  std::size_t max_size() const noexcept {
    return 1u;          // Supports only single node allocation.
  }

  PoolAllocator select_on_container_copy_construction() const {
    return *this;
  }

  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap            = std::false_type;
  using is_always_equal                        = std::false_type;
};

template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
bool operator==(PoolAllocator<tContainerItemA, tInterfaceA> const &aAllocA, PoolAllocator<tContainerItemB, tInterfaceB> const &aAllocB) {
	return aAllocA.mOriginal->mPoolSize               == aAllocB.mOriginal->mPoolSize
      && aAllocA.mOriginal->mMemory                 == aAllocB.mOriginal->mMemory
      && aAllocA.mOriginal->mBlockSizeInPointerSize == aAllocB.mOriginal->mBlockSizeInPointerSize;
}

template <typename tContainerItemA, typename tInterfaceA, typename tContainerItemB, typename tInterfaceB>
bool operator!=(PoolAllocator<tContainerItemA, tInterfaceA> const &aAllocA, PoolAllocator<tContainerItemB, tInterfaceB> const &aAllocB) {
	return !(aAllocA == aAllocB);
}

} }

#endif
