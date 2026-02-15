
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spiffs.h"

void list_spiffs_files(void) {
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE("spiffs", "Failed to open dir /spiffs");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI("spiffs", "Found: %s", entry->d_name);
    }
    closedir(dir);
}
