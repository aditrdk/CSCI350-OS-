   // exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>

extern "C" { int bzero(char *, int); };
using namespace std;

int copyin(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes from the current thread's virtual address vaddr.
    // Return the number of bytes so read, or -1 if an error occors.
    // Errors can generally mean a bad virtual address was passed in.
  bool result;
    int n=0;			// The number of bytes copied in
    int *paddr = new int;

    while ( n >= 0 && n < len) {
      result = machine->ReadMem( vaddr, 1, paddr );
      while(!result) // FALL 09 CHANGES
      {
   			result = machine->ReadMem( vaddr, 1, paddr ); // FALL 09 CHANGES: TO HANDLE PAGE FAULT IN THE ReadMem SYS CALL
       }	

       buf[n++] = *paddr;

       if ( !result ) {
	//translation failed
         return -1;
       }

       vaddr++;
     }

     delete paddr;
     return len;
   }

   int copyout(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes to the current thread's virtual address vaddr.
    // Return the number of bytes so written, or -1 if an error
    // occors.  Errors can generally mean a bad virtual address was
    // passed in.
    bool result;
    int n=0;			// The number of bytes copied in

    while ( n >= 0 && n < len) {
      // Note that we check every byte's address
      result = machine->WriteMem( vaddr, 1, (int)(buf[n++]) );

      if ( !result ) {
	//translation failed
       return -1;
     }

     vaddr++;
   }

   return n;
 }

void Create_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];	// Kernel buffer to put the name in

    if (!buf) return;

    if( copyin(vaddr,len,buf) == -1 ) {
     printf("%s","Bad pointer passed to Create\n");
     delete buf;
     return;
   }

   buf[len]='\0';

   fileSystem->Create(buf,0);
   delete[] buf;
   return;
}

int Open_Syscall(unsigned int vaddr, int len) {
    // Open the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  If
    // the file is opened successfully, it is put in the address
    // space's file table and an id returned that can find the file
    // later.  If there are any errors, -1 is returned.
    char *buf = new char[len+1];	// Kernel buffer to put the name in
    OpenFile *f;			// The new open file
    int id;				// The openfile id

    if (!buf) {
     printf("%s","Can't allocate kernel buffer in Open\n");
     return -1;
   }

   if( copyin(vaddr,len,buf) == -1 ) {
     printf("%s","Bad pointer passed to Open\n");
     delete[] buf;
     return -1;
   }

   buf[len]='\0';

   f = fileSystem->Open(buf);
   delete[] buf;

   if ( f ) {
     if ((id = currentThread->space->fileTable.Put(f)) == -1 )
       delete f;
     return id;
   }
   else
     return -1;
}

void Write_Syscall(unsigned int vaddr, int len, int id) {
  // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one. For disk files, the file is looked
    // up in the current address space's open file table and used as
    // the target of the write.

    char *buf;		// Kernel buffer for output
    OpenFile *f;	// Open file for output

    if ( id == ConsoleInput) return;
    
    if ( !(buf = new char[len]) ) {
     printf("%s","Error allocating kernel buffer for write!\n");
     return;
   } else {
    if ( copyin(vaddr,len,buf) == -1 ) {
     printf("%s","Bad pointer passed to to write: data not written\n");
     delete[] buf;
     return;
   }
 }

 if ( id == ConsoleOutput) {
  for (int ii=0; ii<len; ii++) {
   printf("%c",buf[ii]);
 }

  } else {
	 if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
   f->Write(buf, len);
    } else {
   printf("%s","Bad OpenFileId passed to Write\n");
   len = -1;
    }
  }

  delete[] buf;
}

int Read_Syscall(unsigned int vaddr, int len, int id) {
  // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one.    We reuse len as the number of bytes
    // read, which is an unnessecary savings of space.
    char *buf;		// Kernel buffer for input
    OpenFile *f;	// Open file for output

    if ( id == ConsoleOutput) return -1;
    
    if ( !(buf = new char[len]) ) {
     printf("%s","Error allocating kernel buffer in Read\n");
     return -1;
   }

   if ( id == ConsoleInput) {
      //Reading from the keyboard
    scanf("%s", buf);

    if ( copyout(vaddr, len, buf) == -1 ) {
     printf("%s","Bad pointer passed to Read: data not copied\n");
   }
  } else {
   if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
     len = f->Read(buf, len);
     if ( len > 0 ) {
	        //Read something from the file. Put into user's address space
       if ( copyout(vaddr, len, buf) == -1 ) {
        printf("%s","Bad pointer passed to Read: data not copied\n");
      }
    }
    } else {
    printf("%s","Bad OpenFileId passed to Read\n");
    len = -1;
  }
  }

  delete[] buf;
  return len;
}

void Close_Syscall(int fd) {
  // Close the file associated with id fd.  No error reporting.
  OpenFile *f = (OpenFile *) currentThread->space->fileTable.Remove(fd);

  if ( f ) {
    delete f;
  } else {
    printf("%s","Tried to close an unopen file\n");
  }
}

void Yield_Syscall() {
  currentThread->Yield();
}

int CreateLock_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];  // Kernel buffer to put the name in

    if (!buf) return -1;

    if( copyin(vaddr,len,buf) == -1 ) {
      printf("%s","Bad pointer passed to CreateLock\n");
      delete buf;
      return -1;
    }

    buf[len]='\0';
    lockTableLock->Acquire();
    int index = lockMap.Find();
    if(index != -1) {
      lockTable[index].lock = new Lock(buf);
      lockTable[index].owner = currentThread->space;
      lockTable[index].isToBeDeleted = false;
      lockTable[index].waitingThreads = 0;
    }
    else {
      printf("No empty locks in lockTable\n");
    }
    lockTableLock->Release();
    //delete buf;
    return index;
}

