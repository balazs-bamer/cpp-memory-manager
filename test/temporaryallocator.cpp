#include "TemporaryAllocator.h"
#include <iostream>
#include <algorithm>
#include <forward_list>
#include <deque>
#include <list>
#include <map>
#include <set>

using namespace nowtech::memory;

constexpr size_t cCount          =   55555u;
constexpr size_t cRingbufferSize = 16777216;

class FixedOccupier final {
private:
  uint8_t *mMemory;

public:
  FixedOccupier(size_t const aLen) noexcept : mMemory(new uint8_t[aLen]) {
  }

  ~FixedOccupier() {
    delete[] mMemory;
  }

  void* occupy(size_t const aSize) noexcept {
std::cout << "o: " << aSize << ' ' << static_cast<void*>(mMemory) << ' ' << static_cast<void*>(this) <<'\n';
    return mMemory;
  }

  void release(void* const aPointer) noexcept {
    // nothing to do
  }

  void badAlloc() {
    throw false;
  }
} gOccupier1(cRingbufferSize), gOccupier2(cRingbufferSize);

template<typename tCont>
void print(char const * const aPrefix, tCont const &aCont) {
  std::cout << aPrefix;
  std::for_each(aCont.begin(), aCont.end(), [](auto i){ 
//    std::cout << i << ' ';
  });
  std::cout << '\n';
}

template<typename tKey, typename tValue, typename tComp, typename tAlloc>
void print(char const * const aPrefix, std::map<tKey, tValue, tComp, tAlloc> const &aCont) {
  std::cout << aPrefix;
  std::for_each(aCont.begin(), aCont.end(), [](auto i){ 
//    std::cout << i.first << ' ' << i.second << ' ';
  });
  std::cout << '\n';
}

void testForwardList() {
  std::cout << "testForwardList\n";
  TemporaryAllocator<uint32_t, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  TemporaryAllocator<uint32_t, FixedOccupier> alloc2(cRingbufferSize, gOccupier2);
  std::forward_list<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list1(alloc1);
  std::forward_list<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list2(alloc2);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.push_front(i);
    list2.push_front(i);
  }
  print("fwd: ", list1);
  print("fwd: ", list2);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.pop_front();
    list2.pop_front();
  }
  print("fwd: ", list1);
  print("fwd: ", list2);
}

void testDeque() {
  std::cout << "testDeque\n";
  TemporaryAllocator<uint32_t, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  TemporaryAllocator<uint32_t, FixedOccupier> alloc2(cRingbufferSize, gOccupier2);
  std::deque<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list1(alloc1);
  std::deque<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list2(alloc2);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.push_front(i);
    list2.push_front(i);
  }
  print("deq: ", list1);
  print("deq: ", list2);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.pop_front();
    list2.pop_front();
  }
  print("deq: ", list1);
  print("deq: ", list2);
}

void testList() {
  std::cout << "testList\n";
  TemporaryAllocator<uint32_t, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  TemporaryAllocator<uint32_t, FixedOccupier> alloc2(cRingbufferSize, gOccupier2);
  std::list<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list1(alloc1);
  std::list<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list2(alloc2);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.push_front(i);
    list2.push_front(i);
  }
  print("lst: ", list1);
  print("lst: ", list2);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.pop_front();
    list2.pop_front();
  }
  print("lst: ", list1);
  print("lst: ", list2);
}

