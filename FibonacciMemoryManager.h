#ifndef NOWTECH_FIBONACCIMEMORYMANAGER
#define NOWTECH_FIBONACCIMEMORYMANAGER

#include "PoolAllocator.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <array>
#include <set>

namespace nowtech::memory {

constexpr size_t countSetBits(size_t aNumber) noexcept { 
  size_t count = 0; 
  while (aNumber > 0u) { 
    aNumber &= (aNumber - 1u); 
    count++; 
  } 
  return count; 
}

/// class Interface {
///   static void badAlloc();
///   static void lock();
///   static void unlock();
/// };
template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory = 0u>
class FibonacciMemoryManager final {
  static_assert(tMemorySize >= 16384);
  static_assert(tMemorySize % alignof(std::max_align_t) == 0u);
  static_assert(tMinimalBlockSize % tAlignment == 0u);
  static_assert(tMinimalBlockSize >= tAlignment * 2u);
  static_assert(tAlignment >= 4u);
  static_assert(countSetBits(tAlignment) == 1u);
  static_assert(tFibonacciIndexDifference > 0u);
  static_assert(tFibonacciIndexDifference < 9u);

private:
  class FixedOccupier final {
  private:
    void*  mMemory;

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
      tInterface::badAlloc();
    }
  };

  enum class FibonacciDirection : uint8_t {
    cInvalid = 0u,
    cLeft    = 1u << 5u, // direction to smaller son, index is i-D-1
    cRight   = 2u << 5u, // direction to larger son,  index is i-1  
    cHere    = 3u << 5u  // assume the index is i
  };

  class FibonacciCell final {
  private:
    static constexpr uint16_t cMaskDirection  = static_cast<uint16_t>(FibonacciDirection::cHere);
    static constexpr uint16_t cMaskExact      = 1u << 7u;

    uint8_t mValue;

  public:
    FibonacciCell() noexcept : mValue(static_cast<uint16_t>(FibonacciDirection::cInvalid)) {
    }

    void set(bool const aExact) noexcept {
      mValue = (aExact ? cMaskExact : 0u) | static_cast<uint16_t>(FibonacciDirection::cHere);
    }

    void set(bool const aExact, FibonacciDirection const aDir) noexcept {
      mValue = (aExact ? cMaskExact : 0u) | static_cast<uint16_t>(aDir);
    }

    bool isExact() const noexcept {
      return (mValue & cMaskExact) != 0u;
    }

    FibonacciDirection getDirection() const noexcept {
      return static_cast<FibonacciDirection>(mValue & cMaskDirection);
    }
  };
  static_assert(sizeof(FibonacciCell) == sizeof(uint8_t));
  static_assert(alignof(FibonacciCell) == alignof(uint8_t));

  class BlockHeader final {
  private:
    static constexpr uint32_t cMaskBuddy  = 1u << 31u;
    static constexpr uint32_t cMaskMemory = 1u << 30u;
    static constexpr uint32_t cMaskIndex  = (1u << 30u) - 1u;
    uint32_t mValue;

  public:
    BlockHeader() noexcept = default;
   
    bool getBuddy() const noexcept {
      return (mValue & cMaskBuddy) != 0u;
    } 
   
    bool getMemory() const noexcept {
      return (mValue & cMaskMemory) != 0u;
    } 
   
    size_t getIndex() const noexcept {
      return mValue & cMaskIndex;
    }

    void set(bool const aBuddy, bool const aMemory, size_t const aIndex) noexcept {
      mValue = (aBuddy ? cMaskBuddy : 0u) |
               (aMemory ? cMaskMemory : 0u) |
               static_cast<uint32_t>(aIndex & cMaskIndex);
    }
  };
  static_assert(sizeof(BlockHeader) == sizeof(uint32_t));
  static_assert(alignof(BlockHeader) == alignof(uint32_t));

  typedef std::set<uint8_t*, std::less<uint8_t*>, PoolAllocator<uint8_t*, FixedOccupier>> FreeSet;
  typedef PoolAllocator<uint8_t*, FixedOccupier>                                          FreeSetAllocator;