void DestroyLock_Syscall(int index){
	//Check lock index is a valid number
   if(index < 0 || index >= numLocks){
    printf("Trying to destroy invalid lock index %d\n", index);
    return;
  }

  lockTableLock->Acquire();
	//Check that the lock is assigned in the bitmap
  if(!lockMap.Test(index)){
    printf("Trying to destroy a lock that is not assigned to any process. %d\n", index);
    lockTableLock->Release();
    return;
  }
	//Check lock is of this process
  if(lockTable[index].owner != currentThread->space){
    printf("Trying to destroy lock. Current process is not owner of lock %d\n", index);
    lockTableLock->Release();
    return;
  }

	// if lock has owner, mark it to be deleted
	// lock is not being held, remove it
  if(lockTable[index].lock->isHeld() || lockTable[index].waitingThreads > 0){
    lockTable[index].isToBeDeleted = true;
  }else{ 
    delete lockTable[index].lock;
    lockTable[index].lock = NULL;
    lockTable[index].owner = NULL;
    lockTable[index].isToBeDeleted = false;
    lockMap.Clear(index);
  }

  lockTableLock->Release();
}

void DestroyCondition_Syscall(int index){
  //Check condition index is a valid number
  if(index < 0 || index >= numConditions){
    printf("Trying to destroy invalid condition index %d\n", index);
    return;
  }

  lockTableLock->Acquire();
  //Check that the condition is assigned in the bitmap
  if(!conditionMap.Test(index)){
    printf("Trying to destroy a condition that is not assigned to any process. %d\n", index);
    lockTableLock->Release();
    return;
  }
  //Check condition is of this process
  if(conditionTable[index].owner != currentThread->space){
    printf("Trying to destroy condition. Current process is not owner of condition %d\n", index);
    lockTableLock->Release();
    return;
  }

  // if condition has owner, mark it to be deleted
  // condition is not being held, remove it
  if(conditionTable[index].waitingThreads > 0){
    conditionTable[index].isToBeDeleted = true;
  }else{ 
    delete conditionTable[index].condition;
    conditionTable[index].condition = NULL;
    conditionTable[index].owner = NULL;
    conditionTable[index].isToBeDeleted = false;
    conditionMap.Clear(index);
  }

  lockTableLock->Release();
}

void Exit_Syscall(int status) {
  DEBUG('e', "Exit status: %d\n", status);
  KernelProcess *kp;
  kp = (KernelProcess*)processTable.Get(currentThread->space->tableIndex);
  processTableLock->Acquire();
  kp->numThreads--;
  kp->numRunningThreads--;
  if(kp->numRunningThreads == 0) {
    DEBUG('a', "ending process");
    numProcesses--;
    for(int i = 0; i < numLocks; i++) {
      if(lockMap.Test(i)) {
        if(lockTable[i].owner == currentThread->space) {
          DestroyLock_Syscall(i);
        }
      }
    }
    for(int i= 0; i < numConditions; i++) {
      if(conditionMap.Test(i)) {
        if(conditionTable[i].owner == currentThread->space) {
          DestroyCondition_Syscall(i);
        }
      }
    }
    processTable.Remove(currentThread->space->tableIndex);
    delete currentThread->space;
    processTableLock->Release();
    if(numProcesses == 0) {
      interrupt->Halt();
    }
  }
  else{   //Thread called exit but is not the last exitting thread in the process
    currentThread->space->DeallocateStack(currentThread->stackId);
    processTableLock->Release();
  }
    // IntStatus oldLevel = interrupt->SetLevel(IntOff);
    // for(int i = 0; i < 4; i++){
    //     if(machine->tlb[i].dirty){
    //         ipt[machine->tlb[i].physicalPage].dirty = true;
    //     }
    //     machine->tlb[i].valid = FALSE;
    // }
    // interrupt->SetLevel(oldLevel);
  currentThread->Finish();
}

int Acquire_Syscall(int index){
  IntStatus oldLevel = interrupt->SetLevel(IntOff);
  lockTableLock->Acquire();
  DEBUG('a', "Acquired lock table lock.\n");
  if(index < 0 || index >= numLocks){
    printf("Trying to acquire invalid lock index %d\n", index);
    (void) interrupt->SetLevel(oldLevel);
    lockTableLock->Release();
    return -1;
  }

  if(!lockMap.Test(index)){
    printf("Trying to acquire a lock that is not currently assigned to a process. %d\n", index);
    (void) interrupt->SetLevel(oldLevel);
    lockTableLock->Release();
    return -1;
  }

	//Check lock is of this process
  if(lockTable[index].owner != currentThread->space){
    printf("Cannot acquire lock. Current process is not owner of lock %d\n", index);
    (void) interrupt->SetLevel(oldLevel);
    lockTableLock->Release();
    return -1;
  }

  if(lockTable[index].isToBeDeleted){
    printf("Cannot acquire lock. Lock is marked to be deleted %d\n", index);
    (void) interrupt->SetLevel(oldLevel);
    lockTableLock->Release();
    return -1;
  }
  lockTable[index].waitingThreads ++;
  lockTableLock->Release();
	//Might get context switched here
  
  ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads--;
  int result = lockTable[index].lock->Acquire();
  //Lock Acquire returns 0 if the thread did not go to sleep and 1 if it did go to sleep
  if(result == 0) {
    ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads++;
  }  


  lockTableLock->Acquire();
  lockTable[index].waitingThreads --;
  lockTableLock->Release();
  (void) interrupt->SetLevel(oldLevel);
  return index;
}