void testSet() {
  std::cout << "testSet\n";
  TemporaryAllocator<uint32_t, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  TemporaryAllocator<uint32_t, FixedOccupier> alloc2(cRingbufferSize, gOccupier2);
  std::set<uint32_t, std::less<uint32_t>, TemporaryAllocator<uint32_t, FixedOccupier>> tree1(alloc1);
  std::set<uint32_t, std::less<uint32_t>, TemporaryAllocator<uint32_t, FixedOccupier>> tree2(alloc2);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1.insert(i);
    tree2.insert(i);
  }
  print("set: ", tree1);
  print("set: ", tree2);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1.erase(i);
    tree2.erase(i);
  }
  print("set: ", tree1);
  print("set: ", tree2);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1.insert(i);
    tree2.insert(i);
  }
  print("set: ", tree1);
  print("set: ", tree2);

  for(uint32_t i = cCount - 1u; i < cCount; --i) {
    tree1.erase(i);
    tree2.erase(i);
  }
  print("set: ", tree1);
  print("set: ", tree2);
}

void testMap() {
  std::cout << "testMap\n";
  TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier> alloc2(cRingbufferSize, gOccupier2);
  std::map<uint32_t, uint32_t, std::less<uint32_t>, TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier>> tree1(alloc1);
  std::map<uint32_t, uint32_t, std::less<uint32_t>, TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier>> tree2(alloc2);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1[i] = i;
    tree2[i] = i;
  }
  print("map: ", tree1);
  print("map: ", tree2);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1.erase(i);
    tree2.erase(i);
  }
  print("map: ", tree1);
  print("map: ", tree2);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1[i] = i;
    tree2[i] = i;
  }
  print("map: ", tree1);
  print("map: ", tree2);

  for(uint32_t i = cCount - 1u; i < cCount; --i) {
    tree1.erase(i);
    tree2.erase(i);
  }
  print("map: ", tree1);
  print("map: ", tree2);
  
  // for(uint32_t i = 0; i <= cCount; ++i) { // dumps core
  //  tree1[i] = i;
  //}
}

void testCopyMoveSwapFwd() {
  std::cout << "testCopyMoveSwapFwd\n";
  TemporaryAllocator<uint32_t, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  TemporaryAllocator<uint32_t, FixedOccupier> alloc2(cRingbufferSize, gOccupier2);
  std::forward_list<uint32_t, TemporaryAllocator<uint32_t, FixedOccupier>> list1(alloc1);

  for(uint32_t i = 0; i < cCount; ++i) {
    list1.push_front(i);
  }
  print("l1 orig: ", list1);

//  std::forward_list<uint32_t, TemporaryAllocator<uint32_t>> list2(alloc2);
//  list2 = list1;

//  std::forward_list<uint32_t, TemporaryAllocator<uint32_t>> list2(alloc1); dumps core, because two allocators will share the same pool and attempt to deallocate the contents twice
//  list2 = list1;

//  std::forward_list<uint32_t, TemporaryAllocator<uint32_t>> list2(alloc1); dumps core, because two allocators will share the same pool and attempt to deallocate the contents twice
//  list2 = std::move(list1);

  /*std::forward_list<uint32_t, TemporaryAllocator<uint32_t>> list2(alloc1); dumps core, because two allocators will share the same pool and attempt to deallocate the contents twice
  std::swap(list2, list1);
  print("l1 swap: ", list1);
  print("l2 swap: ", list2);*/
}
void testSwapMap() {
  std::cout << "testSwapMap\n";
  TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier> alloc1(cRingbufferSize, gOccupier1);
  std::map<uint32_t, uint32_t, std::less<uint32_t>, TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier>> tree1(alloc1);
  std::map<uint32_t, uint32_t, std::less<uint32_t>, TemporaryAllocator<std::pair<uint32_t, uint32_t>, FixedOccupier>> tree2(alloc1);

  for(uint32_t i = 0; i < cCount; ++i) {
    tree1[i] = i;
  }
  print("m1 orig: ", tree1);

  // std::swap(tree2, tree1);   dumps core, because two allocators will share the same pool and attempt to deallocate the contents twice
  // print("m1 swap: ", tree1);
  // print("m2 swap: ", tree2);
}

int main() {
  testForwardList();
  testDeque();
  testList();
  testSet();
  testMap();
  testCopyMoveSwapFwd();
  testSwapMap();
  return 0;
}
