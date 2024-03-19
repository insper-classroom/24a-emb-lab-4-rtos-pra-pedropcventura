/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

#define TRIG 27
#define ECHO 28

SemaphoreHandle_t xSemaphoreTrigger;
SemaphoreHandle_t xSemaphoreEcho;
QueueHandle_t xQueueEnd;
QueueHandle_t xQueueStart;
QueueHandle_t xQueueDistance;



// bool timer_0_callback(repeating_timer_t *rt) {
//     xSemaphoreGiveFromISR(xSemaphoreTrigger, 0);
//     return true; // keep repeating
// }


bool timer_0_callback(repeating_timer_t *rt) {
    xSemaphoreGiveFromISR(xSemaphoreTrigger, 0);
    return true; // keep repeating
}

void btn_callback(uint gpio, uint32_t events) {
    int end;
    int start;
    if (events == 0x4) {         // fall edge
        end = to_us_since_boot(get_absolute_time());
        xSemaphoreGiveFromISR(xSemaphoreEcho, 0);
        xQueueSendFromISR(xQueueEnd, &end, 0);

    }
    if (events == 0x8){
        start = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueStart, &start, 0);
    }
    
    
    
} 

void trigger_task(void *p){
    gpio_init(TRIG);
    gpio_set_dir(TRIG, GPIO_OUT);
    repeating_timer_t timer_0;
    if (!add_repeating_timer_ms(1000, 
                                timer_0_callback,
                                NULL, 
                                &timer_0)) {
        printf("Failed to add timer\n");
    }

    while(true){
        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(0)) == pdTRUE){
            gpio_put(TRIG, 1);
            vTaskDelay(pdMS_TO_TICKS(5));
            gpio_put(TRIG, 0);
            printf("1SEGUNDO\n");
        }
            
    }
}

void echo_task(void *p){
    gpio_init(ECHO);
    gpio_set_dir(ECHO, GPIO_IN);
    gpio_pull_up(ECHO);

      gpio_set_irq_enabled_with_callback(ECHO, 
                                     GPIO_IRQ_EDGE_RISE | 
                                     GPIO_IRQ_EDGE_FALL, 
                                     true,
                                     &btn_callback);

    int start;
    int end;
    int distance;
    while(1){
        if (xSemaphoreTake(xSemaphoreEcho, pdMS_TO_TICKS(0)) == pdTRUE){
            
            if (xQueueReceive(xQueueEnd, &end,  pdMS_TO_TICKS(1000)) && xQueueReceive(xQueueStart, &start,  pdMS_TO_TICKS(1000))){
                distance = (end-start)/58;
                //printf("distancia: %d\n", distance);
                xQueueSend(xQueueDistance, &distance, 0);
            } 
            
        }
        
    }
}

void oled_task(void *p){
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    int distance;
    char str[20];
    while(1){
        if (xQueueReceive(xQueueDistance, &distance,  pdMS_TO_TICKS(1000))){
            printf("d: %d cm\n", distance);
            sprintf(str, "Distance: %d", distance);
            gfx_draw_string(&disp, 0, 10, 1, str);
            gfx_draw_line(&disp, 15, 25, 15 + distance, 27);
            gfx_show(&disp);
            
            
        } else{
            gfx_init(&disp, 128, 32);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 2, "Falha");
            gfx_show(&disp);
        }
        gfx_clear_buffer(&disp);
    }
}

int main() {
    stdio_init_all();

    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xSemaphoreEcho = xSemaphoreCreateBinary();
    xQueueEnd = xQueueCreate(32, sizeof(int) );
    xQueueStart = xQueueCreate(32, sizeof(int) );
    xQueueDistance = xQueueCreate(32, sizeof(int));

    xTaskCreate(trigger_task, "trigger task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "echo task", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "oled task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
