/* ============================================================
 *  telnet_server.h — TCP log-sink (port 23)
 *
 *  Starts a single-client Telnet server.  When a client connects
 *  every string passed to telnet_server_send() is forwarded to it
 *  over TCP, newline-terminated.  Only one concurrent client is
 *  supported; a new connection evicts the previous one.
 *
 *  Usage in main.c:
 *      ESP_ERROR_CHECK(telnet_server_start());
 *      ...
 *      telnet_server_send("some log string");
 * ============================================================*/

#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include "esp_err.h"

/**
 * @brief Initialise and start the Telnet log server.
 *
 * Spawns a FreeRTOS task pinned to Core 1 that listens on
 * TELNET_PORT (defined in app_config.h, default 23).
 *
 * @return ESP_OK on success, ESP_FAIL / ESP_ERR_NO_MEM on error.
 */
esp_err_t telnet_server_start(void);

/**
 * @brief Send a log line to the connected Telnet client (if any).
 *
 * Non-blocking — if no client is connected or the send would
 * block, the message is silently dropped.
 * Safe to call from any task context.
 *
 * @param msg  NUL-terminated string to transmit (CRLF appended).
 */
void telnet_server_send(const char *msg);

#endif /* TELNET_SERVER_H */
