/* ============================================================
 *  telnet_server.c — TCP log-sink on TELNET_PORT (default: 23)
 *
 *  Architecture
 *  ────────────
 *  One FreeRTOS task (Core 1, priority 1) runs the accept loop.
 *  A mutex guards the single client file-descriptor so
 *  telnet_server_send() is safe to call from any task.
 *
 *  Client lifecycle
 *  ────────────────
 *  accept() → store fd → poll with 1-second recv timeout to
 *  detect clean disconnect (n == 0) or errors (errno != EAGAIN).
 *  On disconnect: fd closed, cleared, accept() waits for next.
 * ============================================================*/

#include "telnet_server.h"
#include "app_config.h"

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "TELNET";

static int              s_client_fd = -1;
static SemaphoreHandle_t s_mutex    = NULL;

/* ── Internal: close current client and reset fd ───────────── */
static void close_client(int fd)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_client_fd = -1;
    xSemaphoreGive(s_mutex);
    close(fd);
    ESP_LOGI(TAG, "Client disconnected");
}

/* ── Telnet server task ─────────────────────────────────────── */
static void telnet_task(void *arg)
{
    /* ── Create TCP socket ──────────────────────────────────── */
    int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(TELNET_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(server_fd);
        vTaskDelete(NULL);
        return;
    }

    listen(server_fd, 1);
    ESP_LOGI(TAG, "Listening on TCP port %d", TELNET_PORT);

    /* ── Accept loop ────────────────────────────────────────── */
    while (true) {
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* 1-second receive timeout — lets us poll for disconnect */
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        ESP_LOGI(TAG, "Client connected");
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_client_fd = client;
        xSemaphoreGive(s_mutex);

        /* Poll until the client disconnects */
        char dummy[4];
        while (true) {
            int n = recv(client, dummy, sizeof(dummy), 0);
            if (n == 0) {
                /* Clean close by client */
                break;
            }
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                /* Socket error — treat as disconnect */
                break;
            }
            /* EAGAIN / EWOULDBLOCK = timeout, client still alive */
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        close_client(client);
    }
}

/* ── Public API ─────────────────────────────────────────────── */

esp_err_t telnet_server_start(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Reuse STACK_DNS size — defined in app_config.h (3072 bytes) */
    BaseType_t ret = xTaskCreatePinnedToCore(
        telnet_task, "Telnet", STACK_DNS, NULL, 1, NULL, 1);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Task create failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void telnet_server_send(const char *msg)
{
    if (!s_mutex || !msg) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int fd = s_client_fd;
    xSemaphoreGive(s_mutex);

    if (fd < 0) return;   /* No client connected — silently drop */

    size_t len = strlen(msg);
    send(fd, msg,    len, MSG_DONTWAIT);
    send(fd, "\r\n",   2, MSG_DONTWAIT);
}