  bool              mExactAllocation;
  size_t            mSetNodeSize;
  bool              mReady       = false;
  size_t            mBlockSize;
  size_t            mFibonacciCount;
  FreeSetAllocator* mAllocator;
  FreeSet*          mFreeSets;
  size_t*           mFibonaccis;
  FibonacciCell*    mAllocationDirections;
  void*             mPool;
  uint8_t*          mData;
  size_t            mFreeSpace;

public:
  FibonacciMemoryManager(void* aMemory, bool const aExactAllocation);
  
  FibonacciMemoryManager(bool const aExactAllocation) : FibonacciMemoryManager(reinterpret_cast<void*>(tMemory), aExactAllocation) {
  }

  size_t getFibonacciCount() const noexcept {
    return mFibonacciCount;
  }

  size_t getFibonacci(size_t const aIndex) const noexcept {
    return mFibonaccis[std::min<size_t>(aIndex, mFibonacciCount - 1u)];
  }

  size_t getMaxFibonacci() const noexcept {
    return mFibonaccis[mFibonacciCount - 1u];
  }

  size_t getFreeSpace() const noexcept {
    return mFreeSpace;
  }

  size_t getMaxUserBlockSize() const noexcept {
    return getUserBlockSize(mFibonacciCount - 1u);
  }

  size_t getTechnicalBlockSize() const noexcept {
    return mBlockSize;
  }

  size_t getLargestFreeIndex() const noexcept;

  size_t getMaxFreeUserBlockSize() const noexcept {
    size_t fibonacciIndex = getLargestFreeIndex();
    return fibonacciIndex < mFibonacciCount ? getUserBlockSize(fibonacciIndex) : 0u;
  }

  size_t static getAlignment() noexcept {
    return tAlignment;
  }

  void* allocate(size_t const aSize);
  void deallocate(void* const aPointer);

  bool isCorrectEmpty() const noexcept;

private:
  void* alignTo(void* const aPointer, size_t const aAlign) {
    void* pointer = aPointer;
    size_t bufLen = aAlign * 2u;
    return std::align(aAlign, 1u, pointer, bufLen);
  }

  void* alignToMax(void* const aPointer) {
    return alignTo(aPointer, alignof(std::max_align_t));
  }

  static size_t calculateFibonaccis(size_t* const aResult, size_t const aMaxCount, size_t const aMaxValue) noexcept;
  size_t calculateTotalHeaderSize(size_t const * const aFibonaccis, size_t const aFibonacciCount) noexcept;
  void initInternalData(void* aMemory) noexcept;
  void fillAllocationDirections() noexcept;

  FibonacciCell& allocationDirectionAt(size_t const aIndexBig, size_t const aIndexSmall) noexcept {
    return mAllocationDirections[aIndexBig * mFibonacciCount + aIndexSmall];
  }

  size_t getUserBlockSize(size_t const aFibonacciIndex) const noexcept {
    return mBlockSize * mFibonaccis[aFibonacciIndex] - tAlignment;
  }
};

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory = 0u>
class NewDelete final {
private: 
  static FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference>* sFibonacci; // could be inline for c++17

  template<typename tClass, typename ...tParameters>
  struct Wrapper final {
  public:
    tClass mPayload;

    Wrapper(tParameters... aParameters) : mPayload(aParameters...) {
    }

    void* operator new(size_t aSize) {
      return sFibonacci->allocate(aSize);
    }

    void* operator new[](size_t aSize) {
      return sFibonacci->allocate(aSize);
    }

    void operator delete(void* aPointer) {
      sFibonacci->deallocate(aPointer);
    }

    void operator delete[](void* aPointer) {
      sFibonacci->deallocate(aPointer);
    }
  };

public:
  static void init(bool const aExactAllocation) { 
    sFibonacci = new(tMemory) FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>(aExactAllocation);
  }

  static void init(void* aMemory, bool const aExactAllocation) { 
    sFibonacci = new(aMemory) FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>(aMemory, aExactAllocation);
  }

