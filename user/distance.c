#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"


static inline unsigned get_count(void)
{
  unsigned r;
  asm volatile ("rsr %0, ccount" : "=r"(r));
  return r;
}

int get_distance()
{

  GPIO_OUTPUT_SET(12, 1);  
  volatile int pulse = get_count() + 800;  
  while(pulse > get_count());
  GPIO_OUTPUT_SET(12, 0);

  int i = 10000;
  
  while (i-->0)
  {	
    if(GPIO_INPUT_GET(13))
     {
	break;
     }
  }

  int start = get_count();

  while (i-->0)
  {
     if(!GPIO_INPUT_GET(13))
     {
	break;
     }
  }

  int stop = get_count();	

  int distance = ((stop-start)*34)/1600;
  return distance;
  //os_printf("distance %d\n", distance);

}