void Release_Syscall(int index){
  lockTableLock->Acquire();
  if(index < 0 || index >= numLocks){
    printf("Trying to release invalid lock index %d\n", index);
    lockTableLock->Release();
    return;
  }

  if(!lockMap.Test(index)){
    printf("Trying to release a lock that is not currently assigned to a process. %d\n", index);
    lockTableLock->Release();
    return;
  }

	//Check lock is of this process
  if(lockTable[index].owner != currentThread->space){
    printf("Cannot release lock. Current process is not owner of lock %d\n", index);
    lockTableLock->Release();
    return;
  }

  if(!lockTable[index].lock->isHeldByCurrentThread()){
    printf("Cannot release lock. Current thread is not holding the lock %d\n", index);
    lockTableLock->Release();
    return;
  }
  int result = lockTable[index].lock->Release();
  if(lockTable[index].isToBeDeleted && lockTable[index].waitingThreads == 0){
		//Delete lock
    delete lockTable[index].lock;
    lockTable[index].lock = NULL;
    lockTable[index].owner = NULL;
    lockTable[index].isToBeDeleted = false;
    lockMap.Clear(index);
  }

  lockTableLock->Release();

  if(result == 1){
    processTableLock->Acquire();
    ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads++;
    processTableLock->Release();
  }
}

int CreateCondition_Syscall(unsigned int vaddr, int len){
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len];  // Kernel buffer to put the name in

    if (!buf) return -1;

    if( copyin(vaddr,len,buf) == -1 ) {
      printf("%s","Bad pointer passed to CreateCondition\n");
      delete buf;
      return -1;
    }

    //buf[len]='\0';
    lockTableLock->Acquire();
    int index = conditionMap.Find();
    if(index != -1) {
      conditionTable[index].condition = new Condition(buf);
      conditionTable[index].owner = currentThread->space;
      conditionTable[index].isToBeDeleted = false;
      conditionTable[index].waitingThreads = 0;
    }
    else {
      printf("No empty locks in lockTable\n");
    }
    lockTableLock->Release();
    //delete buf;
    return index;
}

int Wait_Syscall(int index, int lock){
  DEBUG('a', "Waiting condition Index %d , lock %d", index, lock);
        DEBUG('a', "lock name is %s\n", lockTable[lock].lock->getName());

    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    DEBUG('a', "Acquired lockTableLock.\n");
    if(index < 0 || index >= numConditions){
      printf("Trying to wait for an invalid condition index %d\n", index);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }

    if(lock < 0 || lock >= numLocks){
      printf("Trying to wait for invalid condition lock %d\n", lock);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }

    if(!lockMap.Test(lock)){
      printf("Trying to wait for a condition with a lock that is not currently assigned to a process. %d\n", lock);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }

    if(!conditionMap.Test(index)){
      printf("Trying to wait for a condition that is not currently assigned to a process. %d\n", index);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }

    //Check condition is of this process
    if(conditionTable[index].owner != currentThread->space){
      printf("Cannot wait for a condition. Current process is not owner of condition %d\n", index);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }

    if(lockTable[lock].owner != currentThread->space){
      printf("Cannot wait for a condition. Current process is not owner of lock %d\n", index);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }

    if(conditionTable[index].isToBeDeleted){
      printf("Cannot wait for a condition. Condition is marked to be deleted %d\n", index);
      (void) interrupt->SetLevel(oldLevel);
      return -1;
    }
    conditionTable[index].waitingThreads ++;
    //lockTableLock->Release();
    //Might get context switched here

    ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads--;
    DEBUG('a', "Actually got to condition wait.\n");
    int result = conditionTable[index].condition->Wait(lockTable[lock].lock);
    if(result != 1){
      ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads++;
    }



    lockTableLock->Acquire();
    conditionTable[index].waitingThreads --;
    lockTableLock->Release();
    (void) interrupt->SetLevel(oldLevel);
    return index;
}

void Signal_Syscall(int index, int lock){
  DEBUG('a', "Signalling condition Index %d , lock %d", index, lock);
    lockTableLock->Acquire();
    if(lock < 0 || lock >= numLocks){
      printf("Trying to wait for invalid condition lock %d\n", lock);
      lockTableLock->Release();
      return;
    }

    if(!lockMap.Test(lock)){
      printf("Trying to wait for a condition with a lock that is not currently assigned to a process. %d\n", lock);
      lockTableLock->Release();
      return;
    }

    if(lockTable[lock].owner != currentThread->space){
      printf("Cannot wait for a condition. Current process is not owner of lock %d\n", index);
      lockTableLock->Release();
      return;
    }

    if(index < 0 || index >= numConditions){
      printf("Trying to signal an invalid condition index %d\n", index);
      lockTableLock->Release();
      return;
    }

    if(!conditionMap.Test(index)){
      printf("Trying to wait for a condition that is not currently assigned to a process. %d\n", index);
      lockTableLock->Release();
      return;
    }

    if(conditionTable[index].owner != currentThread->space){
      printf("Cannot wait for a condition. Current process is not owner of condition %d\n", index);
      lockTableLock->Release();
      return;
    }

    if(conditionTable[index].isToBeDeleted){
      printf("Cannot wait for a condition. Condition is marked to be deleted %d\n", index);
      lockTableLock->Release();
      return;
    }
    DEBUG('a', lockTable[lock].lock->getName());
    int result = conditionTable[index].condition->Signal(lockTable[lock].lock);

    if(conditionTable[index].isToBeDeleted && conditionTable[index].waitingThreads == 0){
    //Delete condition
      delete conditionTable[index].condition;
      conditionTable[index].condition = NULL;
      conditionTable[index].owner = NULL;
      conditionTable[index].isToBeDeleted = false;
      conditionMap.Clear(index);
    }
    lockTableLock->Release();

    if(result == 1){
      processTableLock->Acquire();
      ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads++;
      processTableLock->Release();
    }
}

