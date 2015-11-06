#ifndef IPTENTRY_H
#define IPTENTRY_H

#include "translate.h"
#include "addrspace.h"

class IPTEntry : public TranslationEntry {
public:
	AddrSpace* space;
};

#endif