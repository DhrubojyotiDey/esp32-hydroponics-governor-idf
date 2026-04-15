#ifndef DNS_SERVER_H
#define DNS_SERVER_H
#include "esp_err.h"
/**
 * @brief Start the captive-portal DNS server.
 *
 * Listens on UDP port 53. Responds to EVERY DNS query with
 * the AP IP (192.168.4.1) so that all domain lookups from a
 * phone connected to the AP resolve to this device.
 *
 * This is what makes Android/iOS show the "Sign in to network"
 * notification and open the captive portal automatically.
 */
esp_err_t dns_server_start(void);
void      dns_server_stop(void);
#endif /* DNS_SERVER_H */
