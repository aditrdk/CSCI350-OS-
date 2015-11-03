#include "syscall.h"

void threadsTestLauncher() {
	Write("Launching threadsTest\n", sizeof("Launching threadsTest\n"), ConsoleOutput);
	/*Exec the thread test*/
	Exec("../test/threadsTest", sizeof("../test/threadsTest"));
	Exit(0);
}

void yieldTestLauncher(){
	Write("Launching yieldTest\n", sizeof("Launching yieldTest\n"), ConsoleOutput);
	/*Exec the yield test*/
	Exec("../test/yieldTest", sizeof("../test/yieldTest"));
	Exit(0);
}

int main() {	
	/*Fork two different threads*/
	Fork(threadsTestLauncher);
	Fork(yieldTestLauncher);
	Exit(0);
}