void Broadcast_Syscall(int index, int lock){
    lockTableLock->Acquire();
    if(lock < 0 || lock >= numLocks){
      printf("Trying to wait for invalid condition lock %d\n", lock);
      return;
    }

    if(!lockMap.Test(lock)){
      printf("Trying to wait for a condition with a lock that is not currently assigned to a process. %d\n", lock);
      return;
    }

    if(lockTable[lock].owner != currentThread->space){
      printf("Cannot wait for a condition. Current process is not owner of lock %d\n", index);
      return;
    }

    if(index < 0 || index >= numConditions){
      printf("Trying to signal an invalid condition index %d\n", index);
      return;
    }

    if(!conditionMap.Test(index)){
      printf("Trying to wait for a condition that is not currently assigned to a process. %d\n", index);
      return;
    }

    if(conditionTable[index].owner != currentThread->space){
      printf("Cannot wait for a condition. Current process is not owner of condition %d\n", index);
      return;
    }

    if(conditionTable[index].isToBeDeleted){
      printf("Cannot wait for a condition. Condition is marked to be deleted %d\n", index);
      return;
    }

    int result = conditionTable[index].condition->Broadcast(lockTable[lock].lock);
    if(conditionTable[index].isToBeDeleted && conditionTable[index].waitingThreads == 0){
    //Delete condition
      delete conditionTable[index].condition;
      conditionTable[index].condition = NULL;
      conditionTable[index].owner = NULL;
      conditionTable[index].isToBeDeleted = false;
      conditionMap.Clear(index);
    }
    lockTableLock->Release();

    if(result > 0){
      processTableLock->Acquire();
      ((KernelProcess*)processTable.Get(currentThread->space->tableIndex))->numRunningThreads += result;
      processTableLock->Release();
    }
}

void PrintfInt_Syscall(unsigned int vaddr, int len, int id){
 char *buf;		// Kernel buffer for output


 if ( !(buf = new char[len]) ) {
    printf("%s","Error allocating kernel buffer for write!\n");
    return;
  } else {
    if ( copyin(vaddr,len,buf) == -1 ) {
     printf("%s","Bad pointer passed to to write: data not written\n");
     delete[] buf;
     return;
   }
  }
  if(id >= 100000) {
    printf(buf, id/100000 -1, (id%100000)/1000 -1, id % 1000);
  }else if(id >= 1000){
    printf(buf, id/1000 - 1, id%1000);
  }else{
    printf(buf, id);
  }
delete[] buf;
}

void PrintLargeInt_Syscall(unsigned int vaddr, int len, int id) {
  char *buf;   // Kernel buffer for output


 if ( !(buf = new char[len]) ) {
    printf("%s","Error allocating kernel buffer for write!\n");
    return;
  } else {
    if ( copyin(vaddr,len,buf) == -1 ) {
     printf("%s","Bad pointer passed to to write: data not written\n");
     delete[] buf;
     return;
   }
  }
  printf(buf, id);
  delete[] buf;
}


void kernelThread(int vAddr){
	currentThread->space->SaveState();
	//set registers
	machine->WriteRegister(PCReg, vAddr);
	machine->WriteRegister(NextPCReg, vAddr + 4);
   DEBUG('a', "--------------PC Register is %d\n", vAddr);
	currentThread->space->stackMapLock->Acquire();
	int stackID = currentThread->space->stackMap.Find();
	currentThread->space->stackMapLock->Release();

	if(stackID == -1){
		DEBUG('a', "Cannot Fork. Not enough available threads in current process.");
		Exit_Syscall(-1);
	}
  currentThread->stackId = stackID;
  currentThread->space->AllocateStack(stackID);

  DEBUG('a', "Stack id %d\n", stackID);
  machine->WriteRegister(StackReg, (PageSize * currentThread->space->numCodePages) + (stackID + 1)*UserStackSize - 16);
  DEBUG('a', "--------------Stack Register is %d\n", (PageSize * currentThread->space->numCodePages) + (stackID + 1)*UserStackSize - 16);
  currentThread->space->RestoreState();
  machine->Run();
}

void Fork_Syscall(int vAddr){
	Thread* t = new Thread("Kernel Thread");
  KernelProcess* kp = (KernelProcess*)processTable.Get(currentThread->space->tableIndex);
  processTableLock->Acquire();
  kp->numThreads++;
  kp->numRunningThreads++;
  processTableLock->Release();
  t->space = currentThread->space;
  t->Fork(kernelThread, vAddr);
}

void mainProcessThread(int n) {

  currentThread->space->InitRegisters();
  currentThread->space->RestoreState();  
  machine->Run();
}

