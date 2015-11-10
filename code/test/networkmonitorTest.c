#include "syscall.h"

int main() {
	int monitorNum;
	monitorNum = CreateMonitorRPC("TestMonitor", sizeof("TestMonitor"));
	PrintfInt("Monitor Number %d\n", sizeof("Monitor Number %d\n"), monitorNum);
	SetMonitorRPC(monitorNum, 420);
	Exit(0);
}