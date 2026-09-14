#include <titan4_io.h>
#include <stdio.h>
#include <unistd.h>


//dac  demo
int main()
{	
	int id=1;
	float value=3.3;
	while(1){

		id=1;
		value=3.3;
		for (id;id<=4;id++)
		{
			dac_ctrl(id,value);
			printf("dac%d:%fV  ",id,value);
		}
		printf("\n");
		sleep(2);

		id=1;
		value=0.0;
		for (id;id<=4;id++)
		{
			dac_ctrl(id,value);
			printf("dac%d:%fV  ",id,value);
		}
		printf("\n");
		sleep(2);
	}
	return 0;
}




