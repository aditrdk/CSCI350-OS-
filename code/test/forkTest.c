#include "syscall.h"

int j = 0;

void forkThread(){
	PrintfInt("Forked fork thread%d!\n", sizeof("Forked fork thread%d!\n"), j++);
	/*Exec the printtest program to show that fork and exec work as expected*/
	Exec("../test/printTest", sizeof("../test/printTest"));
	Exit(-1);
}


int main(){
	int i;
	PrintfInt("Starting fork test %d!\n", sizeof("Starting fork test %d!\n"), 420);

	for(i = 0; i < 5; i++){
		Fork(forkThread);
	}

	Exit(1);
}