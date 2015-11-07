#ifndef PTENTRY_H
#define PTENTRY_H

#include "translate.h"
#include "addrspace.h"

class PTEntry : public TranslationEntry {
public:
	bool inMemory; //Whether the page is currently in main memory.
	bool onDisk;
	bool inExecutable;
	int byteOffset; // Byte offset in swap file if on disk or in the executable
};

#endif
