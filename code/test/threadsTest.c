#include "syscall.h"

int j = 0;

void test_thread(){
	/*Printing this output shows that the fork worked*/
	PrintfInt("Forked test thread %d!\n", sizeof("Forked test thread %d!\n"), j++);
	Exit(-1);
}


int main(){
	int i;
	PrintfInt("Starting thread test %d!\n", sizeof("Starting thread test %d!\n"), 420);
	/*Fork 41 threads to demonstrate forking works*/
	for(i = 0; i < 41; i++){
		Fork(test_thread);
	}

	Exit(1);
}
