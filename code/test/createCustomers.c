
int main(){
	int i;
	for(i = 0; i < 3; i++){
		Exec("../test/appclerk", sizeof("../test/appclerk"));
	}
	for(i = 0; i < 5; i++){
		Exec("../test/customer", sizeof("../test/customer"));
	}
	Exit(1);
}