  template<typename tClass, typename ...tParameters>
  static tClass* _new(tParameters... aParameters) {
    Wrapper<tClass, tParameters...> *wrapper = new Wrapper<tClass, tParameters...>(aParameters...);
    return &wrapper->mPayload;
  }

  template<typename tClass>
  static tClass* _newArray(size_t const aCount) {
    Wrapper<tClass> *wrapper = new Wrapper<tClass>[aCount];
    return &wrapper->mPayload;
  }

  template<typename tClass>
  static void _delete(tClass* aPointer) {
    delete reinterpret_cast<Wrapper<tClass>*>(aPointer);
  }

  template<typename tClass>
  static void _deleteArray(tClass* aPointer) {
    delete[] reinterpret_cast<Wrapper<tClass>*>(aPointer);
  }
  
  static size_t getFreeSpace() noexcept {
    return sFibonacci->getFreeSpace();
  }

  static size_t getMaxUserBlockSize() noexcept {
    return sFibonacci->getMaxUserBlockSize();
  }

  static size_t getMaxFreeUserBlockSize() noexcept {
    return sFibonacci->getMaxFreeUserBlockSize();
  }

  static size_t getAlignment() noexcept {
    return sFibonacci->getAlignment();
  }
  
  static bool isCorrectEmpty() noexcept {
    return sFibonacci->isCorrectEmpty();
  }
};

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference>* NewDelete<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::sFibonacci;