int Exec_Syscall(unsigned int vaddr, int len){
  char *buf;   // Kernel buffer for output


  if ( !(buf = new char[len]) ) {
    DEBUG('a', "%s","Error allocating kernel buffer for write!\n");
    delete[] buf;
    return -1;
  } else {
    if ( copyin(vaddr,len,buf) == -1 ) {
      DEBUG('a', "%s","Bad pointer passed to to write: data not written\n");
      delete[] buf;
      return -1;
    }
  }

  AddrSpace *space = new AddrSpace(buf);
  delete[] buf;

  if (space == NULL) {
    DEBUG('a', "Unable to open executable file %s\n", buf);
    return -1;
  }

  Thread* mainThread = new Thread("Main Thread");
  mainThread->space = space;
  mainThread->Fork(mainProcessThread, 0);

  return 1;
}

int Rand_Syscall() {
  return rand();
}

int ReadInt_Syscall(int min, int max) {
  int userChoice;
  std::cin>>userChoice;
  if(std::cin.fail()){
    std::cin.clear();
      std::cin.ignore(80, '\n');
      userChoice = -1;
  }
  while(!(userChoice <= max && userChoice >= min)){
    printf("Not a valid input %d, min %d max %d\n", userChoice, min, max);
    if(std::cin.fail()){
      std::cin.clear();
          std::cin.ignore(80, '\n');
    }
    std::cin>>userChoice;
  }
  return userChoice;
}

int HandleFullMemory(){
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  int evictIndex;
  if(RAND_REPLACEMENT){
    evictIndex = rand()%NumPhysPages;
  }else{
    evictIndex = *((int*)pageQueue->Remove());
  }

  //Removing a page that could be in TLB
  for(int i = 0; i < 4; i++){
    if(machine->tlb[i].valid && machine->tlb[i].physicalPage == evictIndex){
      if(machine->tlb[i].dirty){
        ipt[evictIndex].dirty = TRUE;
      }
      machine->tlb[i].valid = FALSE;
    }
  }
  int vpn = ipt[evictIndex].virtualPage;
  //Update evited page's space's pagetable
  ipt[evictIndex].space->pageTable[vpn].inMemory = FALSE;
  DEBUG('c', "Evicting vpn: %d, ppn: %d \n", vpn, evictIndex);
  if(ipt[evictIndex].dirty || ipt[evictIndex].space->pageTable[vpn].dirty){

    //Need to write to swap file
    swapFileLock->Acquire();
    int swapIndex = swapFileMap.Find();
    if(swapIndex == -1){
      printf("ERROR swapfile map out of space");
      interrupt->SetLevel(oldLevel);
      swapFileLock->Release();
      return -1;
    }
    DEBUG('c', "Writing evicted page vpn: %d, ppn: %d, to swap file page %d \n", vpn, evictIndex, swapIndex);
    swapFile->WriteAt(&machine->mainMemory[evictIndex*PageSize], PageSize, swapIndex*PageSize);
    ipt[evictIndex].space->pageTable[vpn].dirty = TRUE;
    ipt[evictIndex].space->pageTable[vpn].onDisk = TRUE;
    ipt[evictIndex].space->pageTable[vpn].byteOffset = swapIndex*PageSize;


    swapFileLock->Release();
  }

  interrupt->SetLevel(oldLevel);
  return evictIndex;


}


int HandleIPTMiss(int vpn){
  if(vpn < 0 || vpn >= currentThread->space->numPages){
    printf("Virtual page requested is not within bounds. vpn: %d\n", vpn);
    return -1;
  }
  PTEntry* readEntry = &currentThread->space->pageTable[vpn];
  DEBUG('c', "IPT MISS - Virtual page number: %d Physical page number: %d StackID: %d\n", vpn, readEntry->physicalPage, currentThread->stackId);

  if(readEntry->valid == FALSE){
    printf("Page table entry is invalid. vpn: %d\n", vpn);
    return -1;
  }
  if(readEntry->inMemory){
        printf("Error: Page table entry is in memory but not in IPT. vpn: %d ppn: %d\n", vpn, readEntry->physicalPage);
        return readEntry->physicalPage;

  }
  //not in memory. need to read into memory

  memoryMapLock->Acquire();
  int index = memoryMap.Find();
  if(index == -1){
    //Not Enough space in memory
    index = HandleFullMemory();
  }
   readEntry = &currentThread->space->pageTable[vpn];

  if(readEntry->onDisk){
    //Read From Swap File
      DEBUG('c', "Reading from Swap File - Virtual page number: %d to ppn: %d Swap Page: %d \n", vpn,  index, readEntry->byteOffset/PageSize);

    swapFileLock->Acquire();
    int result = swapFile->ReadAt(&(machine->mainMemory[index*PageSize]), PageSize, readEntry->byteOffset);
    if(result == -1){
      printf("Error reading from swap file\n");
    }
    swapFileMap.Clear(readEntry->byteOffset/PageSize);

    swapFileLock->Release();

  }else if(readEntry->inExecutable){
    //Read from executable
    DEBUG('c', "Reading from Executable File - Virtual page number: %d to ppn: %d ByteOffset: %d \n", vpn,  index, readEntry->byteOffset);
    currentThread->space->executable->ReadAt(&(machine->mainMemory[index*PageSize]), PageSize, readEntry->byteOffset);

  }else{
    //Nothing to read
    bzero(&machine->mainMemory[index*PageSize], PageSize);

  }
  pageQueue->Append(&pageIndices[index]);
  readEntry->physicalPage = index;
  readEntry->inMemory = TRUE;
  readEntry->onDisk = FALSE;
  ipt[index].virtualPage = vpn;
  ipt[index].valid = TRUE;
  ipt[index].dirty = readEntry->dirty;
  ipt[index].space = currentThread->space;
  ipt[index].physicalPage = index; 
  memoryMapLock->Release();
  return index;
}

