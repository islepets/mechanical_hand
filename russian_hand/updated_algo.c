#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <driver/ledc.h>
#include <string.h> 

#define BUTTON_GPIO_1    GPIO_NUM_14
#define BUTTON_GPIO_2    GPIO_NUM_3
#define BUTTON_GPIO_3    GPIO_NUM_20

#define MIN_DUTY 26        
#define MAX_DUTY 128       
#define MAX_ANGLE 178      

#define MOV_SPEED 10

typedef struct {
    ledc_channel_t channel;
    gpio_num_t gpio;
    uint32_t init_angle;     
    uint32_t current_angle;  
    uint32_t min_angle;      
    uint32_t max_angle;      
    const char* name;
} ServoConfig;

typedef enum {
    CMD_RESET_ALL,      
    CMD_NEXT_POSE,      
    CMD_PREV_POSE       
} CommandType;

typedef struct {
    CommandType type;
    int finger; 
    uint32_t target_angle;
    uint32_t duration_ms;
} ServoCommand;

typedef struct {
    uint32_t angles[5];
    const char* name;
} Pose;

// Статические переменные
static ServoConfig servos[5] = {
    [0] = {LEDC_CHANNEL_1, GPIO_NUM_12, 80, 80, 40, 80, "Большой"},      // Большой
    [1] = {LEDC_CHANNEL_2, GPIO_NUM_11, 5,  5,  5,  80, "Указательный"},  // Указательный
    [2] = {LEDC_CHANNEL_4, GPIO_NUM_46, 95, 95, 23, 95, "Средний"},       // Средний
    [3] = {LEDC_CHANNEL_3, GPIO_NUM_13, 105, 105, 30, 105, "Безымянный"}, // Безымянный
    [4] = {LEDC_CHANNEL_0, GPIO_NUM_21, 5,  5,  5,  78, "Мизинец"}        // Мизинец
};

// позы
// Порядок пальцев: [Большой][Указательный][Мизинец][Безымянный][Мизинец][Средний]
static Pose poses[] = {
    {{80, 5, 95, 105, 5}, "Все разогнуты"},    
    {{80, 5, 95, 30, 78}, "Жест 1 (Коза)"},
    {{40, 5, 95, 105, 78}, "Жест 2 (большой и мизинец согнуты)"},
    {{80, 80, 95, 105, 78}, "Жест 3 (указательный и мизинец согнуты)"},
    {{40, 80, 23, 30, 78}, "Кулак"},
    {{40, 5, 23, 30, 78}, "Указательный вверх"},
    {{40, 80, 23, 30, 5}, "Средний вверх"},
    {{40, 80, 23, 105, 78}, "Безымянный вверх"},
    {{40, 80, 95, 30, 78}, "Мизинец вверх"},
    {{80, 80, 23, 30, 78}, "Большой вверх"}
};

static int current_pose = 0;
static int total_poses = sizeof(poses) / sizeof(poses[0]);

static QueueHandle_t servo_queue;

static int last_states[3] = {0, 0, 0};
static gpio_num_t button_pins[3] = {BUTTON_GPIO_1, BUTTON_GPIO_2, BUTTON_GPIO_3};

static const char* TAG = "SERVO_MANAGER";

// Преобразование угла в duty cycle
static int angle_to_duty(int angle)
{
    if (angle < 0) angle = 0;
    if (angle > MAX_ANGLE) angle = MAX_ANGLE;
    return MIN_DUTY + (angle * (MAX_DUTY - MIN_DUTY) / MAX_ANGLE);
}

// Инициализация ШИМ таймера
static void config_timer(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,  
        .freq_hz          = 50,              
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
}

