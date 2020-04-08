#include "OnlyAllocate.h"
#include <iomanip>
#include <iostream>

using namespace nowtech::memory;

class Interface final {
public:
  static void badAlloc() {
    std::cout << "bad alloc\n";
  }
};

constexpr size_t cMemorySize           = 1024u;

typedef OnlyAllocate<Interface> MinMemMan;

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

int main() {
  uint8_t* mem = new uint8_t[cMemorySize];

  MinMemMan::init(reinterpret_cast<void*>(mem), cMemorySize);
  std::cout << " getFreeSpace() " << MinMemMan::getFreeSpace() << 
               " getMaxUserBlockSize() " << MinMemMan::getMaxUserBlockSize() <<
               " getMaxFreeUserBlockSize() " << MinMemMan::getMaxFreeUserBlockSize() <<
               "\n"; 

  int* int1 = MinMemMan::_new<int>();                     // allocate(4)
  Test* test1 = MinMemMan::_new<Test>(2, 3.3);            // allocate(16)
  test1->print();
  Test* test2 = MinMemMan::_newArray<Test>(2u);           // allocate(40)
  test2[0].print();
  test2[1].print();

  int* intN = MinMemMan::_newArray<int>(cMemorySize);  // signs bad alloc

  std::cout << " getFreeSpace() " << MinMemMan::getFreeSpace() << 
               " getMaxUserBlockSize() " << MinMemMan::getMaxUserBlockSize() <<
               " getMaxFreeUserBlockSize() " << MinMemMan::getMaxFreeUserBlockSize() <<
               "\n"; 

  MinMemMan::_delete<int>(int1);                                                                        // deallocate()
  MinMemMan::_delete<Test>(test1);                                                                      // deallocate()
  MinMemMan::_deleteArray<Test>(test2);                                                                 // deallocate()
   
  delete[] mem;
  return 0;
}
