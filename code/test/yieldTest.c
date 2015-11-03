#include "syscall.h"

int main() {

	Write("Testing Yield\n", sizeof("Testing Yield\n"), ConsoleOutput );
	Yield();
	Exit(0);
}