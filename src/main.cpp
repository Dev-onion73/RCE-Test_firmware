#include <Arduino.h>
#include <WiFi.h>

#include <Wifi.h>
#include <Netping.h>

static const uint32_t SERIAL_BAUD = 115200;

static String readMainCommand() {
    String input;

    while (true) {
        while (Serial.available()) {
            char c = Serial.read();

            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    Serial.println();
                    input.trim();
                    return input;
                }
            } else {
                input += c;
                Serial.print(c);
            }
        }

        delay(5);
    }
}

static void showMainMenu() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32 NETWORK UTILITY");
    Serial.println("========================================");

    if (Wifi::isConnected()) {
        Serial.println("Wi-Fi Status : CONNECTED");
        Serial.printf("SSID         : %s\n", Wifi::ssid().c_str());
        Serial.printf("IP           : %s\n", Wifi::ipAddress().c_str());
    } else {
        Serial.println("Wi-Fi Status : NOT CONNECTED");
    }

    Serial.println();
    Serial.println("1. Wi-Fi");
    Serial.println("2. Ping");
    Serial.println("X. Exit");
    Serial.println();
    Serial.print("Select feature: ");
}

static void featureMenu() {
    while (true) {
        showMainMenu();

        String command = readMainCommand();
        command.trim();

        if (command == "1") {
            Wifi::feature();
        } else if (command == "2") {
            Netping::feature();
        } else if (command.equalsIgnoreCase("X")) {
            Serial.println("Menu idle.");
            return;
        } else {
            Serial.println("Invalid selection.");
        }
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    WiFi.mode(WIFI_STA);

    Wifi::begin();
    Netping::begin();

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32 NETWORK UTILITY");
    Serial.println("========================================");

    if (Wifi::isConfigured()) {
        Serial.println("Saved Wi-Fi configuration detected.");
        Wifi::connectSaved();
    }

    Serial.println("Boot complete.");
    featureMenu();
}

void loop() {
    Wifi::service();
    Netping::service();

    delay(100);
}