/// This class may be instantiated on the beginning of aMemory using placement new.
template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::FibonacciMemoryManager(void* aMemory, bool const aExactAllocation) 
  : mExactAllocation(aExactAllocation) {
  bool failed = false;
  mBlockSize = tMinimalBlockSize;
  size_t* fibonaccis;
  if(reinterpret_cast<uintptr_t>(aMemory) % alignof(std::max_align_t) == 0u) {
    void* tmpFree = alignToMax(static_cast<uint8_t*>(aMemory) + sizeof(*this));
    mSetNodeSize = AllocatorBlockGauge<std::set<void*>>::getNodeSize(tmpFree, nullptr);
    fibonaccis = static_cast<size_t*>(tmpFree);
    mFibonacciCount = reinterpret_cast<size_t*>(reinterpret_cast<uint8_t*>(aMemory) + tMemorySize) - fibonaccis;
    mFibonacciCount = calculateFibonaccis(fibonaccis, mFibonacciCount, tMemorySize);
    while(tMemorySize / fibonaccis[mFibonacciCount - 1u] < mBlockSize) {
      --mFibonacciCount;
    }
  }
  else {
    failed = true;
  }
  if(!failed && mFibonacciCount > 2u + tFibonacciIndexDifference) {
    size_t headerSize = calculateTotalHeaderSize(fibonaccis, mFibonacciCount);
    mBlockSize = (tMemorySize - headerSize) / fibonaccis[mFibonacciCount - 1u];
    mBlockSize &= ~(tAlignment - 1u);
    while(!failed && (headerSize > tMemorySize || tMemorySize - mBlockSize * fibonaccis[mFibonacciCount - 1u] < headerSize || mBlockSize < tMinimalBlockSize)) {
      --mFibonacciCount;
      if(mFibonacciCount > 2u + tFibonacciIndexDifference) {
        mBlockSize = (tMemorySize - headerSize) / fibonaccis[mFibonacciCount - 1u];
        mBlockSize &= ~(tAlignment - 1u);
        headerSize = calculateTotalHeaderSize(fibonaccis, mFibonacciCount);
      }
      else {
        failed = true;
      }
    }
  }
  else {
    failed = true;
  }
  if(!failed) {
    initInternalData(aMemory);
  }
  else { // nothing to do
    tInterface::badAlloc();
  }
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
size_t FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::getLargestFreeIndex() const noexcept {
  size_t fibonacciIndex;
  for(fibonacciIndex = mFibonacciCount - 1u; fibonacciIndex < mFibonacciCount; --fibonacciIndex) {
    if(mFreeSets[fibonacciIndex].size() > 0u) {
      break;
    }
    else { // nothing to do
    }
  }
  return std::min<size_t>(fibonacciIndex, mFibonacciCount);
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
void* FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::allocate(size_t const aSize) {
  tInterface::lock();
  size_t smallestSuitableIndex = mFibonacciCount;
  size_t fibonacciIndex = mFibonacciCount;
  size_t sizeWithHeader = aSize + tAlignment;
  bool failed = (sizeWithHeader < tAlignment || aSize == 0u);
  size_t sizeInUnitBlocks = (sizeWithHeader + mBlockSize - 1u) / mBlockSize;
  if(!failed) {
    smallestSuitableIndex = std::upper_bound(mFibonaccis, mFibonaccis + mFibonacciCount, sizeInUnitBlocks) - mFibonaccis;
    if(smallestSuitableIndex > 0u && mFibonaccis[smallestSuitableIndex - 1u] == sizeInUnitBlocks) {
      --smallestSuitableIndex;
    }
    else { // nothing to do
    }
  }
  else { // nothing to do
  }
  if(!failed && mExactAllocation) {
    fibonacciIndex = smallestSuitableIndex;
    while(fibonacciIndex < mFibonacciCount && 
          (mFreeSets[fibonacciIndex].size() == 0u ||
          !allocationDirectionAt(fibonacciIndex, smallestSuitableIndex).isExact())) {
      ++fibonacciIndex;
    }
  }
  else { // nothing to do
  }
  if(!failed && fibonacciIndex == mFibonacciCount) {
    fibonacciIndex = smallestSuitableIndex;
    while(fibonacciIndex < mFibonacciCount && 
          mFreeSets[fibonacciIndex].size() == 0u) {
      ++fibonacciIndex;
    }
    if(fibonacciIndex == mFibonacciCount) {
      failed = true;
    }
    else { // nothing to do
    }
  }
  else { // nothing to do
  }
  void* pointer = nullptr;
  if(!failed) {             // now fibonacciIndex contains a block size index which perhaps needs to be split
    auto begin = mFreeSets[fibonacciIndex].begin();
    void* parent = *begin;
    mFreeSets[fibonacciIndex].erase(begin);
    mFreeSpace -= getUserBlockSize(fibonacciIndex);
    while(fibonacciIndex > smallestSuitableIndex && allocationDirectionAt(fibonacciIndex, smallestSuitableIndex).getDirection() != FibonacciDirection::cHere) {
      BlockHeader* header = static_cast<BlockHeader*>(parent);
      bool buddy = header->getBuddy();
      bool memory = header->getMemory();
      size_t leftIndex = fibonacciIndex - tFibonacciIndexDifference - 1u;
      size_t rightIndex = fibonacciIndex - 1u;
      void* leftChild = parent;
      void* rightChild = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(parent) + mBlockSize * mFibonaccis[leftIndex]);
      static_cast<BlockHeader*>(leftChild)->set(false, buddy, leftIndex);
      static_cast<BlockHeader*>(rightChild)->set(true, memory, rightIndex);
      if(allocationDirectionAt(fibonacciIndex, smallestSuitableIndex).getDirection() == FibonacciDirection::cLeft) {
        mFreeSets[rightIndex].insert(reinterpret_cast<uint8_t*>(rightChild));
        parent = leftChild;
        fibonacciIndex = leftIndex;
        mFreeSpace += getUserBlockSize(rightIndex);
      }
      else {
        mFreeSets[leftIndex].insert(reinterpret_cast<uint8_t*>(leftChild));
        parent = rightChild;
        fibonacciIndex = rightIndex;
        mFreeSpace += getUserBlockSize(leftIndex);
      }
    }
    pointer = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(parent) + tAlignment);
  }
  else { // nothing to do
  }
  if(failed) {
    tInterface::badAlloc();
  }
  else { // nothing to do
  }
  tInterface::unlock();
  return pointer;
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
void FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::deallocate(void* const aPointer) {
  tInterface::lock();
  if(aPointer != nullptr) {
    uint8_t* blockStart = reinterpret_cast<uint8_t*>(aPointer) - tAlignment;
    if(reinterpret_cast<uintptr_t>(blockStart) % tAlignment == 0u && blockStart >= mData && blockStart < mData + mBlockSize * mFibonaccis[mFibonacciCount - 1u]) {
      BlockHeader* blockHeader = reinterpret_cast<BlockHeader*>(blockStart);
      size_t blockIndex = blockHeader->getIndex();
      uint8_t* buddyStart = nullptr;
      size_t   buddyIndex = mFibonacciCount;
      bool     buddyFound;
      do {
        if(blockIndex < mFibonacciCount - 1u) {
          bool blockBuddyBit = blockHeader->getBuddy();
          if(blockBuddyBit) {
            buddyIndex = blockIndex - tFibonacciIndexDifference;
            buddyStart = blockStart - mBlockSize * mFibonaccis[buddyIndex];
          }
          else {
            buddyIndex = blockIndex + tFibonacciIndexDifference;
            buddyStart = blockStart + mBlockSize * mFibonaccis[blockIndex];
          }
          auto found = mFreeSets[buddyIndex].find(buddyStart);
          buddyFound = (found != mFreeSets[buddyIndex].end());
          if(buddyFound) {
            BlockHeader* buddyHeader = reinterpret_cast<BlockHeader*>(buddyStart);
            mFreeSets[buddyIndex].erase(found);
            mFreeSpace -= getUserBlockSize(buddyIndex);
            bool blockMemoryBit;
            if(blockBuddyBit) {
              blockBuddyBit = buddyHeader->getMemory();
              blockMemoryBit = blockHeader->getMemory();
              ++blockIndex;
              blockStart = buddyStart;
              blockHeader = buddyHeader;
            }
            else {
              blockBuddyBit = blockHeader->getMemory();
              blockMemoryBit = buddyHeader->getMemory();
              blockIndex += tFibonacciIndexDifference + 1u;
              // block* pointers remain the same
            }
            blockHeader->set(blockBuddyBit, blockMemoryBit, blockIndex);
          }
          else { // nothing to do
          }
        }
        else {
          buddyFound = false;
        }
      } while(buddyFound);
      mFreeSets[blockIndex].insert(blockStart);
      mFreeSpace += getUserBlockSize(blockIndex);
    }
    else {
      tInterface::badAlloc();
    }
  }
  else { // nothing to do
  }
  tInterface::unlock();
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
bool FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::isCorrectEmpty() const noexcept {
  tInterface::lock();
  auto found = std::find_if(mFreeSets, mFreeSets + mFibonacciCount, [](auto &set){
    return set.size() > 0u;
  });
  bool result = (found - mFreeSets == mFibonacciCount - 1u && found->size() == 1u && mFreeSpace == getUserBlockSize(mFibonacciCount - 1u));
  tInterface::unlock();
  return result;
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
size_t FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::calculateFibonaccis(size_t* const aResult, size_t const aMaxCount, size_t const aMaxValue) noexcept {
  intptr_t index = tFibonacciIndexDifference + 1u;
  std::iota(aResult, aResult + index, 1u);
  size_t next = 0u;
  while(index < aMaxCount && next < aMaxValue) {
    next = aResult[index - 1u] + aResult[index - 1u - tFibonacciIndexDifference];
    aResult[index] = next;
    ++index;
  }
  return static_cast<size_t>(index);
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
size_t FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::calculateTotalHeaderSize(size_t const * const aFibonaccis, size_t const aFibonacciCount) noexcept {
  return sizeof(*this)
  + alignof(FreeSet)          + aFibonacciCount * sizeof(FreeSet)
  + alignof(size_t)           + aFibonacciCount * sizeof(size_t)
  + alignof(FibonacciCell)    + aFibonacciCount * aFibonacciCount * sizeof(FibonacciCell)
  + alignof(std::max_align_t) + aFibonaccis[aFibonacciCount - 2u - tFibonacciIndexDifference] * mSetNodeSize
  + tAlignment;
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
void FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::initInternalData(void* aMemory) noexcept {
  uint8_t* allocatorLocation = static_cast<uint8_t*>(alignTo(reinterpret_cast<uint8_t*>(aMemory) + sizeof(*this), alignof(FreeSetAllocator)));
  mFreeSets = static_cast<FreeSet*>(alignTo(reinterpret_cast<uint8_t*>(allocatorLocation) + sizeof(FreeSetAllocator), alignof(FreeSet)));
  mFibonaccis = static_cast<size_t*>(alignTo(reinterpret_cast<uint8_t*>(mFreeSets) + mFibonacciCount * sizeof(FreeSet), alignof(size_t)));
  calculateFibonaccis(mFibonaccis, mFibonacciCount, tMemorySize);
  mAllocationDirections = new(alignTo(mFibonaccis + mFibonacciCount, alignof(FibonacciCell))) FibonacciCell[mFibonacciCount * mFibonacciCount];
  fillAllocationDirections();
  mPool = alignToMax(mAllocationDirections + mFibonacciCount * mFibonacciCount);
  FixedOccupier occupier(mPool);
  size_t poolSize = mFibonaccis[mFibonacciCount - 2u - tFibonacciIndexDifference];
  mAllocator = new(allocatorLocation) FreeSetAllocator(poolSize, mSetNodeSize, occupier);
  for(size_t i = 0; i < mFibonacciCount; ++i) {
    new(mFreeSets + i) FreeSet(*mAllocator);
  }
  void* data = alignTo(reinterpret_cast<uint8_t*>(mPool) + poolSize * mSetNodeSize, tAlignment);
  mData = reinterpret_cast<uint8_t*>(data);
  static_cast<BlockHeader*>(data)->set(false, false, mFibonacciCount - 1u);
  mFreeSets[mFibonacciCount - 1u].insert(mData);
  mFreeSpace = getMaxUserBlockSize();
  mReady = true;
}

template <typename tInterface, size_t tMemorySize, size_t tMinimalBlockSize, size_t tAlignment, size_t tFibonacciIndexDifference, uintptr_t tMemory>
void FibonacciMemoryManager<tInterface, tMemorySize, tMinimalBlockSize, tAlignment, tFibonacciIndexDifference, tMemory>::fillAllocationDirections() noexcept {
  for(size_t i = 0u; i < mFibonacciCount; ++i) {
    allocationDirectionAt(i, i).set(true);
  }
  for(size_t i = 1u; i <= tFibonacciIndexDifference; ++i) {
    for(size_t j = 0u; j < i; ++j) {
      allocationDirectionAt(i, j).set(false);
    }
  }
  if(mExactAllocation) {
    for(size_t i = tFibonacciIndexDifference + 1u; i < mFibonacciCount; ++i) {
      for(size_t j = 0u; j < i; ++j) {
        auto& leftChild = allocationDirectionAt(i - tFibonacciIndexDifference - 1u, j);
        auto& rightChild = allocationDirectionAt(i - 1u, j);
        if(j <= i - tFibonacciIndexDifference - 1u && leftChild.isExact()) {
          allocationDirectionAt(i, j).set(true, FibonacciDirection::cLeft);
        }
        else if(rightChild.isExact()) {
          allocationDirectionAt(i, j).set(true, FibonacciDirection::cRight);
        }
        else if(j <= i - tFibonacciIndexDifference - 1u) {
          allocationDirectionAt(i, j).set(false, FibonacciDirection::cLeft);
        }
        else {
          allocationDirectionAt(i, j).set(false, FibonacciDirection::cRight);
        }
      }
    }
  }
  else {
    for(size_t i = tFibonacciIndexDifference + 1u; i < mFibonacciCount; ++i) {
      for(size_t j = 0u; j < i; ++j) {
        if(j <= i - tFibonacciIndexDifference - 1u) {
          bool leftExact = allocationDirectionAt(i - tFibonacciIndexDifference - 1u, j).isExact();
          allocationDirectionAt(i, j).set(leftExact, FibonacciDirection::cLeft);
        }
        else {
          bool rightExact = allocationDirectionAt(i - 1u, j).isExact();
          allocationDirectionAt(i, j).set(rightExact, FibonacciDirection::cRight);
        }
      }
    }
  }
}


}

#endif
