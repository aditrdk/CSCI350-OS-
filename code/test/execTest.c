#include "syscall.h"

int main() {
	Exec("../test/threadsTest", sizeof("../test/threadsTest"));
	Exec("../test/yieldTest", sizeof("../test/yieldTest"));
	Exit(0);
}