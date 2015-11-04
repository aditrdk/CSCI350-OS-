// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "table.h"

#define UserStackSize		1024 	// increase this as necessary!
#define numStacks 100
#define MaxOpenFiles 256
#define MaxChildSpaces 256

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch
    void AllocateStack(int stackID);
    void DeallocateStack(int stackID);
    void DeallocatePage(int pageNo);
    Table fileTable;			// Table of openfiles
    unsigned int numStackPages;
    unsigned int numCodePages;
    Lock *stackMapLock;

    BitMap stackMap;
    int tableIndex;
    Lock *pageTableLock;
    TranslationEntry *pageTable;   // Assume linear page table translation
 private:
   
					// for now!
    unsigned int numPages;      // Number of pages of code, init, and uninit in the virtual 
                    // address space

};

#endif // ADDRSPACE_H
