// system.h 
//	All global variables used in Nachos are defined here.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SYSTEM_H
#define SYSTEM_H

#include "copyright.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "timer.h"
#include "bitmap.h"
#include "synch.h"
#include "table.h"
#include "iptentry.h"
#include "list.h"
struct KernelLock {
    Lock* lock;
    AddrSpace *owner;
    bool isToBeDeleted;
    int waitingThreads;
};

struct KernelCondition{
    Condition* condition;
    Lock* conditionLock;
    AddrSpace *owner;
    bool isToBeDeleted;
    int waitingThreads;
};

struct KernelProcess{
    int numThreads;
    int numRunningThreads;
    AddrSpace* pSpace;
};

// Initialization and cleanup routines
extern void Initialize(int argc, char **argv); 	// Initialization,
						// called before anything else
extern void Cleanup();				// Cleanup, called when
						// Nachos is done.

extern Thread *currentThread;			// the thread holding the CPU
extern Thread *threadToBeDestroyed;  		// the thread that just finished
extern Scheduler *scheduler;			// the ready list
extern Interrupt *interrupt;			// interrupt status
extern Statistics *stats;			// performance metrics
extern Timer *timer;				// the hardware alarm clock

extern BitMap memoryMap;
extern Lock *memoryMapLock;
extern IPTEntry* ipt;

extern Table processTable;
extern Lock *processTableLock;
#define maxProcesses 50
extern int numProcesses;

extern KernelLock *lockTable;
extern BitMap lockMap;
extern int numLocks;
extern Lock* lockTableLock;

extern KernelCondition *conditionTable;
extern BitMap conditionMap;
extern int numConditions;
extern Lock* conditionTableLock;

extern int currentTLBIndex;

extern Lock* swapFileLock;
extern OpenFile* swapFile;
extern BitMap swapFileMap;
extern bool RAND_REPLACEMENT;
extern int* pageIndices;
extern List* pageQueue;

#ifdef USER_PROGRAM
#include "machine.h"
extern Machine* machine;	// user program memory and registers
#endif

#ifdef FILESYS_NEEDED 		// FILESYS or FILESYS_STUB 
#include "filesys.h"
extern FileSystem  *fileSystem;
#endif

#ifdef FILESYS
#include "synchdisk.h"
extern SynchDisk   *synchDisk;
#endif

#ifdef NETWORK
#include "post.h"
extern PostOffice* postOffice;
#endif

#endif // SYSTEM_H
