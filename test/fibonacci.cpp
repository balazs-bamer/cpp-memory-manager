#include "FibonaccyMemoryManager.h"
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <list>
#include <deque>
#include <random>
#include <chrono>

using namespace nowtech::memory;

class Interface final {
public:
  static void badAlloc() {
    std::cout << "bad alloc\n";
  }
  static void lock() {
    std::cout << " -=###=- lock\n";
  }

  static void unlock() {
    std::cout << " -=###=- unlock\n\n";
  }
};

char cSeparator[] = "\n----------------------------------------------------\n\n";
constexpr size_t cMemorySize          =  1024u * 512u;
constexpr size_t cMinBlockSize        =   128u;
constexpr size_t cUserAlign           =     8u;
constexpr size_t cFibonacciDifference =     3u;
constexpr size_t cDiverseAllocCount   = 11111u;
constexpr size_t cPoolSize            =   111u;

typedef FibonacciMemoryManager<Interface, cMemorySize, cMinBlockSize, cUserAlign, cFibonacciDifference> Fibonacci;

void testUniform(size_t const aSize, bool const aExact, bool const aDeallocReverse) noexcept {
  std::cout << " -=###=- size: " << aSize << (aExact ? " exact" : " inexact" ) << "\n\n";
  uint8_t* mem;
  try {
    mem = new uint8_t[cMemorySize];
    void* buffer = reinterpret_cast<void*>(mem);
    Fibonacci* fib = new(buffer) Fibonacci(buffer, aExact);
    std::cout << "free: " << fib->getFreeSpace() << " max user: " << fib->getMaxUserBlockSize() << '\n';
    size_t count = 0u;
    void* pointer;
    std::list<void*> pointers;
    while((pointer = fib->allocate(aSize)) != nullptr) {
      pointers.push_back(pointer);
      std::cout << "free after alloc: " << fib->getFreeSpace() << '\n';
      ++count;
    }
    std::cout << "allocated blocks: " << count << " of size " << aSize << (aExact ? " exact" : " inexact" ) << "\n\n";
    while(pointers.size() > 0u) {
      if(aDeallocReverse) {
        pointer = pointers.back();
        pointers.pop_back();
      }
      else {
        pointer = pointers.front();
        pointers.pop_front();
      }
      fib->deallocate(pointer);
      std::cout << "free after dealloc: " << fib->getFreeSpace() << '\n';
    }
    if(!fib->isCorrectEmpty()) {
      std::cout << "!!!!!!!!!!!!!!!!! corrupt after freeing everything !!!!!!!!!!!!!!!!!!\n";
    }
    else {  // nothing to do
    }
    delete[] mem;
  }
  catch(...) {
    delete[] mem;
  }
  std::cout << cSeparator;
}

void testDiverse(size_t const aAllocCount, bool const aExact) noexcept {
  std::cout << " -=###=- count: " << aAllocCount << (aExact ? " exact" : " inexact" ) << "\n\n";
  uint8_t* mem;
  try {
    mem = new uint8_t[cMemorySize];
    void* buffer = reinterpret_cast<void*>(mem);
    Fibonacci* fib = new(buffer) Fibonacci(buffer, aExact);
    std::cout << "free: " << fib->getFreeSpace() << " max user: " << fib->getMaxUserBlockSize() << '\n';
    std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<double> uniformDouble(0.0, 1.0);
    std::uniform_int_distribution<size_t> uniformCount(0u, fib->getFibonacciCount() - 1u);
    std::uniform_int_distribution<size_t> uniformFreeSpace(0u, fib->getMaxUserBlockSize());
    std::deque<void*> pointers;
    size_t count = 0u;
    while(count < aAllocCount) {
      if(uniformFreeSpace(generator) < fib->getFreeSpace()) {
        size_t whichFibonacci = std::min<size_t>(uniformCount(generator), fib->getLargestFreeIndex());
        size_t upperLimit = fib->getTechnicalBlockSize() * fib->getFibonacci(whichFibonacci) - cUserAlign;
        size_t lowerLimit = (whichFibonacci == 0u ? 0u : fib->getTechnicalBlockSize() * fib->getFibonacci(whichFibonacci - 1u) - cUserAlign) + 1u;
        size_t toAllocate = lowerLimit + static_cast<size_t>(uniformDouble(generator) * (upperLimit - lowerLimit));
        pointers.push_back(fib->allocate(toAllocate));
        std::cout << "########## al: " << static_cast<double>(fib->getFreeSpace()) / fib->getMaxUserBlockSize() << ' ' << std::setw(2) << whichFibonacci << ' ' << std::setw(5) << toAllocate << '\n';
        ++count;
      }
      else if(pointers.size() > 0u) { // dealloc
        size_t which = std::max<size_t>(static_cast<size_t>(uniformDouble(generator) * pointers.size()), pointers.size() - 1u);
        fib->deallocate(pointers[which]);
        pointers.erase(pointers.begin() + which);
        std::cout << "########## de: " << static_cast<double>(fib->getFreeSpace()) / fib->getMaxUserBlockSize() << '\n';
      }
      else { // nothing to do
      }
    }
    while(pointers.size() > 0u) {
      void* pointer = pointers.back();
      pointers.pop_back();
      fib->deallocate(pointer);
      std::cout << "########## de: " << static_cast<double>(fib->getFreeSpace()) / fib->getMaxUserBlockSize() << '\n';
    }
    if(!fib->isCorrectEmpty()) {
      std::cout << "########## !!!!!!!!!!!!!!!!! corrupt after freeing everything !!!!!!!!!!!!!!!!!!\n";
    }
    else {  // nothing to do
    }
    delete[] mem;
  }
  catch(...) {
    delete[] mem;
  }
  std::cout << cSeparator;
}

