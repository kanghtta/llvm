//===-- Intercept.cpp - System function interception routines -------------===//
//
// If a function call occurs to an external function, the JIT is designed to use
// dlsym on the current process to find a function to call.  This is useful for
// calling system calls and library functions that are not available in LLVM.
// Some system calls, however, need to be handled specially.  For this reason,
// we intercept some of them here and use our own stubs to handle them.
//
//===----------------------------------------------------------------------===//

#include "VM.h"
#include "Config/dlfcn.h"    // dlsym access
#include <iostream>

// AtExitList - List of functions registered with the at_exit function
static std::vector<void (*)()> AtExitList;

void VM::runAtExitHandlers() {
  while (!AtExitList.empty()) {
    void (*Fn)() = AtExitList.back();
    AtExitList.pop_back();
    Fn();
  }
}

//===----------------------------------------------------------------------===//
// Function stubs that are invoked instead of raw system calls
//===----------------------------------------------------------------------===//

// NoopFn - Used if we have nothing else to call...
static void NoopFn() {}

// jit_exit - Used to intercept the "exit" system call.
static void jit_exit(int Status) {
  VM::runAtExitHandlers();   // Run at_exit handlers...
  exit(Status);
}

// jit_atexit - Used to intercept the "at_exit" system call.
static int jit_atexit(void (*Fn)(void)) {
  AtExitList.push_back(Fn);    // Take note of at_exit handler...
  return 0;  // Always successful
}

//===----------------------------------------------------------------------===//
// 
/// getPointerToNamedFunction - This method returns the address of the specified
/// function by using the dlsym function call.  As such it is only useful for
/// resolving library symbols, not code generated symbols.
///
void *VM::getPointerToNamedFunction(const std::string &Name) {
  // Check to see if this is one of the functions we want to intercept...
  if (Name == "exit") return (void*)&jit_exit;
  if (Name == "atexit") return (void*)&jit_atexit;

  // If it's an external function, look it up in the process image...
  // On Sparc, RTLD_SELF is already defined and it's not zero
  // Linux/x86 wants to use a 0, other systems may differ
#ifndef RTLD_SELF
#define RTLD_SELF 0
#endif
  void *Ptr = dlsym(RTLD_SELF, Name.c_str());
  if (Ptr == 0) {
    std::cerr << "WARNING: Cannot resolve fn '" << Name
	      << "' using a dummy noop function instead!\n";
    Ptr = (void*)NoopFn;
  }
  
  return Ptr;
}
