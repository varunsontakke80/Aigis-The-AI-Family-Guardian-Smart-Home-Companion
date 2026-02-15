#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t main_ui_start(void);
void main_ui_show(bool visible);

#ifdef __cplusplus
}
#endif
