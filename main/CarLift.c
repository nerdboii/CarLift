#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#define FAT_MAX_FILES 4
#define SLOT_AMOUNT 16
#define ASCII_NUMBER_OFFSET 48
#define FILE_LINK "/spiflash/myfile.txt"
#define HOUR_TO_MS 3600000000000
#define CAR_ARRIVE_PIN GPIO_NUM_2
#define PARKING_LOT_PIN_1 GPIO_NUM_4
#define PARKING_LOT_PIN_2 GPIO_NUM_5
#define EVENT_BIT_CAR_ARRIVE BIT0
#define EVENT_BIT_LOT_1 BIT1
#define EVENT_BIT_LOT_2 BIT2
#define THRESHOLD_LIGHT_LEVEL 1500
#define QUEUE_LENGTH 12

static const char *TAG = "FAT";
// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
// Mount path for the partition
const char *base_path = "/spiflash";

static bool lots[SLOT_AMOUNT] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

EventGroupHandle_t xEventGroup;
SemaphoreHandle_t file_semaphore;

void get_car_reg();
void get_car_park_1();
void get_car_park_2();
void write_file();
void write_file_end_line();

void IRAM_ATTR car_arrive_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int light_value, len = 0;
    adc2_get_raw(ADC2_CHANNEL_2, ADC_WIDTH_BIT_12, &light_value);
    char buffer;
    if (light_value < THRESHOLD_LIGHT_LEVEL) {
        xEventGroupSetBitsFromISR(xEventGroup, EVENT_BIT_CAR_ARRIVE, &xHigherPriorityTaskWoken);
        while (1) {
            if (xSemaphoreTakeFromISR(file_semaphore, portMAX_DELAY) == pdTRUE) {
                // Mở và thao tác với file ở đây (ví dụ: đọc, ghi...)
                write_file_end_line("16K5-5585");

                xSemaphoreGiveFromISR(file_semaphore, &xHigherPriorityTaskWoken);
            }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Chờ 1 giây trước khi thao tác tiếp
        }
    }
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR car_park_isr_handler_1(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int light_value;
    adc2_get_raw(ADC2_CHANNEL_0, ADC_WIDTH_BIT_12, &light_value);
    if (light_value < THRESHOLD_LIGHT_LEVEL) {
        xEventGroupSetBitsFromISR(xEventGroup, EVENT_BIT_LOT_1, &xHigherPriorityTaskWoken);
        char* dataToFile = " 1 0";
        while (1) {
            if (xSemaphoreTakeFromISR(file_semaphore, portMAX_DELAY) == pdTRUE) {
                write_file(dataToFile);

                xSemaphoreGiveFromISR(file_semaphore, &xHigherPriorityTaskWoken);
            }
        }
    }
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR car_park_isr_handler_2(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(xEventGroup, EVENT_BIT_LOT_2, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void get_car_reg() {
    // Thiết lập chân gpio để xác nhận có xe vừa đến quầy thu ngân
    esp_rom_gpio_pad_select_gpio(CAR_ARRIVE_PIN);
    gpio_set_direction(CAR_ARRIVE_PIN, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC2_CHANNEL_2, ADC_ATTEN_DB_0);

    gpio_set_intr_type(CAR_ARRIVE_PIN, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(CAR_ARRIVE_PIN, car_arrive_isr_handler, (void*) CAR_ARRIVE_PIN);

    while (1) {
        printf("Wait for car arrival... \n");
        int light_value;
        adc2_get_raw(ADC2_CHANNEL_2, ADC_WIDTH_BIT_12, &light_value);
        printf("%d\n", light_value);
        // Chờ bit 0 được thiết lập trong event group
        EventBits_t bits = xEventGroupWaitBits(xEventGroup, EVENT_BIT_CAR_ARRIVE, pdTRUE, pdFALSE, portMAX_DELAY);

        // Kiểm tra xem bit 0 đã được thiết lập chưa
        if (bits & EVENT_BIT_CAR_ARRIVE) {
            printf("Interrupted!\n");

        }
    }
}

void get_car_park_1() {
    // Thiết lập chân gpio để xác định xe đã đỗ ở ô nào
    esp_rom_gpio_pad_select_gpio(PARKING_LOT_PIN_1);
    gpio_set_direction(PARKING_LOT_PIN_1, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC2_CHANNEL_0, ADC_ATTEN_DB_0);

    gpio_set_intr_type(PARKING_LOT_PIN_1, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(PARKING_LOT_PIN_1, car_park_isr_handler_1, (void*) PARKING_LOT_PIN_1);

    while (1) {
        printf("Waiting car park at lot 1... \n");
        int light_value;
        adc2_get_raw(ADC2_CHANNEL_0, ADC_WIDTH_BIT_12, &light_value);
        printf("%d\n", light_value);
        // Chờ bit 0 được thiết lập trong event group
        EventBits_t bits = xEventGroupWaitBits(xEventGroup, EVENT_BIT_LOT_1, pdTRUE, pdFALSE, portMAX_DELAY);

        // Kiểm tra xem bit 1 đã được thiết lập chưa
        if (bits & EVENT_BIT_LOT_1) {
            printf("Interrupted!\n");

        }
    }
}

void get_car_park_2() {
    // Thiết lập chân gpio để xác định xe đã đỗ ở ô nào
    esp_rom_gpio_pad_select_gpio(PARKING_LOT_PIN_2);
    gpio_set_direction(PARKING_LOT_PIN_2, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_0);

    gpio_set_intr_type(PARKING_LOT_PIN_2, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(PARKING_LOT_PIN_2, car_park_isr_handler_2, (void*) PARKING_LOT_PIN_2);

    while (1) {
        printf("Waiting car park at lot 2... \n");
        //int light_value = adc1_get_raw(ADC1_CHANNEL_6);
        //printf("%d\n", light_value);
        // Chờ bit 0 được thiết lập trong event group
        EventBits_t bits = xEventGroupWaitBits(xEventGroup, EVENT_BIT_LOT_2, pdTRUE, pdFALSE, portMAX_DELAY);

        // Kiểm tra xem bit 2 đã được thiết lập chưa
        if (bits & EVENT_BIT_LOT_2) {
            printf("Interrupted!\n");

        }
    }
}

/*
    Khởi tạo hệ thống file ảo spiffs
*/
void init_spiffs() {
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}

/*
Đọc file từ hệ thống
*/
char* read_file() {
    FILE* f = fopen(FILE_LINK, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading!");
        return NULL;
    }

    char* content = malloc(1 * sizeof(char));
    content[0] = '\0';
    int i = 0;
    while (!feof(f)) {
        char ch = fgetc(f);
        if (ch == '\0') break;
        content[i++] = ch;
    }
    content[--i] = '\0';
    fclose(f);
    //printf("%s\n", content);
    return content;
}

char* get_current_slot(char* fileContent) {
    int i = 0, spaceCount = 0, slot = 0;
    for (; *fileContent != '\0'; fileContent++) {
        //printf("%d\n", i);
        if (*fileContent == '\n') {
            spaceCount = 0;
            slot = 0;
        }
        if (*fileContent == ' ') {
            spaceCount++;
        } else {
            if (spaceCount == 1) slot = slot * 10 + (int)*fileContent - ASCII_NUMBER_OFFSET;
        }
        if (spaceCount == 2) lots[slot - 1] = 1;
    }

    char* displayString = malloc(16 * sizeof(char));

    for (i = 0; i < SLOT_AMOUNT; i++) {
        if (lots[i] == 1)  displayString[i] = 'X';
        else displayString[i] = 'O';
        printf("%c", displayString[i]);
        if (i == SLOT_AMOUNT - 1) break;
    }
    printf("\n");

    return displayString;

    free(displayString);
}

char** get_cars(char* fileContent) {
    char* eachCar = malloc(1 * sizeof(char));
    eachCar[0] = '\0';
    char** allCars = malloc(5 * sizeof(char*));
    uint8_t carAmount = 0, eachLen = 0;
    bool check = true;

    for (; *fileContent != '\0'; fileContent++) {
        if (*fileContent == '\n' || check == true) {
            check = false;
            eachLen = 0;

            if (*fileContent == '\n') fileContent++;

            while (true) {
                if (*fileContent == ' ') {
                    eachCar[eachLen] = '\0';
                    allCars[carAmount++] = eachCar;
                    printf("%s\n", eachCar);
                    break;
                }
                eachCar[eachLen++] = *(fileContent++);
            }
        }
    }

    return allCars;
}

void write_file(const char* data) {
    FILE* f = fopen(FILE_LINK, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing!");
        return;
    }

    fprintf(f, data);
    if (fclose(f) == 0) {
        ESP_LOGI(TAG, "File has been closed!");
    }

    return;
}

void write_file_end_line(const char* data) {
    FILE* f = fopen(FILE_LINK, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing!");
        return;
    }

    fprintf(f, "\n");
    fprintf(f, data);
    if (fclose(f) == 0) {
        ESP_LOGI(TAG, "File has been closed!");
    }

    return;
}

long long get_time_from_reg(char* reg) {
    char* fileContent = read_file();
    int spaceCount = 0;
    long long time = 0;

    for (; *fileContent != '\0'; fileContent++) {
        int i = 0;
        bool ok = 1;
        for (; reg[i] != '\0'; i++) {
            if (reg[i] != *(fileContent+i)) {
                ok = 0;
                break;
            }
        }
        if (ok == 1 && spaceCount == 2) {
            time = time * 10 + (long long)(*fileContent) - ASCII_NUMBER_OFFSET;
        }
        if (*fileContent == ' ') spaceCount++;
    }

    return time;
}

bool stringExistsInFile(const char *data) {
    FILE *file = fopen(FILE_LINK, "r");
    if (file != NULL) {
        char buffer[100]; // Độ dài tối đa của chuỗi trong tệp
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            if (strstr(buffer, data) != NULL) {
                fclose(file);
                return true;
            }
        }
        fclose(file);
    }
    return false;
}

void app_main(void)
{
    init_spiffs();
    //init_queue();
    gpio_install_isr_service(0);
    file_semaphore = xSemaphoreCreateMutex();

    xEventGroup = xEventGroupCreate();
    xTaskCreate(&get_car_reg, "GetCarReg", 2048, NULL, 5, NULL);
    xTaskCreate(&get_car_park_1, "GetCarPark1", 2048, NULL, 5, NULL);
   // xTaskCreate(&get_car_park_2, "GetCarPark2", 2048, NULL, 5, NULL);
    //printf("%s\n", read_file());

    while (true) {
        vTaskDelay(10);
    }
    //write_file("30H3-4567 6 1");
    //printf("%s\n", read_file());
}
