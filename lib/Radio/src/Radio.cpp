#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <RadioLib.h>
#include <cstdlib>

#include "Radio.h"

namespace Radio {

static constexpr uint8_t RADIO_SCK = 36;
static constexpr uint8_t RADIO_MISO = 37;
static constexpr uint8_t RADIO_MOSI = 35;

static constexpr uint8_t RADIO_NSS = 34;
static constexpr uint8_t RADIO_DIO1 = 40;
static constexpr uint8_t RADIO_RESET = 38;
static constexpr uint8_t RADIO_BUSY = 39;

static constexpr uint32_t RADIO_SPI_FREQUENCY = 4000000;

static constexpr float DEFAULT_FREQUENCY = 868.0f;
static constexpr float DEFAULT_BANDWIDTH = 125.0f;
static constexpr uint8_t DEFAULT_SPREADING_FACTOR = 7;
static constexpr uint8_t DEFAULT_CODING_RATE = 5;
static constexpr int8_t DEFAULT_TX_POWER = 14;
static constexpr uint16_t DEFAULT_PREAMBLE_LENGTH = 8;
static constexpr uint8_t DEFAULT_SYNC_WORD = 0x12;
static constexpr bool DEFAULT_CRC = true;
static constexpr const char* DEFAULT_NODE_NAME = "NODE-01";

static Preferences preferences;

static SPIClass spi(FSPI);

static SPISettings spiSettings(
    RADIO_SPI_FREQUENCY,
    MSBFIRST,
    SPI_MODE0
);

static SX1262 radio = new Module(
    RADIO_NSS,
    RADIO_DIO1,
    RADIO_RESET,
    RADIO_BUSY,
    spi,
    spiSettings
);

static bool configured = false;
static bool enabled = false;
static bool detected = false;
static bool ready = false;

static int radioError = RADIOLIB_ERR_NONE;

static float frequency = DEFAULT_FREQUENCY;
static float bandwidth = DEFAULT_BANDWIDTH;
static uint8_t spreadingFactor = DEFAULT_SPREADING_FACTOR;
static uint8_t codingRate = DEFAULT_CODING_RATE;
static int8_t txPower = DEFAULT_TX_POWER;
static uint16_t preambleLength = DEFAULT_PREAMBLE_LENGTH;
static uint8_t syncWord = DEFAULT_SYNC_WORD;
static bool crcEnabled = DEFAULT_CRC;
static String nodeName = DEFAULT_NODE_NAME;

static String readInput()
{
    String input;

    while (true) {
        while (Serial.available()) {
            char c = Serial.read();

            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    input.trim();
                    Serial.println();
                    return input;
                }
            } else {
                input += c;
                Serial.print(c);
            }
        }

        delay(10);
    }
}

static void loadDefaults()
{
    frequency = DEFAULT_FREQUENCY;
    bandwidth = DEFAULT_BANDWIDTH;
    spreadingFactor = DEFAULT_SPREADING_FACTOR;
    codingRate = DEFAULT_CODING_RATE;
    txPower = DEFAULT_TX_POWER;
    preambleLength = DEFAULT_PREAMBLE_LENGTH;
    syncWord = DEFAULT_SYNC_WORD;
    crcEnabled = DEFAULT_CRC;
}

static void saveConfiguration()
{
    preferences.begin("lora", false);

    preferences.putBool("configured", true);
    preferences.putBool("enabled", enabled);

    preferences.putFloat("frequency", frequency);
    preferences.putFloat("bandwidth", bandwidth);
    preferences.putUChar("sf", spreadingFactor);
    preferences.putUChar("cr", codingRate);
    preferences.putChar("txpower", txPower);
    preferences.putUShort("preamble", preambleLength);
    preferences.putUChar("syncword", syncWord);
    preferences.putBool("crc", crcEnabled);

    preferences.end();

    configured = true;
}

