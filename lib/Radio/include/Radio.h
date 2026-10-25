#ifndef RADIO_H
#define RADIO_H

#include <ArduinoJson.h>

namespace Radio {

void begin();
void service();
void feature();

bool isConfigured();
bool isEnabled();
bool isDetected();
bool isReady();

void setEnabled(bool enabled);

// Exposed for RadioTest module
float getFrequency();
float getBandwidth();
uint8_t getSpreadingFactor();
uint8_t getCodingRate();
int8_t getTxPower();
uint16_t getPreambleLength();
uint8_t getSyncWord();
bool getCrcEnabled();
String getNodeName();

void setFrequency(float f);
void setBandwidth(float bw);
void setSpreadingFactor(uint8_t sf);
void setCodingRate(uint8_t cr);
void setTxPower(int8_t power);

uint32_t getBroadcastSequence();
void incrementBroadcastSequence();

// Transmit a packet
bool transmitPacket(const String& packet);

// Start receive mode
bool startReceiveMode();

// Read received packet
bool readReceivedPacket(String& packet, float& rssi, float& snr);

// Stop receive mode
void stopReceiveMode();

// Set packet received callback
void setPacketReceivedCallback(void (*callback)());

String createBroadcastPacket();
String getUTCTimestamp();
void displayJson(JsonVariantConst value, const String& key);

}

#endif