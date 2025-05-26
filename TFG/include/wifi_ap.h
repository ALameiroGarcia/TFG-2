#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stdbool.h>

void start_wifi_ap();
void connect_to_wifi(const char *ssid, const char *password);
bool wifi_is_connected();
void try_auto_connect();

#endif
