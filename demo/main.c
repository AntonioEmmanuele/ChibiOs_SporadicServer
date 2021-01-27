#include "ch.h"
#include "hal.h"
#include "chprintf.h"

void BW(void* );
static SerialConfig my_serial;
static BaseSequentialStream* bsp;
static thread_reference_t t;
static uint32_t exec=0;
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("Observer");
  uint32_t consumed_arr[10]={0,0,0};
  uint8_t num_element;
  uint32_t num=0;
  while (true) {
    chSporadicServerGetTime(&num_element,consumed_arr,&num);
    chprintf(bsp,"Elements in the array %u \n\r",num_element);
    chprintf(bsp,"Reservation called %u \n\r",num);
    chprintf(bsp,"Aperiodic Req executed %lu \n \r",exec);
    chprintf(bsp,"Consumed times :\n \r");
    for(uint8_t i=0;i<num_element;i++)
      chprintf(bsp,"%lu \n\r",TIME_I2MS(consumed_arr[i]));
    //chprintf(bsp,"Last capacity %lu \n \r",TIME_I2MS(t->capacity));
    chThdSleepMilliseconds(10);
  }
}
int main(void){
  halInit();
  chSysInit();
  my_serial.speed=115200;
  my_serial.cr1=0;
  my_serial.cr2=USART_CR2_STOP1_BITS;
  my_serial.cr3=0;
  sdStart(&SD2, &my_serial);
  bsp=(BaseSequentialStream*)&SD2;
  t=chSporadicServerObjectInit(TIME_MS2I(1000),TIME_MS2I(500),NORMALPRIO+2);
  AperiodicRequest k,k1,k2;
  bool first=true;
  uint16_t time_towt=(100);
  /*
   * If it is the first initialization creates the sporadic server requests, then inserts requests in the queue again if the sporadic server when the button is pressed
   */
  while(true){
    if(first){
      chSporadicServerCreateAperiodic(BW,(void*)&time_towt,&k);
      chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO+1, Thread1, NULL);
      chSporadicServerCreateAperiodic(BW,(void*)&time_towt,&k1);
      chSporadicServerCreateAperiodic(BW,(void*)&time_towt,&k2);
      first=false;
    }
   //chprintf(bsp,"Sporadic time %lu, sporadic capacity %lu, sporadic last TA %lu ,sporadic time to replinish %lu, and exec %lu \n \r",TIME_I2MS(t->consumed_time),TIME_I2MS(t->capacity),TIME_I2MS(t->TA),TIME_I2MS(t->timeToReplinish),exec);
   // chprintf(bsp,"Last rep %lu \n\r ",TIME_I2MS(chSporadicServerGetLastReplinishment()));
    if (!palReadPad(GPIOC, GPIOC_BUTTON)) {
          chprintf(bsp,"Button pressed \n \r");
          chSporadicServerAperiodicQueueInsert(&k);
          chSporadicServerAperiodicQueueInsert(&k1);
          chSporadicServerAperiodicQueueInsert(&k2);
    }
    chThdSleepMilliseconds(100);
  }

}

void BW(void* k){
  palSetPad(GPIOA,GPIOA_LED_GREEN);
  uint16_t sample=10;
  uint16_t time=*(uint16_t*)k;
  bool check=false;
  systime_t now=chVTGetSystemTime();
  systime_t now1;
  for(uint16_t i=0;i<=time/sample;i++){
    check=false;
    now=chVTGetSystemTime();
    while(!check){ //500ms
        now1=chVTGetSystemTime();
        if(chTimeI2MS(chTimeDiffX(now,now1))>=sample)
          check=true;
      }
  }
  palClearPad(GPIOA,GPIOA_LED_GREEN);
  exec++;
}


