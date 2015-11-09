#include "syscall.h"

void matmultLauncher() {
	Write("Launching matmult\n", sizeof("Launching matmult\n"), ConsoleOutput);
	/*Exec the thread test*/
	Exec("../test/matmult", sizeof("../test/threadsTest"));
	Exit(0);
}

int main() {	
	/*Fork two different threads*/
	Fork(matmultLauncher);
	Fork(matmultLauncher);
	Exit(0);
}