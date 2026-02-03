## Plan: Add OTA Update Support via GitHub Releases

Enable the ESP32 device to automatically check for firmware updates on GitHub releases, download newer versions over HTTPS, and self-update using ESP-IDF's native OTA capabilities. Firmware assets follow naming `homepost-{version}.bin`, tags follow `release-v{version}` format.

### Steps

1. **Switch partition table to OTA-compatible layout** in `menuconfig` → Partition Table → select "Factory app, two OTA definitions", and enable `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` under Bootloader config for automatic rollback on failed boot.

2. **Create [main/ota_update.c](main/ota_update.c) and [inc/ota_update.h](inc/ota_update.h)** implementing a FreeRTOS task that:
   - Waits 30 seconds after start for network stability, then waits for WiFi via `wifi_is_connected_to_internet()`
   - Fetches `https://api.github.com/repos/{owner}/{repo}/releases/latest` using `esp_http_client`
   - Parses JSON with `cJSON`: extracts `tag_name`, strips `release-v` prefix to get version (e.g., `release-v1.2.0` → `1.2.0`)
   - Finds asset URL matching `homepost-{version}.bin` from `assets[].browser_download_url`
   - Compares semver against `CONFIG_APP_PROJECT_VER`
   - On newer version: calls `tracker_scanner_stop()` to free heap, runs `esp_https_ota()` with `esp_crt_bundle_attach()` for GitHub's CA chain, then `esp_restart()`
   - Loops with `CONFIG_HOMEPOST_OTA_CHECK_INTERVAL_HOURS` delay (default 4 hours)

3. **Add OTA Kconfig options** in [main/Kconfig.projbuild](main/Kconfig.projbuild):
   - `CONFIG_HOMEPOST_OTA_ENABLED` (bool, default y)
   - `CONFIG_HOMEPOST_OTA_GITHUB_OWNER` (string)
   - `CONFIG_HOMEPOST_OTA_GITHUB_REPO` (string, default "homepost")
   - `CONFIG_HOMEPOST_OTA_CHECK_INTERVAL_HOURS` (int, default 4)
   - `CONFIG_HOMEPOST_OTA_INITIAL_DELAY_SECONDS` (int, default 30)
   
   Add required components to REQUIRES in [main/CMakeLists.txt](main/CMakeLists.txt): `esp_https_ota`, `esp_http_client`, `app_update`, `esp_crt_bundle`, `json`

4. **Integrate OTA task startup** in [main/main.c](main/main.c) by calling `ota_update_start_task()` after WiFi initialization, guarded by `#if CONFIG_HOMEPOST_OTA_ENABLED`, following existing task patterns with `configASSERT()`.

5. **Add HTTP endpoints** in [main/http_server.c](main/http_server.c):
   - `GET /check-update` → returns JSON `{"current_version": "1.2.0", "available_version": "1.2.1", "update_available": true}`
   - `POST /trigger-update` → stops BLE tracking, triggers immediate OTA, restarts on success
   - Update [main/web/index.html](main/web/index.html) with version display and "Check for Updates" button

6. **Update documentation**: Add OTA section to [README.md](README.md) explaining GitHub release requirements (tag format `release-v{version}`, asset naming `homepost-{version}.bin`), Kconfig options, rollback behavior. Update [.github/copilot-instructions.md](.github/copilot-instructions.md) with OTA architecture. Bump `CONFIG_APP_PROJECT_VER` in [sdkconfig](sdkconfig).
