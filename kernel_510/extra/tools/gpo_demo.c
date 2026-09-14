#include<titan4_io.h>
#include<stdio.h>
#include<unistd.h>


 //gpo  demo
int main()
{	
	int id=1;
	int value=0;
	while(1){
		id=1;
		value=1;
		for(id;id<=2;id++){
			gpo_ctrl(id,value);
			printf("gpo%d:%d	",id,value);
		}
		printf("\n");	
		sleep(2);

		id=1;
		value=0;
		for(id;id<=2;id++){
			gpo_ctrl(id,value);
			printf("gpo%d:%d	",id,value);
		}
		printf("\n");	
		sleep(2);
	}
	return 0;
}
