#include "syscall.h"

int main() {
	SendMessage("Hey there server\n", sizeof("Hey there server\n"));
	Exit(0);
}