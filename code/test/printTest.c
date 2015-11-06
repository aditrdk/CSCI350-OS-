#include "syscall.h"

int main() {
	Write("Exec'd printTest\n", sizeof("Exec'd printTest\n"), ConsoleOutput);
	Exit(0);
}