#ifndef NOWTECH_ONLYALLOCATE
#define NOWTECH_ONLYALLOCATE

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace nowtech::memory {

// class Interface final {
// public:
//   static void badAlloc() {
// };

template <typename tInterface, uintptr_t tMemory = 0u, size_t tSize = 0u>
class OnlyAllocate final {
  static_assert(tMemory % alignof(std::max_align_t) == 0u);
  static_assert(tSize   % alignof(std::max_align_t) == 0u);

private: 
  static std::atomic<uint8_t*> sGetPointer;
  static size_t                sSize;
  static uint8_t*              sEnd;

  template<typename tClass, typename ...tParameters>
  struct Wrapper final {
  public:
    tClass mPayload;

    Wrapper(tParameters... aParameters) : mPayload(aParameters...) {
    }

    void* operator new(size_t aSize) {
      return allocate(aSize);
    }

    void* operator new[](size_t aSize) {
      return allocate(aSize);
    }

    void operator delete(void* aPointer) { // nothing to do
    }

    void operator delete[](void* aPointer) { // nothing to do
    }
  };

public:
  static void init() { 
    sGetPointer = reinterpret_cast<uint8_t*>(tMemory);
    sSize = tSize;
    sEnd = sGetPointer + sSize;
  }

  static void init(void* aMemory, size_t const aSize) { 
    if(reinterpret_cast<uintptr_t>(aMemory) % alignof(std::max_align_t) == 0u && aSize % alignof(std::max_align_t) == 0u) {
      sGetPointer = reinterpret_cast<uint8_t*>(aMemory);
      sSize = aSize;
      sEnd = sGetPointer + sSize;
    }
    else {
      tInterface::badAlloc();
    }
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
    return sEnd - sGetPointer;
  }

  static size_t getMaxUserBlockSize() noexcept {
    return sEnd - sGetPointer;
  }

  static size_t getMaxFreeUserBlockSize() noexcept {
    return sSize;
  }

protected:
  static void* allocate(size_t const aSize) {
    uint8_t* pointer = sGetPointer.fetch_add(aSize);
    if(pointer + aSize > sEnd) {
      tInterface::badAlloc();
    }
    else { // nothing to do
    }
    return static_cast<void*>(pointer);
  }
};

template <typename tInterface, uintptr_t tMemory, size_t tSize>
size_t OnlyAllocate<tInterface, tMemory, tSize>::sSize;

template <typename tInterface, uintptr_t tMemory, size_t tSize>
uint8_t* OnlyAllocate<tInterface, tMemory, tSize>::sEnd;

template <typename tInterface, uintptr_t tMemory, size_t tSize>
std::atomic<uint8_t*> OnlyAllocate<tInterface, tMemory, tSize>::sGetPointer;

}

#endif
