#ifndef WIFI_H
#define WIFI_H

#include <Arduino.h>

namespace Wifi {

void begin();
void service();
void feature();

bool isConfigured();
bool isConnected();

bool connectSaved();

String ssid();
String ipAddress();

}

#endif