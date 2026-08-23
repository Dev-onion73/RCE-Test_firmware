#include <Arduino.h>
#include <WiFi.h>

#include <Wifi.h>
#include <Netping.h>
#include <NTP.h>
#include <lora_radio.h>

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

    if (NTP::hasValidSystemTime()) {
        Serial.printf("Time         : %s\n", NTP::currentTime().c_str());
        Serial.printf("Date         : %s\n", NTP::currentDate().c_str());

        if (NTP::isSynchronized()) {
            Serial.println("NTP Status   : SYNCHRONIZED");
        } else if (NTP::isConfigured() && NTP::isEnabled()) {
            Serial.println("NTP Status   : ENABLED / NOT SYNCED");
        } else if (NTP::isConfigured()) {
            Serial.println("NTP Status   : DISABLED");
        } else {
            Serial.println("NTP Status   : NOT CONFIGURED");
        }
    } else {
        Serial.println("Time         : NOT SET");

        if (NTP::isConfigured() && NTP::isEnabled()) {
            Serial.println("NTP Status   : ENABLED / NOT SYNCED");
        } else if (NTP::isConfigured()) {
            Serial.println("NTP Status   : DISABLED");
        } else {
            Serial.println("NTP Status   : NOT CONFIGURED");
        }
    }

    Serial.println();
    Serial.println("1. Wi-Fi");
    Serial.println("2. Ping");
    Serial.println("3. NTP Time");
    Serial.println("4. LoRa Radio");
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
        } else if (command == "3") {
            NTP::feature();
        } else if (command == "4") {
            LoRaRadio::feature();
        } else if (command.equalsIgnoreCase("X")) {
            Serial.println("Node entering standby.");
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
    NTP::begin();
    LoRaRadio::begin();

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32 NETWORK UTILITY");
    Serial.println("========================================");
    Serial.println("Persistent configuration loaded.");

    if (Wifi::isConfigured()) {
        Serial.println("Saved Wi-Fi configuration detected.");
        Wifi::connectSaved();
    }

    if (NTP::isConfigured()) {
        Serial.println("Saved NTP configuration detected.");

        if (NTP::isEnabled()) {
            Serial.println("NTP is enabled.");

            if (Wifi::isConnected()) {
                if (NTP::syncNow()) {
                    Serial.println("Startup NTP synchronization successful.");
                } else {
                    Serial.println("Startup NTP synchronization failed.");
                }
            } else {
                Serial.println("Waiting for Wi-Fi before NTP synchronization.");
            }
        } else {
            Serial.println("NTP is disabled.");
        }
    }

    Serial.println("Boot complete.");
    featureMenu();
}

void loop() {
    Wifi::service();
    Netping::service();
    NTP::service();
    LoRaRadio::service();

    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.equalsIgnoreCase("WAKE")) {
            featureMenu();
        }
    }

    delay(100);
}