void HandlePageFault(int vaddr) {
  IntStatus oldLevel = interrupt->SetLevel(IntOff);

  int physPage = -1;
  int vpn = vaddr/PageSize;
  //DEBUG('c', "TLB MISS - Virtual page number: %d StackID: %d\n", vpn, currentThread->stackId);

  for(int i = 0; i < NumPhysPages; i++){
    if(ipt[i].space == currentThread->space && ipt[i].virtualPage == vpn && ipt[i].valid){
     physPage = i;
     break;
   }
  } 
  if(physPage == -1){
    physPage = HandleIPTMiss(vpn);
    if(physPage == -1){
      printf("Failed to load page vpn %d ppn %d stackId %d\n", vpn, physPage, currentThread->stackId);
      interrupt->Halt(); 
      return;
    }
  }
  IPTEntry* readEntry = &ipt[physPage];
 
  
  ipt[machine->tlb[currentTLBIndex].physicalPage].dirty = machine->tlb[currentTLBIndex].dirty;
  ipt[machine->tlb[currentTLBIndex].physicalPage].use = machine->tlb[currentTLBIndex].use;
  
  machine->tlb[currentTLBIndex].physicalPage = physPage;
  machine->tlb[currentTLBIndex].virtualPage = readEntry->virtualPage;
  machine->tlb[currentTLBIndex].valid = TRUE;
  machine->tlb[currentTLBIndex].readOnly = readEntry->readOnly;
  machine->tlb[currentTLBIndex].dirty = readEntry->dirty;
  machine->tlb[currentTLBIndex].use = readEntry->use;
  currentTLBIndex = (currentTLBIndex + 1)%4;
  interrupt->SetLevel(oldLevel);
}

int CreateLockRPC_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];  // Kernel buffer to put the name in
    char message[MaxMailSize];
    int index;
    if (!buf) return -1;

    if( copyin(vaddr,len,buf) == -1 ) {
      printf("%s","Bad pointer passed to CreateLock\n");
      delete buf;
      return -1;
    }

    buf[len]='\0';
    PacketHeader outPktHdr, inPktHdr;
    MailHeader outMailHdr, inMailHdr;
    strcpy(message, "CreateLock ");
    strcat(message, buf);
    outPktHdr.to = 0;   
    outMailHdr.to = 0;
    outMailHdr.from = 0;
    outMailHdr.length = strlen(message) + 1;
    bool success = postOffice->Send(outPktHdr, outMailHdr, message);
    if(!success) {
      printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
      interrupt->Halt();
    }
    postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
    fflush(stdout);
    std::stringstream ss;
    ss << message;
    ss >> message;
    if(!strcmp(message, "TableFull")){
      printf("%s\n", message);
      return -1;
    } 
    ss >> index;
    //delete buf;
    return index;
}

int AcquireRPC_Syscall(int index){
  std::stringstream ss;
  char message[MaxMailSize] ,lockNum[MaxMailSize];
  ss << index;
  ss >> lockNum;
  strcpy(message, "AcquireLock ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidLock")){
   printf("%s\n", message);
   return -1;
 }
  ms >> index;
  return index;
}

void ReleaseRPC_Syscall(int index){
  std::stringstream ss;
  char message[MaxMailSize] ,lockNum[MaxMailSize];
  ss << index;
  ss >> lockNum;
  strcpy(message, "ReleaseLock ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidLock")){
    printf("%s\n", message);
  }
}

void DestroyLockRPC_Syscall(int index){
  std::stringstream ss;
  char message[MaxMailSize] ,lockNum[MaxMailSize];
  ss << index;
  ss >> lockNum;
  strcpy(message, "DestroyLock ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidLock")){
    printf("InvalidLock\n");
  }
}

int CreateConditionRPC_Syscall(unsigned int vaddr, int len){
  char *buf = new char[len+1];  // Kernel buffer to put the name in
  char message[MaxMailSize];
  int index;
  if (!buf) return -1;

  if( copyin(vaddr,len,buf) == -1 ) {
    printf("%s","Bad pointer passed to CreateCondition\n");
    delete buf;
    return -1;
  }

  buf[len]='\0';
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  strcpy(message, "CreateCondition ");
  strcat(message, buf);
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ss;
  ss << message;
  ss >> message;
  if(!strcmp(message, "TableFull")){
   printf("%s\n", message);
   return -1;
 }
  ss >> index;
  //delete buf;
  return index;
}

int WaitRPC_Syscall(int index, int lock){
  std::stringstream ss, bs;
  char message[MaxMailSize] ,lockNum[MaxMailSize], conditionNum[MaxMailSize];
  ss << index;
  ss >> conditionNum;
  bs << lock;
  bs >> lockNum;
  strcpy(message, "WaitCondition ");
  strcat(message, conditionNum);
  strcat(message, " ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms, rs;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidLock")){
   printf("%s\n", message);
   return -1;
 }
  if(!strcmp(message, "InvalidCondition")){
    printf("%s\n", message);
    return -1;
  } 
  else if(!strcmp(call, "ReleasedLock")){
    printf("Released Lock%d\n", lock);
     postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
     fflush(stdout);
     rs << message;
     ms >> call;
     if(!strcmp(message, "InvalidLock")){
      printf("%s\n", message);
      return -1;
     } 
  }
  ms >> index;
  AcquireRPC_Syscall(lock);
  return index;
}

