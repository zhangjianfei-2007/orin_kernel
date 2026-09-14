/*
 *This lib suport TITAN4 to access hardware
 *It include dac_ctrl() (use to output Analogue voltage)
 *           adc_get()  (use to input Analogue voltage )
 *           gpo_ctrl() (use to output Digital level)
 *           gpi_get()  (use to input Digital level )
 *           led_ctrl() (use to control LED)
 *           get_key()  (use to check if system run on a correct hardware)
 * */
#ifndef TITAN4_IO_H
#define TITAN4_IO_H

/*func dac_ctrl()
 *use to output Analogue voltage
 *input:id(must be one of the 1/2/3/4/5/6 )	
 *	value(must be 0.0-12.0)
 *output: 1 success  -1 fail
 * */
int dac_ctrl(int id,float value);

/*func adc_get()
 *use to intput Analogue voltage
 *input:   id(must be one of the 1/2/3/4 )	
 *output:  value(0.0-12.0)  error -1
 * */
float adc_get(int id);


/*func gpo_ctrl()
 *use to output Digital level
 *input:id(must be one of the 1/2 )	
 *	value(must be 0(high) or 1(low) )
 *output: 1 success  -1 fail
 * */
int gpo_ctrl(int id,int value);


/*func adc_get()
 *use to intput Digital level
 *input:id(must be one of the 1/2/3 )	
 *output: 1 high 	0 low	-1 error
 * */
int gpi_get(int id);


/*func led_ctrl()
 *use to control LED
 *input:id(must be one of the 1/2/3/4 )	
 *	value(must be 0(off)/1(on)/2(flash)/3(EXplosion_flash )
 *output: 1 success  -1 fail
 * */
int led_ctrl(int id ,int value);

struct GPS_date{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int millisecond;
	int microsecond;
};
struct GPS_date get_date();



char * get_key();

#endif 
