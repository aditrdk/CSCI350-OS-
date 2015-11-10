#include "syscall.h"

int main() {
	int monitorNum, value;
	monitorNum = CreateMonitorRPC("TestMonitor", sizeof("TestMonitor"));
	PrintfInt("Monitor Number %d\n", sizeof("Monitor Number %d\n"), monitorNum);
	value = GetMonitorRPC(monitorNum);
	PrintfInt("Monitor Value %d\n", sizeof("Monitor Value %d\n"), value);
	DestroyMonitorRPC(monitorNum);
	Exit(0);
}