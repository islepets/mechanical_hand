
#include <stdio.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

int min_duty = 26;      // минимальный импульс ШИМ = 0.5 мс
int max_duty = 128;     // максимальный импульс ШИМ = 2.0 мс
int max_angle = 178;    // максимальный угол поворота сервопривода

static int set_angle(int angle)
{
    if (angle < 0) angle = 0;
    if (angle > max_angle) angle = max_angle;
    return min_duty + (angle * (max_duty - min_duty) / max_angle);
}

static void config_timer(void)
{
     ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,  
        .freq_hz          = 50,              
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
}

static void config_channel(ledc_channel_t LEDC_CHANNEL, gpio_num_t GPIO_NUM, int duty)
{
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_NUM,
        .duty           = duty,                   
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

static void move_finger(void *params)
{   
    int *p = (int*)params;
    
    uint32_t start = p[0];
    uint32_t end = p[1];
    ledc_channel_t channel = p[2];

    int step = (start < end) ? 1 : -1;
    for(uint32_t angle = start; angle != end + step; angle += step)
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, set_angle(angle));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
    vTaskDelete(NULL);
}

void app_main(void)
{   
    config_timer();

    config_channel(LEDC_CHANNEL_0, GPIO_NUM_21, max_duty); // Мизинец
    config_channel(LEDC_CHANNEL_1, GPIO_NUM_12, min_duty); // Большой
    config_channel(LEDC_CHANNEL_2, GPIO_NUM_11, max_duty); // указательный
    config_channel(LEDC_CHANNEL_3, GPIO_NUM_13, max_duty); // безымянный
    config_channel(LEDC_CHANNEL_4, GPIO_NUM_46, max_duty); // средний

    // Разгибание
    static int params1[] = {2, 178, LEDC_CHANNEL_1};
    static int params2[] = {178, 2, LEDC_CHANNEL_2};
    static int params3[] = {178, 2, LEDC_CHANNEL_4};

    // Сгибание
    static int params4[] = {178, 2, LEDC_CHANNEL_1};
    static int params5[] = {2, 178, LEDC_CHANNEL_4};
    static int params6[] = {2, 178, LEDC_CHANNEL_2};

    while(1) {
        
        xTaskCreate(move_finger, "finger1", 2048, params1, 5, NULL);
        xTaskCreate(move_finger, "finger2", 2048, params2, 5, NULL);
        xTaskCreate(move_finger, "finger3", 2048, params3, 5, NULL);

        vTaskDelay(pdMS_TO_TICKS(10000));
        
        xTaskCreate(move_finger, "finger4", 2048, params4, 5, NULL);
        xTaskCreate(move_finger, "finger5", 2048, params5, 5, NULL);
        xTaskCreate(move_finger, "finger6", 2048, params6, 5, NULL);

        
        vTaskDelay(pdMS_TO_TICKS(10000));
       
    }
}