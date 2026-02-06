## Plan: Display Stored NVS Values in Web UI

Pre-populate web form fields with stored configuration values from NVS. Passwords are not returned—instead, a flag indicates whether they're set, and the UI displays `********` as a placeholder.

### Steps

1. **Add config GET endpoint** in `main/http_server.c`
   - Implement `config_get_handler()` that:
     - Uses `internal_storage_check_*_preserved()` for each setting
     - Calls getters for non-password values (WiFi SSID, MQTT broker, port, client ID, username, topic)
     - For password fields return boolean flags `wifi_password_set` and `mqtt_password_set` instead of actual passwords
     - Build JSON response similar to existing handlers (use `snprintf`)
   - Register new `httpd_uri_t` for `GET /config` in webserver start

2. **Add JavaScript loader** in `main/web/index.html`
   - Add `loadConfig()` called from `window.onload` (or invoked alongside `checkUpdate()`)
   - `loadConfig()` should `fetch('/config')`, parse JSON and populate inputs:
     - `ssid` ← `wifi_ssid`
     - If `wifi_password_set` true → set password field value to `********`, else leave empty
     - `mqtt-server`, `mqtt-port`, `mqtt-client-id`, `mqtt-username`, `mqtt-topic` ← corresponding JSON fields
     - If `mqtt_password_set` true → set mqtt-password to `********`
   - Add `onfocus` handler for password fields to clear `********` when user focuses the field

3. **Server-side security/validation**
   - Ensure `config_get_handler()` does NOT return actual passwords
   - Limit JSON response buffer size (e.g., 512 bytes) and validate string lengths before inclusion

4. **Form submit behavior**
   - When POSTing forms, ensure server-side handlers treat `********` as "no change" for passwords (i.e., do not overwrite stored password with literal asterisks). If form password equals `********` server should skip saving password.
   - Update `configure_wifi_post_handler()` and `configure_mqtt_post_handler()` to check the posted password against placeholder and skip saving if unchanged.

5. **Register endpoint and update UI wiring**
   - Register `/config` handler in `main/http_server.c` along with other URIs
   - Call `loadConfig()` on page load before/after `checkUpdate()`

6. **Testing & verification**
   - Boot device, configure WiFi and MQTT via web UI
   - Reload web UI and verify fields are populated
   - Confirm password fields display `********` when set and are cleared on focus
   - Submit forms without changing password and confirm stored passwords are preserved
   - Submit forms with new password and confirm updated in NVS

7. **Documentation**
   - Update README and internal docs to mention the `/config` endpoint and the `********` password UX

### JSON Response Example
```
{
  "wifi_ssid": "MyNetwork",
  "wifi_password_set": true,
  "mqtt_broker": "mqtt.example.com",
  "mqtt_port": 1883,
  "mqtt_client_id": "homepost",
  "mqtt_username": "user",
  "mqtt_password_set": true,
  "mqtt_topic": "home/sensors"
}
```

### Notes
- Reuse internal storage getters and checkers in `internal_storage.c` via `inc/internal_storage.h`.
- Keep response small and deterministic; build JSON with `snprintf` like other handlers.
- Ensure client-side fields include `maxlength` and basic validation already present.
