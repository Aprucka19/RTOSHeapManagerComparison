// *************ADC.c**************
// EE445M/EE380L.6 Labs 1, 2, Lab 3, and Lab 4 
// mid-level ADC functions
// you are allowed to call functions in the low level ADCSWTrigger driver
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano Jan 5, 2020, valvano@mail.utexas.edu
#include <stdint.h>
#include "../inc/ADCSWTrigger.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/LaunchPad.h"
#include "../inc/LPF.h"
#include "../inc/PLL.h"
#include "../inc/IRDistance.h"

// channelNum (0 to 11) specifies which pin is sampled 
// return with error 1, if channelNum>11, 
// otherwise initialize ADC and return 0 (success)
int ADC_Init(uint32_t channelNum){
// put your Lab 1 code here
	if (channelNum > 11){
		return 1;
	}
	ADC0_InitSWTriggerSeq3(channelNum);
	//ADC0_InitSWTriggerSeq3_InternalTemperature();
  return 0;
}
// software start sequencer 3 and return 12 bit ADC result
uint32_t ADC_In(void){
// put your Lab 1 code here
	
  return ADC0_InSeq3();
	//DisableInterrupts();
	//uint32_t adcval = ADC0_InSeq3_InternalTemperature();
	//EnableInterrupts();
	//return adcval;
}



//static int32_t ADCdata,FilterOutput,Distance;
//static uint32_t FilterWork;

//// periodic task HW Timer ADC
//void DAStaskHW(uint32_t data){  // runs at 10Hz in background
//  ADCdata = data;  // channel set when calling ADC_Init
//  FilterOutput = Median(ADCdata); // 3-wide median filter
//  Distance = IRDistance_Convert(FilterOutput,0);
//  FilterWork++;        // calculation finished
//}


////Initializes the periodic ADC recording task with a hardware timer
////Inputs: channel num (0-11)
////				fs (sampling frequency in hz)
////
//int ADC_InitHW(uint32_t channelNum, uint32_t fs){
//	if (channelNum > 11){
//		return 1;
//	}
//	ADC0_InitTimer0ATriggerSeq0(channelNum,fs, &DAStaskHW);
//  return 0;	
//	
//}

////Returns the last recorded ADC value
////New values are sampled every x seconds as 
////dicated by fs in ADC_InitHW
//uint32_t ADC_InHW(void){
//	return ADCdata;
//}