#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define TRIG GPIO_NUM_11
#define ECHO GPIO_NUM_12

#define LOOP_DELAY_MS 1000

// too short or too long echo pulse widths -> out of range.
#define OUT_OF_RANGE_SHORT 116
#define OUT_OF_RANGE_LONG  23200

// ----- Global variables -----
esp_timer_handle_t oneshot_timer;           // One-shot timer handle
volatile uint64_t echo_pulse_time = 0;      // Echo pulse width in microseconds

// For capturing the rising edge time
static volatile int64_t echo_rise_time_us = 0;

// ISR for the trigger pulse: after 10us, pull TRIG low
void IRAM_ATTR oneshot_timer_handler(void* arg)
{
    gpio_set_level(TRIG, 0);
}

/*************************/
/* 3. Echo ISR goes here */
/*************************/
void IRAM_ATTR echo_isr_handler(void* arg)
{
    // Read the level to know which edge we are on 
    int level = gpio_get_level(ECHO);
    int64_t now_us = esp_timer_get_time(); // microseconds since boot

    if (level == 1) {
        // Rising edge: start timing
        echo_rise_time_us = now_us;
    } else {
        // Falling edge: stop timing and compute pulse width
        int64_t width = now_us - echo_rise_time_us;
        if (width < 0) width = 0; // safety

        echo_pulse_time = (uint64_t)width;
    }
}

// Initialize pins and timer
void hc_sr04_init(void)
{
    // Trigger is an output, initially 0
    gpio_reset_pin(TRIG);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG, 0);

    // Echo is input, floating, interrupt on both edges
    gpio_reset_pin(ECHO);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ECHO, GPIO_FLOATING);
    gpio_set_intr_type(ECHO, GPIO_INTR_ANYEDGE);

    // Install ISR service and add handler for ECHO pin
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ECHO, echo_isr_handler, NULL);

    // Create one-shot esp timer for trigger pulse width control
    const esp_timer_create_args_t oneshot_timer_args = {
        .callback = &oneshot_timer_handler,
        .name = "one-shot"
    };
    esp_timer_create(&oneshot_timer_args, &oneshot_timer);
}

/***************************/
/* 4. app_main() goes here */
/***************************/
void app_main(void)
{
    hc_sr04_init();

    while (1)
    {
        // a) Set Trigger high
        gpio_set_level(TRIG, 1);

        // b) Start one-shot timer for 10us (will set TRIG low in handler)
        esp_timer_start_once(oneshot_timer, 10);

        // c) Wait long enough for echo to return (must be >= 40-60ms between triggers)
        vTaskDelay(pdMS_TO_TICKS(60));

        // d) Calculate and print distance
        uint64_t t_us = echo_pulse_time;

        if (t_us < OUT_OF_RANGE_SHORT || t_us > OUT_OF_RANGE_LONG) {
            printf("Out of range (echo pulse = %llu us)\n", (unsigned long long)t_us);
        } else {
            float distance_cm = (float)t_us / 58.3f; // d = t / 58.3
            printf("Echo: %llu us  ->  Distance: %.2f cm\n",
                   (unsigned long long)t_us, distance_cm);
        }

        // e) Delay loop time to make printed results readable
        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}