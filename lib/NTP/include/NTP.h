#ifndef NTP_H
#define NTP_H

#include <Arduino.h>

namespace NTP {

void begin();
void feature();
void service();

bool isConfigured();
bool isEnabled();
bool isSynchronized();
bool hasValidSystemTime();

bool syncNow();

String currentTime();
String currentDate();

}

#endif