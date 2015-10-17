#include "syscall.h"

int j = 0;

void test_thread(){
	PrintfInt("Forked %d!\n", sizeof("Forked %d!\n"), j++);
	Exit(-1);
}


int main(){
	int i;
	PrintfInt("Starting thread test %d!\n", sizeof("Starting thread test %d!\n"), 420);

	for(i = 0; i < 41; i++){
		Fork(test_thread);
	}

	Exit(1);
	return 0;

}
