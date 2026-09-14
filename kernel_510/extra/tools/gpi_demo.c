#include<titan4_io.h>
#include<stdio.h>
#include<unistd.h>


  //gpi demo
int main()
{	
	int id =1;
	int value=0;
	while(1){
		id=1;
		for(id;id<=4;id++){
			value=gpi_get(id);
			printf("gpi%d:%d 	",id,value);
		}
		printf("\n");
		sleep(1);
	}
	return 0;
}

