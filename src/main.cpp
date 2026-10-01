#include <Arduino.h>
#include <WiFi.h>
#include <nvs_flash.h>
#include <esp_err.h>

#include <Wifi.h>
#include <Netping.h>
#include <NTP.h>
#include <Clock.h>
#include <Radio.h>

static const uint32_t SERIAL_BAUD = 115200;

static String readMainCommand()
{
    String input;

    while (true) {
        while (Serial.available()) {
            char c = Serial.read();

            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    Serial.println();
                    return input;
                }
            } else {
                Serial.print(c);
                input += c;
            }
        }

        delay(10);
    }
}

static void resetAllConfiguration()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("RESET CONFIGURATION");
    Serial.println("========================================");
    Serial.println();
    Serial.println("WARNING: This will erase ALL persistent configuration.");
    Serial.println();
    Serial.println("The following will be removed:");
    Serial.println("- Wi-Fi configuration");
    Serial.println("- NTP configuration");
    Serial.println("- General radio configuration");
    Serial.println("- Radio enable/disable state");
    Serial.println("- All other data stored in NVS");
    Serial.println();
    Serial.println("Type RESET to continue.");
    Serial.println("X to cancel.");
    Serial.print("Select: ");

    String command = readMainCommand();
    command.trim();
    command.toUpperCase();

    if (command != "RESET") {
        Serial.println("Reset cancelled.");
        return;
    }

    Serial.println();
    Serial.println("Erasing NVS...");

    esp_err_t result = nvs_flash_erase();

    if (result != ESP_OK) {
        Serial.printf(
            "NVS erase failed: %s\n",
            esp_err_to_name(result)
        );
        return;
    }

    Serial.println("NVS erased.");
    Serial.println("Restarting...");

    delay(1000);

    ESP.restart();
}

static void showMainMenu()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("MAIN MENU");
    Serial.println("========================================");

    Serial.printf(
        "Wi-Fi : %s\n",
        Wifi::isConnected()
            ? "CONNECTED"
            : (Wifi::isConfigured() ? "CONFIGURED" : "NOT CONFIGURED")
    );

    Serial.printf(
        "NTP   : %s\n",
        NTP::isSynchronized()
            ? "SYNCHRONIZED"
            : (NTP::isConfigured() ? "CONFIGURED" : "NOT CONFIGURED")
    );

    Serial.printf(
        "Radio : %s\n",
        Radio::isDetected()
            ? (Radio::isEnabled() ? "ENABLED" : "DISABLED")
            : "NOT DETECTED"
    );

    Serial.println();
    Serial.println("1. Wi-Fi");
    Serial.println("2. Ping");
    Serial.println("3. NTP Time");
    Serial.println("4. Clock");
    Serial.println("5. LoRa Radio");
    Serial.println("6. Reset Configuration");
    Serial.println("X. Exit");
    Serial.print("Select: ");
}

static void featureMenu()
{
    while (true) {
        showMainMenu();

        String command = readMainCommand();
        command.trim();
        command.toUpperCase();

        if (command == "1") {
            Wifi::feature();
        } else if (command == "2") {
            Netping::feature();
        } else if (command == "3") {
            NTP::feature();
        } else if (command == "4") {
            Clock::feature();
        } else if (command == "5") {
            Radio::feature();
        } else if (command == "6") {
            resetAllConfiguration();
        } else if (command == "X") {
            Serial.println();
            Serial.println("Exiting menu.");

            return;
        } else {
            Serial.println("Invalid selection.");
        }
    }
}

void setup()
{
    Serial.begin(SERIAL_BAUD);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32 SYSTEM");
    Serial.println("========================================");

    WiFi.mode(WIFI_STA);

    Wifi::begin();
    Netping::begin();
    NTP::begin();
    Clock::begin();
    Radio::begin();

    if (Wifi::isConfigured()) {
        Wifi::connectSaved();
    }

    if (
        NTP::isConfigured() &&
        NTP::isEnabled() &&
        Wifi::isConnected()
    ) {
        NTP::syncNow();
    }

    featureMenu();
}

void loop()
{
    Wifi::service();
    Netping::service();
    NTP::service();
    Clock::service();
    Radio::service();

    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.equalsIgnoreCase("WAKE")) {
            Serial.println("System active.");
        }
    }

    delay(100);
}