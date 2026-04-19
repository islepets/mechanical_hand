#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <driver/ledc.h>

#define BUTTON_GPIO_1    GPIO_NUM_14
#define BUTTON_GPIO_2    GPIO_NUM_3
#define BUTTON_GPIO_3    GPIO_NUM_20

static int last_states[3] = {0, 0, 0};
static gpio_num_t button_pins[3] = {BUTTON_GPIO_1, BUTTON_GPIO_2, BUTTON_GPIO_3};

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

static void init_input_pins(){

    for(int i = 0; i < 3; i++){
        gpio_config_t io_conf = {
                .pin_bit_mask = (1ULL << button_pins[i]),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            gpio_config(&io_conf);
    }
}


void first_move(){
    static int up_first_alg_finger1[] = {178, 2, LEDC_CHANNEL_2};
    static int down_first_alg_finger1[] = {2, 178, LEDC_CHANNEL_2};
    static int up_first_alg_finger2[] = {178, 2, LEDC_CHANNEL_4};
    static int down_first_alg_finger2[] = {2, 178, LEDC_CHANNEL_4};


    xTaskCreate(move_finger, "up_first_alg_finger1", 2048, up_first_alg_finger1, 5, NULL);
    xTaskCreate(move_finger, "up_first_alg_finger2", 2048, up_first_alg_finger2, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(3000));
    xTaskCreate(move_finger, "down_first_alg_finger1", 2048, down_first_alg_finger1, 5, NULL);
    xTaskCreate(move_finger, "down_first_alg_finger2", 2048, down_first_alg_finger2, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
}


void second_move(){
    static int up_second_alg_finger1[] = {2, 178, LEDC_CHANNEL_1};
    static int down_second_alg_finger1[] = {178, 2, LEDC_CHANNEL_1};
    static int up_second_alg_finger2[] = {178, 2, LEDC_CHANNEL_0};
    static int down_second_alg_finger2[] = {2, 178, LEDC_CHANNEL_0};

    xTaskCreate(move_finger, "up_second_alg_finger1", 2048, up_second_alg_finger1, 5, NULL);
    xTaskCreate(move_finger, "up_second_alg_finger2", 2048, up_second_alg_finger2, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(3000));
    xTaskCreate(move_finger, "down_second_alg_finger1", 2048, down_second_alg_finger1, 5, NULL);
    xTaskCreate(move_finger, "down_second_alg_finger2", 2048, down_second_alg_finger2, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
}


void third_move(){
    static int up_third_alg_finger1[] = {178, 2, LEDC_CHANNEL_2};
    static int down_third_alg_finger1[] = {2, 178, LEDC_CHANNEL_2};
    static int up_third_alg_finger2[] = {178, 2, LEDC_CHANNEL_0};
    static int down_third_alg_finger2[] = {2, 178, LEDC_CHANNEL_0};

    xTaskCreate(move_finger, "up_third_alg_finger1", 2048, up_third_alg_finger1, 5, NULL);
    xTaskCreate(move_finger, "up_third_alg_finger2", 2048, up_third_alg_finger2, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(3000));
    xTaskCreate(move_finger, "down_third_alg_finger1", 2048, down_third_alg_finger1, 5, NULL);
    xTaskCreate(move_finger, "down_third_alg_finger2", 2048, down_third_alg_finger2, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
}


void app_main(void) {

    init_input_pins();
    config_timer();

    config_channel(LEDC_CHANNEL_0, GPIO_NUM_21, max_duty); // Мизинец
    config_channel(LEDC_CHANNEL_1, GPIO_NUM_12, min_duty); // Большой
    config_channel(LEDC_CHANNEL_2, GPIO_NUM_11, max_duty); // указательный
    config_channel(LEDC_CHANNEL_3, GPIO_NUM_13, max_duty); // безымянный
    config_channel(LEDC_CHANNEL_4, GPIO_NUM_46, max_duty); // средний

    
    int current_states[3];
    while (1) {
        for (int i = 0; i < 3; i++) {
            current_states[i] = gpio_get_level(button_pins[i]);

            if (current_states[i] == 0 && last_states[i] == 1) {
                switch(i){
                    case 0:
                    {
                        ESP_LOGI("MAIN", "Кнопка=%d", i + 1);
                        first_move();
                        break;
                    }
                    case 1:
                    {
                        ESP_LOGI("MAIN", "Кнопка=%d", i + 1);
                        second_move();
                        break;
                    }
                    case 2:
                    {
                        ESP_LOGI("MAIN", "Кнопка=%d", i + 1);
                        third_move();
                        break; 
                    }
                }
            }
            
            last_states[i] = current_states[i];
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}