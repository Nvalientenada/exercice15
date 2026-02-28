#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define LOOP_DELAY_MS 500       // Sampling time (ms)


void app_main(void)
{
    int64_t last_time = 0;
   
    while(1)
    {
        int64_t new_time = esp_timer_get_time();
        printf("Elapsed time: %8lld us \tSample time:  %8lld us\n",
            new_time, new_time - last_time);
        last_time = new_time;
        vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
    }  
}