void SignalRPC_Syscall(int index, int lock){
  std::stringstream ss, bs;
  char message[MaxMailSize] ,lockNum[MaxMailSize], conditionNum[MaxMailSize];
  ss << index;
  ss >> conditionNum;
  bs << lock;
  bs >> lockNum;
  strcpy(message, "SignalCondition ");
  strcat(message, conditionNum);
  strcat(message, " ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidLock")){
   printf("%s\n", message);
   return;
 }
  if(!strcmp(message, "InvalidCondition")){
   printf("%s\n", message);
   return;
 }
  ms >> index;
  return;
}

void DestroyConditionRPC_Syscall(int index){
  std::stringstream ss;
  char message[MaxMailSize] ,lockNum[MaxMailSize];
  ss << index;
  ss >> lockNum;
  strcpy(message, "DestroyCondition ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidCondition")){
    printf("InvalidCondition\n");
  }
}

void BroadcastRPC_Syscall(int index, int lock){
  std::stringstream ss, bs;
  char message[MaxMailSize] ,lockNum[MaxMailSize], conditionNum[MaxMailSize];
  ss << index;
  ss >> conditionNum;
  bs << lock;
  bs >> lockNum;
  strcpy(message, "BroadcastCondition ");
  strcat(message, conditionNum);
  strcat(message, " ");
  strcat(message, lockNum);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  while(strcmp(call, "BroadcastedCondition") != 0){
    if(!strcmp(message, "InvalidLock")){
     printf("%s\n", message);
     return;
    }
    if(!strcmp(message, "InvalidCondition")){
     printf("%s\n", message);
     return;
   }
    postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
    fflush(stdout);
    std::stringstream cs;
    cs << message;
    cs >> call;
  }
  return;
}

int CreateMonitorRPC_Syscall(unsigned int vaddr, int len){
    char *buf = new char[len+1];  // Kernel buffer to put the name in
    char message[MaxMailSize];
    int index;
    if (!buf) return -1;

    if( copyin(vaddr,len,buf) == -1 ) {
      printf("%s","Bad pointer passed to CreateMonitor\n");
      delete buf;
      return -1;
    }

    buf[len]='\0';
    PacketHeader outPktHdr, inPktHdr;
    MailHeader outMailHdr, inMailHdr;
    strcpy(message, "CreateMonitor ");
    strcat(message, buf);
    outPktHdr.to = 0;   
    outMailHdr.to = 0;
    outMailHdr.from = 0;
    outMailHdr.length = strlen(message) + 1;
    bool success = postOffice->Send(outPktHdr, outMailHdr, message);
    if(!success) {
      printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
      interrupt->Halt();
    }
    postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
    fflush(stdout);
    std::stringstream ss;
    ss << message;
    ss >> message;
    if(!strcmp(message, "TableFull")){
      printf("%s\n", message);
      return -1;
    } 
    ss >> index;
    //delete buf;
    return index;
}

void SetMonitorRPC_Syscall(int index, int value){
  std::stringstream ss, bs;
  char message[MaxMailSize], mValue[MaxMailSize], monitorNum[MaxMailSize];
  ss << index; 
  ss >> monitorNum;
  bs << value;
  bs >> mValue;
  strcpy(message, "SetMonitor ");
  strcat(message, monitorNum);
  strcat(message, " ");
  strcat(message, mValue);
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidMonitor")){
   printf("%s\n", message);
   return;
 }
  return;
}

int GetMonitorRPC_Syscall(int index){
  std::stringstream ss;
  char message[MaxMailSize], monitorNum[MaxMailSize];
  ss << index; 
  ss >> monitorNum;
  strcpy(message, "GetMonitor ");
  strcat(message, monitorNum); 
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  printf("%s\n", message);
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidMonitor")){
   printf("%s\n", message);
   return -1;
 }
  else if(!strcmp(call, "GotMonitor")){
    ms >> index;
    return index;
  }
  return -1;
}

void DestroyMonitorRPC_Syscall(int index){
  std::stringstream ss;
  char message[MaxMailSize], monitorNum[MaxMailSize];
  ss << index; 
  ss >> monitorNum;
  strcpy(message, "DestroyMonitor ");
  strcat(message, monitorNum); 
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(message) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, message);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
    interrupt->Halt();
  }
  postOffice->Receive(0, &inPktHdr, &inMailHdr, message);
  fflush(stdout);
  std::stringstream ms;
  ms << message;
  printf("%s\n", message);
  char call[MaxMailSize];
  ms >> call;
  if(!strcmp(message, "InvalidMonitor")){
   printf("%s\n", message);
   return;
  }
  return; 
}

void SendMessage_Syscall(unsigned int vaddr, int len) {
  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char *buf = new char[len];  // Kernel buffer to put the name in

    if (!buf) return;

    if( copyin(vaddr,len,buf) == -1 ) {
      printf("%s","Bad pointer passed to SendMessage\n");
      delete buf;
      return;
    }
  outPktHdr.to = 0;   
  outMailHdr.to = 0;
  outMailHdr.from = 0;
  outMailHdr.length = strlen(buf) + 1;
  bool success = postOffice->Send(outPktHdr, outMailHdr, buf);
  if(!success) {
    printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
      interrupt->Halt();
  }
  else{
    printf("successfully sent message [%s] to server", buf);
  }
}

