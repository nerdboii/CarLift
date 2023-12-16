#include <stdio.h>
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "esp_check.h"
#include "esp_log.h"

#define SPIFFS_BASE_PATH "/storage"
#define SPIFFS_MAX_FILES 5
#define SLOT_AMOUNT 16

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
    FILE* f = fopen("/storage/myfile.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading!");
        return NULL;
    }

    char content[100];
    int i = 0;
    while (!feof(f)) {
        char ch = fgetc(f);
        if (ch == '.') break;
        content[i++] = ch;
    }
    i--;
    fclose(f);
    printf("%s\n", content);
    char* result = content;
    return result;
}

void slot_display(char* fileContent) {
    int i = 0, spaceCount = 0, slot = 0;
    while (fileContent[i]) {
        if (fileContent[i] == '\n') {
            spaceCount = 0;
            slot = 0;
        }
        if (fileContent[i] == ' ') {
            spaceCount++;
        } else {
            if (spaceCount == 1) slot = slot * 10 + (int)fileContent[i] - 48;
        }
        if (spaceCount == 2) lots[slot - 1] = 1;
        i++;
    }

    for (int i = 0; i < SLOT_AMOUNT; i++) {
        if (lots[i] == 1) printf("X");
        else printf("O");
    }
}

void AddCar() {

}

void app_main(void)
{
    init_spiffs();
    slot_display(read_file());
}
