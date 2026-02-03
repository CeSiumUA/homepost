#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdio.h>
#include <stdbool.h>
#include <esp_err.h>

/**
 * @brief Start the OTA update background task
 * 
 * The task waits for an initial delay, then periodically checks for updates
 * from GitHub releases. If a newer version is found, it downloads and installs
 * the firmware, then restarts the device.
 */
void ota_update_start_task(void);

/**
 * @brief Stop the OTA update background task
 */
void ota_update_stop_task(void);

/**
 * @brief Trigger an immediate OTA update check
 * 
 * @return ESP_OK if check initiated, ESP_FAIL on error
 */
esp_err_t ota_update_check_now(void);

/**
 * @brief Check if an update is currently available
 * 
 * @return true if a newer version is available
 */
bool ota_update_is_available(void);

/**
 * @brief Get the current firmware version
 * 
 * @return Version string (e.g., "1.2.0")
 */
const char* ota_update_get_current_version(void);

/**
 * @brief Get the latest available version from GitHub
 * 
 * @return Version string if available, NULL if not checked yet
 */
const char* ota_update_get_available_version(void);

#endif // OTA_UPDATE_H
