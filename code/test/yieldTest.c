#include "syscall.h"

int main() {

	Write("Testing Yield", sizeof("Testing Yield"), ConsoleOutput );
	Yield();
	Exit(0);
}