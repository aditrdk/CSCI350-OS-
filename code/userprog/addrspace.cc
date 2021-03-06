// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include "synch.h"

extern "C" { int bzero(char *, int); };

Table::Table(int s) : map(s), table(0), lock(0), size(s) {
    table = new void *[size];
    lock = new Lock("TableLock");
}
Table::~Table() {
    if (table) {
       delete table;
       table = 0;
   }
   if (lock) {
       delete lock;
       lock = 0;
   }
}

void *Table::Get(int i) {
    // Return the element associated with the given if, or 0 if
    // there is none.

    return (i >=0 && i < size && map.Test(i)) ? table[i] : 0;
}

int Table::Put(void *f) {
    // Put the element in the table and return the slot it used.  Use a
    // lock so 2 files don't get the same space.
    int i;	// to find the next slot

    lock->Acquire();
    i = map.Find();
    lock->Release();
    if ( i != -1)
       table[i] = f;
   return i; 
}

void *Table::Remove(int i) {
    // Remove the element associated with identifier i from the table,
    // and return it.

    void *f =0;

    if ( i >= 0 && i < size ) {
       lock->Acquire();
       if ( map.Test(i) ) {
           map.Clear(i);
           f = table[i];
           table[i] = 0;
       }
       lock->Release();
   }
   return f;
}

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	"executable" is the file containing the object code to load into memory
//
//      It's possible to fail to fully construct the address space for
//      several reasons, including being unable to allocate memory,
//      and being unable to read key parts of the executable.
//      Incompletely consretucted address spaces have the member
//      constructed set to false.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(char* filename) : fileTable(MaxOpenFiles), stackMap(numStacks) {

    executable = fileSystem->Open(filename);
    if (executable == NULL) {
        DEBUG('a', "Unable to open execuatble file %s\n", filename);
        return;
    }

    NoffHeader noffH;
    unsigned int i, size;
    KernelProcess *kp;
    stackMapLock = new Lock("Stack map lock");
    pageTableLock = new Lock("Page table lock");

    // Don't allocate the input or output to disk files
    fileTable.Put(0);
    fileTable.Put(0);

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
      (WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size ;

    numCodePages = divRoundUp(size, PageSize) ;
    numStackPages = divRoundUp(UserStackSize*numStacks,PageSize);
                                                // we need to increase the size
						// to leave room for the stack
    numPages = numCodePages+numStackPages;

    size = (numPages) * PageSize;

    //ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('c', "Initializing address space, num total pages %d, size %d\n", 
       numPages, size);
// first, set up the translation 
    pageTableLock->Acquire();
    pageTable = new PTEntry[numPages];
    for (i = 0; i < numPages; i++) {
    	pageTable[i].virtualPage = i;	
        if(i < numCodePages) {
            pageTable[i].inExecutable = TRUE;
            pageTable[i].byteOffset = i*PageSize + noffH.code.inFileAddr;

        }
        else{

            pageTable[i].inExecutable = FALSE;


        }

        if(i < numCodePages + 8){
            pageTable[i].valid = TRUE;

        }else{
            pageTable[i].valid = FALSE;
        }
        pageTable[i].onDisk = FALSE;        
        pageTable[i].use = FALSE;
        pageTable[i].inMemory = FALSE;
        pageTable[i].dirty = FALSE;
    	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
    					// a separate page, we could set its 
    					// pages to be read-only
    }

    //Add the new process to the process table
    processTableLock->Acquire();
    numProcesses++;
    processTableLock->Release();
    kp = new KernelProcess();
    kp->numThreads = 1;
    kp->numRunningThreads = 1;
    kp->pSpace = this;
    tableIndex = processTable.Put(kp);
    if(tableIndex == -1) {
        printf("Cannot have more than %d processes\n", maxProcesses);
        ASSERT(FALSE);
    }
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
//  bzero(machine->mainMemory, size);

