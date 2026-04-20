#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

/* ── Connection state ────────────────────────────────────── */
typedef enum {
    WIFI_MGR_IDLE,        /* not started yet                    */
    WIFI_MGR_AP_ONLY,     /* AP up, waiting for credentials     */
    WIFI_MGR_CONNECTING,  /* STA handshake in progress          */
    WIFI_MGR_CONNECTED,   /* STA connected, got IP              */
    WIFI_MGR_FAILED,      /* connection failed (wrong password)  */
} wifi_mgr_state_t;

/**
 * Callback invoked exactly once when the AP shuts down.
 * Used by main.c to stop the DNS server without creating a
 * direct dependency between wifi_manager and dns_server.
 */
typedef void (*wifi_mgr_ap_shutdown_cb_t)(void);

/** Init NVS, netif, event loop, WiFi driver. Call once before all else. */
esp_err_t wifi_manager_init(void);

/**
 * Register a callback fired when the AP shuts down.
 * Must be called before wifi_manager_start().
 * Only one callback slot — subsequent calls overwrite.
 */
void wifi_manager_set_ap_shutdown_cb(wifi_mgr_ap_shutdown_cb_t cb);

/**
 * Read NVS; direct STA if credentials exist, AP otherwise.
 * Blocks up to 15s on direct STA; auto-clears NVS and reboots on failure.
 * Returns immediately in AP mode.
 */
esp_err_t wifi_manager_start(void);

/**
 * Save credentials to NVS and initiate STA connection (non-blocking).
 * Only valid in WIFI_MGR_AP_ONLY or WIFI_MGR_FAILED state.
 * `open` = true for networks with no password — prevents the WPA2
 * auth threshold from rejecting the association at the driver level.
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *pass, bool open);

/**
 * Reconnect after a runtime WiFi drop.
 * Reads stored credentials from NVS and calls esp_wifi_connect().
 * Does NOT re-init netif, event loop, or WiFi driver — safe to
 * call from the main loop on repeated WIFI_MGR_FAILED detections.
 */
esp_err_t wifi_manager_reconnect(void);

/** Start async WiFi scan. Results available after WIFI_EVENT_SCAN_DONE. */
esp_err_t wifi_manager_scan_start(void);

/** True while the scan is still running. */
bool wifi_manager_scan_running(void);

/** Fill buf with a JSON array of visible SSIDs. Empty array if scanning. */
void wifi_manager_get_scan_json(char *buf, size_t len);

/** Current state machine state. */
wifi_mgr_state_t wifi_manager_get_state(void);

/** IP address string — only valid when state == WIFI_MGR_CONNECTED. */
const char *wifi_manager_get_ip(void);

/** Gateway IP string — valid after STA gets an IP. */
const char *wifi_manager_get_gateway_ip(void);

/** True if the AP interface is currently active. */
bool wifi_manager_ap_active(void);

/**
 * Shut down the AP interface.
 * Switches WiFi to STA-only, fires the registered shutdown callback,
 * and logs the event. No-ops if AP is already down.
 */
void wifi_manager_ap_shutdown(void);

#endif /* WIFI_MANAGER_H */