static void loadConfiguration()
{
    preferences.begin("lora", true);

    configured = preferences.getBool(
        "configured",
        false
    );

    if (configured) {
        enabled = preferences.getBool(
            "enabled",
            true
        );

        frequency = preferences.getFloat(
            "frequency",
            DEFAULT_FREQUENCY
        );

        bandwidth = preferences.getFloat(
            "bandwidth",
            DEFAULT_BANDWIDTH
        );

        spreadingFactor = preferences.getUChar(
            "sf",
            DEFAULT_SPREADING_FACTOR
        );

        codingRate = preferences.getUChar(
            "cr",
            DEFAULT_CODING_RATE
        );

        txPower = preferences.getChar(
            "txpower",
            DEFAULT_TX_POWER
        );

        preambleLength = preferences.getUShort(
            "preamble",
            DEFAULT_PREAMBLE_LENGTH
        );

        syncWord = preferences.getUChar(
            "syncword",
            DEFAULT_SYNC_WORD
        );

        crcEnabled = preferences.getBool(
            "crc",
            DEFAULT_CRC
        );
    }

    preferences.end();
}

static void printInitializationError()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("LORA RADIO INITIALIZATION ERROR");
    Serial.println("========================================");

    Serial.printf(
        "RadioLib Error Code : %d\n",
        radioError
    );

    Serial.println();
}

static void printConfigurationError()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("LORA RADIO CONFIGURATION ERROR");
    Serial.println("========================================");

    Serial.printf(
        "RadioLib Error Code : %d\n",
        radioError
    );

    Serial.println();
}

