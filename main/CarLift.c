#include <stdio.h>
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define SPIFFS_BASE_PATH "/storage"
#define SPIFFS_MAX_FILES 5
#define SLOT_AMOUNT 16
#define ASCII_NUMBER_OFFSET 48
#define FILE_LINK "/storage/myfile.txt"

static const char* TAG = "FleSystem";

static bool lots[SLOT_AMOUNT] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
    Khởi tạo hệ thống file ảo spiffs
*/
void init_spiffs() {
    const esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = SPIFFS_MAX_FILES,
        .format_if_mount_failed = true
    };

    esp_err_t result = esp_vfs_spiffs_register(&conf);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(result));
        return;
    }

    size_t total = 0, used = 0;
    result = esp_spiffs_info(conf.partition_label, &total, &used);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition info (%s)", esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
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

    if (fseek(f, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "Failed to load end of file!");
        return;
    }

    fprintf(f, "\n");
    /*int i = 0;
    for (; data[i] != '\0'; i++)
        fprintf(f, "%c", data[i]);*/
    fprintf(f, data);
    if (fclose(f) == 0) {
        printf("Close file successfully!\n");
    }
    return;
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
    //char* fileContent = read_file();
    /*get_current_slot(fileContent);
    fileContent = read_file();
    printf("%s\n\n", fileContent);
    get_cars(fileContent);*/
    write_file("30H3-4567 6 1");
    char* fileContent = read_file();
    printf("%s\n", fileContent);
}
