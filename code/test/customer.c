#include "syscall.h"
#include "passport.h"

int main(){
	int val;
	if(setup()){
		Clerk* ac = &appClerks[0];
		val = GetMonitorRPC(ac->numLine);
		PrintfInt("%d Blaze it faggits\n", sizeof("%d Blaze it faggits\n"), val);
	}
}