typedef NewDelete<Interface, cMemorySize, cMinBlockSize, cUserAlign, cFibonacciDifference> ExampleNewDelete;

class Test final {
  int    mI = 0u;
  double mD = 0.0;

public:
  Test() : mI(1), mD(2.2) {
    std::cout << " Test()\n";
  }

  Test(int aI, double aD) : mI(aI), mD(aD) {
    std::cout << " Test(int aI)\n";
  }

  ~Test() {
    std::cout << "~Test()\n";
  }

  void print() {
    std::cout << "print() " << mI << ' ' << mD << '\n';
  }
};

class NewDeleteOccupier final {
public:
  NewDeleteOccupier() noexcept = default;

  void* occupy(size_t const aSize) {
    std::cout << "@@@@ occupy: ";
    return reinterpret_cast<void*>(ExampleNewDelete::_newArray<uint8_t>(aSize));
  }

  void release(void* const aPointer) {
    std::cout << "@@@@ release: ";
    ExampleNewDelete::_deleteArray<uint8_t>(reinterpret_cast<uint8_t*>(aPointer));
  }
  
  void badAlloc() {
    throw std::bad_alloc();
  }
} gOccupier;

int main() {
  size_t technicalBlockSize;
  size_t maxUserBlockSize;
  size_t maxFibonacci;
  uint8_t mem[cMemorySize];
  {
    Fibonacci* fibonacci = new(reinterpret_cast<void*>(mem)) Fibonacci(reinterpret_cast<void*>(mem), false);
    technicalBlockSize = fibonacci->getTechnicalBlockSize();
    maxUserBlockSize   = fibonacci->getMaxUserBlockSize();
    maxFibonacci       = fibonacci->getMaxFibonacci();
    std::cout << "maxFibonacci: " << maxFibonacci << "  technicalBlockSize: " << technicalBlockSize << "  maxUserBlockSize " << maxUserBlockSize << "  alignment: " << cUserAlign << '\n';
    std::cout << cSeparator;
  }

  ExampleNewDelete::init(reinterpret_cast<void*>(mem), false);
  int* int1 = ExampleNewDelete::_new<int>();                     // allocate(4)
  Test* test1 = ExampleNewDelete::_new<Test>(2, 3.3);            // allocate(16)
  test1->print();
  Test* test2 = ExampleNewDelete::_newArray<Test>(2u);           // allocate(40)
  test2[0].print();
  test2[1].print();

  size_t nodeSize = AllocatorBlockGauge<std::set<int>>::getNodeSize(0u);
//  PoolAllocator<int, NewDeleteOccupier> allocator(cPoolSize, nodeSize, gOccupier);
  PoolAllocator<int, NewDeleteOccupier>* allocator = ExampleNewDelete::_new<PoolAllocator<int, NewDeleteOccupier>>(cPoolSize, nodeSize, gOccupier);         // allocate(72) -D_GLIBCXX_DEBUG
                                                                                                                                                            // allocate(4480)
  std::set<int, std::less<int>, PoolAllocator<int, NewDeleteOccupier>>* set1 = ExampleNewDelete::_new<std::set<int, std::less<int>, PoolAllocator<int, NewDeleteOccupier>>>(*allocator);     // allocate(144)
  set1->insert(1);
  set1->insert(2);
  std::cout << "set1->size() " << set1->size() << '\n';
  ExampleNewDelete::_delete<std::set<int, std::less<int>, PoolAllocator<int, NewDeleteOccupier>>>(set1);       // deallocate() *2
  ExampleNewDelete::_delete<PoolAllocator<int, NewDeleteOccupier>>(allocator);                                 // deallocate()

  ExampleNewDelete::_delete<int>(int1);                                                                        // deallocate()
  ExampleNewDelete::_delete<Test>(test1);                                                                      // deallocate()
  ExampleNewDelete::_deleteArray<Test>(test2);                                                                 // deallocate()
   
  std::cout << "Checking if everything freed.\n"; 
  if(!ExampleNewDelete::isCorrectEmpty()) {
    std::cout << "########## !!!!!!!!!!!!!!!!! corrupt after freeing everything !!!!!!!!!!!!!!!!!!\n";
  }
  else {  // nothing to do
  }
  std::cout << cSeparator;

  for(size_t i = 0u; i <= maxFibonacci; ++i) {
    size_t size = i * technicalBlockSize - cUserAlign;
    testUniform(size, true, true);
    testUniform(size, false, true);
    testUniform(size, true, false);
    testUniform(size, false, false);
  }
  testDiverse(cDiverseAllocCount, true);
  std::cout << "##########################################################\n";
  testDiverse(cDiverseAllocCount, false);
  return 0;
}