// then, copy in the code and data segments into memory
    /*
    int initPages = divRoundUp(noffH.code.size + noffH.initData.size, PageSize);
    for(int j = 0; j < initPages; j++) {
        DEBUG('c', "Initializing code/initdata page, at 0x%x, size %d\n", 
            pageTable[j].physicalPage * PageSize, PageSize);
        if(j == initPages -1) {
            executable->ReadAt(&(machine->mainMemory[pageTable[j].physicalPage * PageSize]),
                noffH.code.size + noffH.initData.size - j*PageSize, noffH.code.inFileAddr + j*PageSize);
        }
        else{
            executable->ReadAt(&(machine->mainMemory[pageTable[j].physicalPage * PageSize]),
                PageSize, noffH.code.inFileAddr + j*PageSize);
        }
    }
*/
    pageTableLock->Release();

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
//
// 	Dealloate an address space.  release pages, page tables, files
// 	and file tables
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    for(int i = 0; i < numCodePages + numStackPages; i++) {
        if(pageTable[i].valid == TRUE){
            DeallocatePage(i);
        }
    }
    delete executable;
    delete pageTable;
    delete stackMapLock;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    stackMapLock->Acquire();
    currentThread->stackId = stackMap.Find();  //Finding space for the the first stack
    stackMapLock->Release();
    int i;

    for (i = 0; i < NumTotalRegs; i++)
       machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
   machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
   machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
   machine->WriteRegister(StackReg, (numCodePages+8) * PageSize - 16);
   DEBUG('a', "Initializing stack register to %d\n", (numCodePages+8) * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    for(int i = 0; i < 4; i++){
        if(machine->tlb[i].valid && machine->tlb[i].dirty){
            ipt[machine->tlb[i].physicalPage].dirty = TRUE;
        }
        machine->tlb[i].valid = FALSE;
    }
    interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    DEBUG('c', "Restoring State and clearing TLB stack ID: %d\n", currentThread->stackId);
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    for(int i = 0; i < 4; i++){
        if(machine->tlb[i].valid && machine->tlb[i].dirty){
            ipt[machine->tlb[i].physicalPage].dirty = TRUE;
        }
        machine->tlb[i].valid = FALSE;
    }
    interrupt->SetLevel(oldLevel);

    //machine->pageTable = pageTable;
    //machine->pageTableSize = numStackPages + numCodePages;
}

void AddrSpace::AllocateStack(int stackID) {
    DEBUG('c', "Allocating stack %d\n", stackID);
    // memoryMapLock->Acquire();
    for(int i = numCodePages + stackID*8; i < numCodePages + (stackID+1)*8; i++) {
        // int index = memoryMap.Find();
        // if(index == -1) {
        //     printf("Not enough physical memory for another thread stack\n");
        //     memoryMapLock->Release();
        //     ASSERT(false);
        // }
        pageTable[i].virtualPage = i;
        // pageTable[i].physicalPage = index;
        pageTable[i].valid = TRUE;
        pageTable[i].inMemory = FALSE;
        pageTable[i].onDisk = FALSE;
        pageTable[i].inExecutable = FALSE;
        // ipt[index].space = this;
        // ipt[index].virtualPage = i;
        // ipt[index].physicalPage = index;
        // ipt[index].valid = TRUE;
    }
        // memoryMapLock->Release();

}

void AddrSpace::DeallocateStack(int stackID) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    for(int i = numCodePages + stackID*8; i < numCodePages + (stackID+1)*8; i++) {
        DeallocatePage(i);
    }

    stackMapLock->Acquire();
    stackMap.Clear(stackID);
    stackMapLock->Release();
    interrupt->SetLevel(oldLevel);
}

void AddrSpace::DeallocatePage(int pageNo) {
    DEBUG('c', "Deallocating virtual page %d in stackId %d \n", pageNo, currentThread->stackId );

    // bzero(&machine->mainMemory[index*PageSize], PageSize);
    memoryMapLock->Acquire();
    pageTable[pageNo].valid = FALSE;
    for(int i = 0; i < NumPhysPages; i++){
        if(ipt[i].space == this && ipt[i].virtualPage == pageNo && ipt[i].valid){
            ipt[i].valid = FALSE;
            int index = pageTable[pageNo].physicalPage;
            memoryMap.Clear(index);
        }
    } 
    memoryMapLock->Release();
}