static bool applyRadioConfiguration()
{
    radioError = radio.setBandwidth(bandwidth);

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    radioError = radio.setSpreadingFactor(
        spreadingFactor
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    radioError = radio.setCodingRate(
        codingRate
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    radioError = radio.setSyncWord(
        syncWord
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    radioError = radio.setOutputPower(
        txPower
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    radioError = radio.setPreambleLength(
        preambleLength
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    radioError = radio.setCRC(
        crcEnabled ? 2 : 0
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        return false;
    }

    return true;
}

static bool initializeRadio()
{
    detected = false;
    ready = false;
    radioError = RADIOLIB_ERR_NONE;

    if (!configured) {
        return false;
    }

    /*
     * Use exactly the same SPI configuration as the
     * standalone test program that is known to work.
     */
    spi.begin(
        RADIO_SCK,
        RADIO_MISO,
        RADIO_MOSI,
        RADIO_NSS
    );

    /*
     * IMPORTANT:
     *
     * Do not use:
     *
     *     radio.XTAL = true;
     *
     * Do not use the long begin() overload with the
     * TCXO parameter.
     *
     * The known-good standalone test initializes the
     * SX1262 with the simple begin(frequency) call.
     */
    radioError = radio.begin(
        frequency
    );

    if (radioError != RADIOLIB_ERR_NONE) {
        detected = false;
        ready = false;

        printInitializationError();

        return false;
    }

    detected = true;

    /*
     * The module has successfully initialized.
     *
     * Now apply the remaining stored configuration.
     */
    if (!applyRadioConfiguration()) {
        ready = false;

        printConfigurationError();

        return false;
    }

    if (!enabled) {
        radioError = radio.sleep();

        ready = false;

        if (radioError != RADIOLIB_ERR_NONE) {
            printConfigurationError();
            return false;
        }

        return true;
    }

    ready = true;

    return true;
}

static void printConfiguration()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("GENERAL RADIO CONFIGURATION");
    Serial.println("========================================");

    Serial.printf(
        "Frequency        : %.3f MHz\n",
        frequency
    );

    Serial.printf(
        "Bandwidth        : %.1f kHz\n",
        bandwidth
    );

    Serial.printf(
        "Spreading Factor : SF%u\n",
        spreadingFactor
    );

    Serial.printf(
        "Coding Rate      : 4/%u\n",
        codingRate
    );

    Serial.printf(
        "TX Power         : %d dBm\n",
        txPower
    );

    Serial.printf(
        "Preamble Length  : %u\n",
        preambleLength
    );

    Serial.printf(
        "Sync Word        : 0x%02X\n",
        syncWord
    );

    Serial.printf(
        "CRC              : %s\n",
        crcEnabled ? "ENABLED" : "DISABLED"
    );

    Serial.println();
}

static void editFrequency()
{
    Serial.println();
    Serial.printf(
        "Enter frequency in MHz [%.3f]: ",
        frequency
    );

    String input = readInput();

    float value = input.toFloat();

    if (value <= 0.0f) {
        Serial.println("Invalid frequency.");
        return;
    }

    frequency = value;

    saveConfiguration();

    Serial.println("Frequency updated.");
}

static void editBandwidth()
{
    Serial.println();
    Serial.printf(
        "Enter bandwidth in kHz [%.1f]: ",
        bandwidth
    );

    String input = readInput();

    float value = input.toFloat();

    if (value <= 0.0f) {
        Serial.println("Invalid bandwidth.");
        return;
    }

    bandwidth = value;

    saveConfiguration();

    Serial.println("Bandwidth updated.");
}

static void editSpreadingFactor()
{
    Serial.println();
    Serial.printf(
        "Enter spreading factor [SF%u]: ",
        spreadingFactor
    );

    String input = readInput();

    int value = input.toInt();

    if (value < 5 || value > 12) {
        Serial.println(
            "Invalid spreading factor. Use 5-12."
        );
        return;
    }

    spreadingFactor = static_cast<uint8_t>(value);

    saveConfiguration();

    Serial.println("Spreading factor updated.");
}

static void editCodingRate()
{
    Serial.println();
    Serial.printf(
        "Enter coding rate denominator [4/%u]: ",
        codingRate
    );

    String input = readInput();

    int value = input.toInt();

    if (value < 5 || value > 8) {
        Serial.println(
            "Invalid coding rate. Use 5-8."
        );
        return;
    }

    codingRate = static_cast<uint8_t>(value);

    saveConfiguration();

    Serial.println("Coding rate updated.");
}

static void editTxPower()
{
    Serial.println();
    Serial.printf(
        "Enter TX power in dBm [%d]: ",
        txPower
    );

    String input = readInput();

    int value = input.toInt();

    if (value < -9 || value > 22) {
        Serial.println("Invalid TX power.");
        return;
    }

    txPower = static_cast<int8_t>(value);

    saveConfiguration();

    Serial.println("TX power updated.");
}

static void editPreambleLength()
{
    Serial.println();
    Serial.printf(
        "Enter preamble length [%u]: ",
        preambleLength
    );

    String input = readInput();

    int value = input.toInt();

    if (value < 1 || value > 65535) {
        Serial.println("Invalid preamble length.");
        return;
    }

    preambleLength = static_cast<uint16_t>(value);

    saveConfiguration();

    Serial.println("Preamble length updated.");
}

static void editSyncWord()
{
    Serial.println();
    Serial.printf(
        "Enter sync word in hexadecimal [0x%02X]: ",
        syncWord
    );

    String input = readInput();

    input.trim();

    if (
        input.startsWith("0x") ||
        input.startsWith("0X")
    ) {
        input = input.substring(2);
    }

    char *end = nullptr;

    unsigned long value = strtoul(
        input.c_str(),
        &end,
        16
    );

    if (
        end == input.c_str() ||
        *end != '\0' ||
        value > 0xFF
    ) {
        Serial.println("Invalid sync word.");
        return;
    }

    syncWord = static_cast<uint8_t>(value);

    saveConfiguration();

    Serial.println("Sync word updated.");
}

static void editCRC()
{
    Serial.println();
    Serial.println("CRC:");
    Serial.println("1. Enable");
    Serial.println("2. Disable");
    Serial.print("Select: ");

    String input = readInput();

    if (input == "1") {
        crcEnabled = true;
    } else if (input == "2") {
        crcEnabled = false;
    } else {
        Serial.println("Invalid selection.");
        return;
    }

    saveConfiguration();

    Serial.println("CRC setting updated.");
}

static void configureManually()
{
    while (true) {
        printConfiguration();

        Serial.println("1. Edit Frequency");
        Serial.println("2. Edit Bandwidth");
        Serial.println("3. Edit Spreading Factor");
        Serial.println("4. Edit Coding Rate");
        Serial.println("5. Edit TX Power");
        Serial.println("6. Edit Preamble Length");
        Serial.println("7. Edit Sync Word");
        Serial.println("8. Edit CRC");
        Serial.println("9. Use Default Values");
        Serial.println("X. Back");
        Serial.print("Select: ");

        String command = readInput();

        command.toUpperCase();

        if (command == "1") {
            editFrequency();
        } else if (command == "2") {
            editBandwidth();
        } else if (command == "3") {
            editSpreadingFactor();
        } else if (command == "4") {
            editCodingRate();
        } else if (command == "5") {
            editTxPower();
        } else if (command == "6") {
            editPreambleLength();
        } else if (command == "7") {
            editSyncWord();
        } else if (command == "8") {
            editCRC();
        } else if (command == "9") {
            loadDefaults();

            saveConfiguration();

            Serial.println();
            Serial.println("Default values restored.");
        } else if (command == "X") {
            return;
        } else {
            Serial.println("Invalid selection.");
        }
    }
}

static bool firstConfiguration()
{
    while (true) {
        Serial.println();
        Serial.println("========================================");
        Serial.println("GENERAL RADIO CONFIGURATION");
        Serial.println("========================================");
        Serial.println();
        Serial.println(
            "No general radio configuration was found."
        );
        Serial.println();
        Serial.println("1. Use Default Values");
        Serial.println("2. Configure Manually");
        Serial.println("X. Exit");
        Serial.print("Select: ");

        String command = readInput();

        command.toUpperCase();

        if (command == "1") {
            loadDefaults();

            enabled = true;

            saveConfiguration();

            Serial.println();
            Serial.println(
                "Default radio configuration saved."
            );

            return true;
        }

        if (command == "2") {
            loadDefaults();

            enabled = true;

            saveConfiguration();

            configureManually();

            return true;
        }

        if (command == "X") {
            return false;
        }

        Serial.println("Invalid selection.");
    }
}

static void showStatus()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("LORA RADIO STATUS");
    Serial.println("========================================");

    Serial.printf(
        "Radio Status : %s\n",
        enabled ? "ENABLED" : "DISABLED"
    );

    Serial.printf(
        "Radio Module : %s\n",
        detected ? "DETECTED" : "NOT DETECTED"
    );

    if (!enabled) {
        Serial.println("Radio State  : DISABLED");
    } else if (ready) {
        Serial.println("Radio State  : READY");
    } else {
        Serial.println("Radio State  : NOT READY");
    }

    Serial.printf(
        "Error Code   : %d\n",
        radioError
    );

    Serial.println();
}

static void showRadioMenu()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("LORA RADIO");
    Serial.println("========================================");

    Serial.printf(
        "Radio Status : %s\n",
        enabled ? "ENABLED" : "DISABLED"
    );

    Serial.printf(
        "Radio Module : %s\n",
        detected ? "DETECTED" : "NOT DETECTED"
    );

    if (!enabled) {
        Serial.println("Radio State  : DISABLED");
    } else if (ready) {
        Serial.println("Radio State  : READY");
    } else {
        Serial.println("Radio State  : NOT READY");
    }

    Serial.println();
    Serial.println("1. Enable Radio");
    Serial.println("2. Disable Radio");
    Serial.println("3. General Radio Configuration");
    Serial.println("4. Radio Status");
    Serial.println("X. Exit");
    Serial.print("Select: ");
}

void begin()
{
    loadConfiguration();

    if (!configured) {
        return;
    }

    initializeRadio();
}

void service()
{
}

bool isConfigured()
{
    return configured;
}

bool isEnabled()
{
    return enabled;
}

bool isDetected()
{
    return detected;
}

bool isReady()
{
    return ready;
}

void setEnabled(bool value)
{
    enabled = value;

    preferences.begin("lora", false);
    preferences.putBool("enabled", enabled);
    preferences.end();

    if (!configured) {
        detected = false;
        ready = false;
        return;
    }

    if (enabled) {
        initializeRadio();
    } else {
        if (detected) {
            radioError = radio.sleep();
        }

        ready = false;
    }
}

void feature()
{
    if (!configured) {
        if (!firstConfiguration()) {
            return;
        }

        initializeRadio();
    }

    while (true) {
        showRadioMenu();

        String command = readInput();

        command.toUpperCase();

        if (command == "1") {
            setEnabled(true);

            Serial.println();

            if (ready) {
                Serial.println("Radio enabled.");
            } else if (detected) {
                Serial.println(
                    "Radio detected but not ready."
                );
            } else {
                Serial.println(
                    "Radio could not be initialized."
                );
            }

        } else if (command == "2") {
            setEnabled(false);

            Serial.println();
            Serial.println("Radio disabled.");

        } else if (command == "3") {
            configureManually();

            /*
             * Reinitialize using the newly saved
             * configuration.
             */
            initializeRadio();

        } else if (command == "4") {
            showStatus();

        } else if (command == "X") {
            return;

        } else {
            Serial.println("Invalid selection.");
        }
    }
}

}