// Инициализация ШИМ канала
static void config_channel(ledc_channel_t channel, gpio_num_t gpio, int duty)
{
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = channel,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio,
        .duty           = duty,                   
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// Инициализация всех сервоприводов
static void init_all_servos(void)
{
    for (int i = 0; i < 5; i++) {
        config_channel(servos[i].channel, servos[i].gpio, 
                      angle_to_duty(servos[i].init_angle));
        servos[i].current_angle = servos[i].init_angle;
    }
}

// Инициализация кнопок
static void init_input_pins(void)
{
    for (int i = 0; i < 3; i++) {
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

// Применение позы
static void apply_pose(int pose_index)
{
    if (pose_index < 0 || pose_index >= total_poses) {
        ESP_LOGW(TAG, "Неверный индекс позы: %d", pose_index);
        return;
    }
    
    Pose* pose = &poses[pose_index];
    ESP_LOGI(TAG, "Применяем позу: %s (индекс %d)", pose->name, pose_index);
    
    uint32_t start_angles[5];
    uint32_t target_angles[5];
    int steps[5];
    bool moving[5];
    int max_steps = 0;
    
    for (int i = 0; i < 5; i++) {
        start_angles[i] = servos[i].current_angle;
        target_angles[i] = pose->angles[i];

        if (target_angles[i] > servos[i].max_angle) target_angles[i] = servos[i].max_angle;
        if (target_angles[i] < servos[i].min_angle) target_angles[i] = servos[i].min_angle;
        
        if (start_angles[i] != target_angles[i]) {
            steps[i] = (start_angles[i] < target_angles[i]) ? 1 : -1;
            moving[i] = true;
            int diff = abs((int)target_angles[i] - (int)start_angles[i]);
            if (diff > max_steps) {
                max_steps = diff;
            }
        } else {
            steps[i] = 0;
            moving[i] = false;
        }
    }
    
    uint32_t current_angles[5];
    memcpy(current_angles, start_angles, sizeof(current_angles));
    
    for (int step = 0; step <= max_steps; step++) {
        for (int i = 0; i < 5; i++) {
            if (moving[i]) {
                if (steps[i] > 0) {
                    if (current_angles[i] < target_angles[i]) {
                        current_angles[i]++;
                    }
                } else if (steps[i] < 0) {
                    if (current_angles[i] > target_angles[i]) {
                        current_angles[i]--;
                    }
                }
                ledc_set_duty(LEDC_LOW_SPEED_MODE, servos[i].channel, 
                            angle_to_duty(current_angles[i]));
                ledc_update_duty(LEDC_LOW_SPEED_MODE, servos[i].channel);

                servos[i].current_angle = current_angles[i];
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(MOV_SPEED)); 
    }
    
    ESP_LOGI(TAG, "Поза применена: %s", pose->name);
    current_pose = pose_index;
}


static void servo_task(void* arg)
{
    ServoCommand cmd;
    while (1) {
        if (xQueueReceive(servo_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {                    
                case CMD_RESET_ALL:
                    apply_pose(0);  
                    break;
                    
                case CMD_NEXT_POSE:
                    if (current_pose < total_poses - 1) {
                        apply_pose(current_pose + 1);
                    } else {
                        ESP_LOGW(TAG, "Достигнута последняя поза");
                    }
                    break;
                    
                case CMD_PREV_POSE:
                    if (current_pose > 0) {
                        apply_pose(current_pose - 1);
                    } else {
                        ESP_LOGW(TAG, "Достигнута первая поза");
                    }
                    break;
            }
        }
    }
}

// Задача обработки кнопок
static void button_task(void* arg)
{
    int current_states[3];
    
    last_states[0] = gpio_get_level(BUTTON_GPIO_1);
    last_states[1] = gpio_get_level(BUTTON_GPIO_2);
    last_states[2] = gpio_get_level(BUTTON_GPIO_3);
    
    ESP_LOGI(TAG, "Начальные состояния: BTN1=%d, BTN2=%d, BTN3=%d", 
             last_states[0], last_states[1], last_states[2]);
    
    while (1) {
        current_states[0] = gpio_get_level(BUTTON_GPIO_1);
        current_states[1] = gpio_get_level(BUTTON_GPIO_2);
        current_states[2] = gpio_get_level(BUTTON_GPIO_3);
        
        for (int i = 0; i < 3; i++) {
            if (current_states[i] == 0 && last_states[i] == 1) {
                ESP_LOGI(TAG, "Кнопка %d нажата", i + 1);
                
                ServoCommand cmd;
                memset(&cmd, 0, sizeof(cmd));
                
                switch (i) {
                    case 0:
                        cmd.type = CMD_RESET_ALL;
                        ESP_LOGI(TAG, "Отправка RESET");
                        break;
                    case 1:
                        cmd.type = CMD_NEXT_POSE;
                        ESP_LOGI(TAG, "Отправка NEXT");
                        break;
                    case 2:
                        cmd.type = CMD_PREV_POSE;
                        ESP_LOGI(TAG, "Отправка PREV");
                        break;
                }
                
                BaseType_t result = xQueueSend(servo_queue, &cmd, 10);
                if (result == pdTRUE) {
                    ESP_LOGI(TAG, "Команда отправлена успешно");
                } else {
                    ESP_LOGW(TAG, "Ошибка отправки команды: %d", result);
                }
            }
            last_states[i] = current_states[i];
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {    
    init_input_pins();
    config_timer();
    init_all_servos();
    
    servo_queue = xQueueCreate(10, sizeof(ServoCommand));
    if (servo_queue == NULL) {
        ESP_LOGE(TAG, "Не удалось создать очередь команд");
        return;
    }
    
    xTaskCreate(servo_task, "servo_task", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 2048, NULL, 4, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Текущая поза: %s (индекс %d/%d)", 
                 poses[current_pose].name, current_pose, total_poses - 1);
    }
}
