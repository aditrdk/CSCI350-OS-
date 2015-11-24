#include "syscall.h"
#include "passport.h"

int main(){
	if(setup()){
		Clerk* ac = &appClerks[0];
		SetMonitorRPC(ac->numLine, 420);
	}
}