void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2); // Which syscall?
    int rv=0; 	// the return value from a syscall
    if(which == PageFaultException) {
      int vaddr = machine->ReadRegister(BadVAddrReg);
      HandlePageFault(vaddr);
    }
    else if ( which == SyscallException ) {
     switch (type) {
       default:
       DEBUG('a', "Unknown syscall - shutting down.\n");
       case SC_Halt:
       DEBUG('a', "Shutdown, initiated by user program.\n");
       interrupt->Halt();
       break;
       case SC_Create:
       DEBUG('a', "Create syscall.\n");
       Create_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_Open:
       DEBUG('a', "Open syscall.\n");
       rv = Open_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_Write:
       DEBUG('a', "Write syscall.\n");
       Write_Syscall(machine->ReadRegister(4),
         machine->ReadRegister(5),
         machine->ReadRegister(6));
       break;
       case SC_Read:
       DEBUG('a', "Read syscall.\n");
       rv = Read_Syscall(machine->ReadRegister(4),
         machine->ReadRegister(5),
         machine->ReadRegister(6));
       break;
       case SC_Close:
       DEBUG('a', "Close syscall.\n");
       Close_Syscall(machine->ReadRegister(4));
       break;
       case SC_Yield:
       DEBUG('a', "Yield syscall.\n");
       Yield_Syscall();
       break;
       case SC_Exit:
       DEBUG('a', "Exit syscall.\n");
       Exit_Syscall(machine->ReadRegister(4));
       break;
       case SC_CreateLock:
       DEBUG('a', "Create Lock syscall.\n");
       rv = CreateLock_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_DestroyLock:
       DEBUG('a', "Destroy Lock syscall.\n");
       DestroyLock_Syscall(machine->ReadRegister(4));
       break;
       case SC_Acquire:
       DEBUG('a', "Acquire Lock syscall.\n");
       rv = Acquire_Syscall(machine->ReadRegister(4));
       break;
       case SC_Release:
       DEBUG('a', "Release Lock syscall.\n");
       Release_Syscall(machine->ReadRegister(4));
       break;
       case SC_CreateCondition:
       DEBUG('a', "Create condition syscall.\n");
       rv = CreateCondition_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_DestroyCondition:
       DEBUG('a', "Destroy condition syscall.\n");
       DestroyCondition_Syscall(machine->ReadRegister(4));
       break;
       case SC_Wait:
       DEBUG('a', "Wait syscall.\n");
       rv = Wait_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_Signal:
       DEBUG('a', "Signal syscall.\n");
       Signal_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_Broadcast:
       DEBUG('a', "Broadcast syscall.\n");
       Broadcast_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_Fork:
       DEBUG('a', "Fork syscall.\n");
       Fork_Syscall(machine->ReadRegister(4));
       break;
       case SC_PrintfInt:
       DEBUG('a', "PrintfInt syscall.\n");
       PrintfInt_Syscall(machine->ReadRegister(4), machine->ReadRegister(5), machine->ReadRegister(6));
       break;
       case SC_Exec:
       DEBUG('a', "Exec syscall.\n");
       rv = Exec_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_Rand:
       DEBUG('a', "Rand syscall.\n");  
       rv = Rand_Syscall();
       break;
       case SC_ReadInt:
       DEBUG('a', "Read int syscall.\n");  
       rv = ReadInt_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_PrintLargeInt:
       DEBUG('a', "PrintLargeInt syscall.\n");
       PrintLargeInt_Syscall(machine->ReadRegister(4), machine->ReadRegister(5), machine->ReadRegister(6));
       break;
       case SC_SendMessage:
       DEBUG('a', "SendMessage syscall.\n");
       SendMessage_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_CreateLockRPC:
       DEBUG('a', "CreateLockRPC syscall.\n");
       rv = CreateLockRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_AcquireRPC:
       DEBUG('a', "AcquireRPC syscall.\n");
       rv = AcquireRPC_Syscall(machine->ReadRegister(4));
       break;
       case SC_ReleaseRPC:
       DEBUG('a', "ReleaseRPC syscall.\n");
       ReleaseRPC_Syscall(machine->ReadRegister(4));
       break;
       case SC_DestroyLockRPC:
       DEBUG('a', "DestroyLockRPC syscall.\n");
       DestroyLockRPC_Syscall(machine->ReadRegister(4));
       break;
       case SC_CreateConditionRPC:
       DEBUG('a', "CreateConditionRPC syscall.\n");
       rv = CreateConditionRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_WaitRPC:
       DEBUG('a', "WaitRPC syscall.\n");
       rv = WaitRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_SignalRPC:
       DEBUG('a', "SignalRPC syscall.\n");
       SignalRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_BroadcastRPC:
       DEBUG('a', "BroadcastRPC syscall.\n");
       BroadcastRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_DestroyConditionRPC:
       DEBUG('a', "DestroyConditionRPC syscall.\n");
       DestroyConditionRPC_Syscall(machine->ReadRegister(4));
       break;
       case SC_CreateMonitorRPC:
       DEBUG('a', "CreateMonitorRPC syscall.\n");
       rv = CreateMonitorRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_SetMonitorRPC:
       DEBUG('a', "SetMonitorRPC syscall.\n");
       SetMonitorRPC_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
       break;
       case SC_GetMonitorRPC:
       DEBUG('a', "GetMonitorRPC syscall.\n");
       rv = GetMonitorRPC_Syscall(machine->ReadRegister(4));
       break;
       case SC_DestroyMonitorRPC:
       DEBUG('a', "DestroyMonitorRPC syscall.\n");
       DestroyMonitorRPC_Syscall(machine->ReadRegister(4));
       break;
     }

	// Put in the return value and increment the PC
     machine->WriteRegister(2,rv);
     machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
     machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
     machine->WriteRegister(NextPCReg,machine->ReadRegister(PCReg)+4);
     return;
   } else {
    cout<<"Unexpected user mode exception - which:"<<which<<"  type:"<< type<<endl;
    interrupt->Halt();
  }
}
