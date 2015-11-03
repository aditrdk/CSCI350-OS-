#include "syscall.h"

int main() {
	Exec("../test/passportoffice", sizeof("../test/passportoffice"));
	Exec("../test/passportoffice", sizeof("../test/passportoffice"));
	Exit(0);
}