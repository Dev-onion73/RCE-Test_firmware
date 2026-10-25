#include <Arduino.h>
#include <ArduinoJson.h>
#include <RadioLib.h>

#include "RadioTest.h"
#include "Radio.h"

namespace Radio {
namespace RadioTest {

// ============================================================
// TEST CONFIGURATION STRUCTURE
// ============================================================

struct TestConfig {
    float frequencyMHz = 0.0f;
    uint8_t spreadingFactor = 0;
    float bandwidthKHz = 0.0f;
    uint8_t codingRate = 0;  // denominator (5-8 for 4/5, 4/6, 4/7, 4/8)
    int8_t txPowerDbm = 0;
    bool valid = false;
};

enum class TestRole {
    NONE,
    BROADCAST,
    RECEIVE
};

// ============================================================
// INPUT HELPER
// ============================================================

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

// ============================================================
// CODING RATE PARSER
// ============================================================

static bool parseCodingRate(const String& value, uint8_t& result)
{
    String trimmed = value;
    trimmed.trim();

    if (trimmed.length() == 1 && isDigit(trimmed[0])) {
        int denominator = trimmed.toInt();
        if (denominator >= 5 && denominator <= 8) {
            result = static_cast<uint8_t>(denominator);
            return true;
        }
    }

    if (trimmed.startsWith("4/")) {
        String denominatorText = trimmed.substring(2);
        int denominator = denominatorText.toInt();
        if (denominator >= 5 && denominator <= 8) {
            result = static_cast<uint8_t>(denominator);
            return true;
        }
    }

    return false;
}

// ============================================================
// JSON PARSING
// ============================================================

static bool parseTestConfig(const String& jsonInput, TestRole& role, TestConfig& config)
{
    JsonDocument document;

    DeserializationError error = deserializeJson(document, jsonInput);

    if (error) {
        Serial.println();
        Serial.println("JSON Parse Error:");
        Serial.printf("  %s\n", error.c_str());
        return false;
    }

    // Parse ROLE
    const char* roleStr = document["ROLE"];
    if (!roleStr) {
        Serial.println();
        Serial.println("Missing 'ROLE' field in JSON.");
        return false;
    }

    String roleString = roleStr;
    roleString.toUpperCase();

    if (roleString == "BROADCAST") {
        role = TestRole::BROADCAST;
    } else if (roleString == "RECEIVE") {
        role = TestRole::RECEIVE;
    } else {
        Serial.println();
        Serial.printf("Invalid ROLE: %s (must be BROADCAST or RECEIVE)\n", roleString.c_str());
        return false;
    }

    // Parse CONFIGURATION
    JsonObject cfg = document["CONFIGURATION"];
    if (cfg.isNull()) {
        Serial.println();
        Serial.println("Missing 'CONFIGURATION' object in JSON.");
        return false;
    }

    // FREQUENCY-MHZ
    if (!cfg.containsKey("FREQUENCY-MHZ")) {
        Serial.println();
        Serial.println("Missing 'FREQUENCY-MHZ' in CONFIGURATION.");
        return false;
    }
    config.frequencyMHz = cfg["FREQUENCY-MHZ"].as<float>();
    if (config.frequencyMHz <= 0.0f) {
        Serial.println();
        Serial.println("Invalid FREQUENCY-MHZ (must be > 0).");
        return false;
    }

    // SPREADING-FACTOR
    if (!cfg.containsKey("SPREADING-FACTOR")) {
        Serial.println();
        Serial.println("Missing 'SPREADING-FACTOR' in CONFIGURATION.");
        return false;
    }
    config.spreadingFactor = cfg["SPREADING-FACTOR"].as<uint8_t>();
    if (config.spreadingFactor < 5 || config.spreadingFactor > 12) {
        Serial.println();
        Serial.println("Invalid SPREADING-FACTOR (must be 5-12).");
        return false;
    }

    // BANDWIDTH-KHZ
    if (!cfg.containsKey("BANDWIDTH-KHZ")) {
        Serial.println();
        Serial.println("Missing 'BANDWIDTH-KHZ' in CONFIGURATION.");
        return false;
    }
    config.bandwidthKHz = cfg["BANDWIDTH-KHZ"].as<float>();
    if (config.bandwidthKHz != 125.0f && config.bandwidthKHz != 250.0f && config.bandwidthKHz != 500.0f) {
        Serial.println();
        Serial.println("Invalid BANDWIDTH-KHZ (must be 125, 250, or 500).");
        return false;
    }

    // CODING-RATE
    if (!cfg.containsKey("CODING-RATE")) {
        Serial.println();
        Serial.println("Missing 'CODING-RATE' in CONFIGURATION.");
        return false;
    }
    String crStr = cfg["CODING-RATE"].as<String>();
    if (!parseCodingRate(crStr, config.codingRate)) {
        Serial.println();
        Serial.println("Invalid CODING-RATE (must be 4/5, 4/6, 4/7, or 4/8).");
        return false;
    }

    // TX-POWER-DBM (only required for BROADCAST)
    if (role == TestRole::BROADCAST) {
        if (!cfg.containsKey("TX-POWER-DBM")) {
            Serial.println();
            Serial.println("Missing 'TX-POWER-DBM' in CONFIGURATION (required for BROADCAST).");
            return false;
        }
        config.txPowerDbm = cfg["TX-POWER-DBM"].as<int8_t>();
        if (config.txPowerDbm < -9 || config.txPowerDbm > 22) {
            Serial.println();
            Serial.println("Invalid TX-POWER-DBM (must be -9 to 22).");
            return false;
        }
    } else {
        config.txPowerDbm = 14;  // Default for receive
    }

    config.valid = true;
    return true;
}

// ============================================================
// SAVE/RESTORE ORIGINAL RADIO CONFIG
// ============================================================

struct RadioConfigBackup {
    float frequency;
    float bandwidth;
    uint8_t spreadingFactor;
    uint8_t codingRate;
    int8_t txPower;
};

static void backupRadioConfig(RadioConfigBackup& backup)
{
    backup.frequency = Radio::getFrequency();
    backup.bandwidth = Radio::getBandwidth();
    backup.spreadingFactor = Radio::getSpreadingFactor();
    backup.codingRate = Radio::getCodingRate();
    backup.txPower = Radio::getTxPower();
}

static void restoreRadioConfig(const RadioConfigBackup& backup)
{
    Radio::setFrequency(backup.frequency);
    Radio::setBandwidth(backup.bandwidth);
    Radio::setSpreadingFactor(backup.spreadingFactor);
    Radio::setCodingRate(backup.codingRate);
    Radio::setTxPower(backup.txPower);
}

static void applyTestConfigToRadio(const TestConfig& config)
{
    Radio::setFrequency(config.frequencyMHz);
    Radio::setBandwidth(config.bandwidthKHz);
    Radio::setSpreadingFactor(config.spreadingFactor);
    Radio::setCodingRate(config.codingRate);
    Radio::setTxPower(config.txPowerDbm);
}

// ============================================================
// REINITIALIZE RADIO
// ============================================================

static bool reinitializeRadio()
{
    // This calls the internal initializeRadio() logic
    // We use the public setEnabled(false) then setEnabled(true)
    // which internally calls initializeRadio()
    
    bool wasEnabled = Radio::isEnabled();
    Radio::setEnabled(false);
    delay(100);
    Radio::setEnabled(true);
    
    return Radio::isReady();
}

// ============================================================
// TEST BROADCAST
// ============================================================

static void runTestBroadcast()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("TEST BROADCAST MODE");
    Serial.println("========================================");
    Serial.println("Press X then Enter to exit.");
    Serial.println();

    while (true) {
        // Check for exit command
        if (Serial.available()) {
            String command = Serial.readStringUntil('\n');
            command.trim();
            command.toUpperCase();
            if (command == "X") {
                break;
            }
        }

        // Create and transmit broadcast packet using Radio's function
        String packet = Radio::createBroadcastPacket();

        // Transmit using Radio's accessor function
        bool success = Radio::transmitPacket(packet);

        if (!success) {
            Serial.println();
            Serial.println("Broadcast transmission failed.");
        } else {
            uint32_t seqNo = Radio::getBroadcastSequence();
            Serial.println();
            Serial.println("========================================");
            Serial.println("TEST BROADCAST TRANSMISSION");
            Serial.println("========================================");
            Serial.printf("SEQ-NO : %lu\n", static_cast<unsigned long>(seqNo));
            Serial.printf("Packet : %s\n", packet.c_str());
            Serial.println();
        }

        Radio::incrementBroadcastSequence();
        delay(1000);  // 1 second interval for test mode
    }

    Radio::stopReceiveMode();  // This will put radio to sleep
    Serial.println();
    Serial.println("Test broadcast mode stopped.");
}

// ============================================================
// TEST RECEIVE
// ============================================================

static volatile bool testPacketReceived = false;

static void setTestPacketReceivedFlag()
{
    testPacketReceived = true;
}

static void runTestReceive()
{
    testPacketReceived = false;

    // Install receive callback using Radio's accessor function
    Radio::setPacketReceivedCallback(setTestPacketReceivedFlag);

    // Start receive mode using Radio's accessor function
    if (!Radio::startReceiveMode()) {
        Serial.println();
        Serial.println("Failed to start receiver.");
        return;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("TEST RECEIVE MODE");
    Serial.println("========================================");
    Serial.println("Listening for packets...");
    Serial.println("Press X then Enter to exit.");
    Serial.println();

    while (true) {
        // Check for exit command
        if (Serial.available()) {
            String command = Serial.readStringUntil('\n');
            command.trim();
            command.toUpperCase();
            if (command == "X") {
                break;
            }
        }

        // Check for received packet
        if (testPacketReceived) {
            testPacketReceived = false;

            String receptionTimestamp = Radio::getUTCTimestamp();

            String receivedPacket;
            float receivedRSSI = 0.0f;
            float receivedSNR = 0.0f;

            // Read received packet using Radio's accessor function
            if (!Radio::readReceivedPacket(receivedPacket, receivedRSSI, receivedSNR)) {
                Serial.println();
                Serial.println("Failed to read received packet.");
                Radio::startReceiveMode();  // Restart receive
                continue;
            }

            Serial.println();
            Serial.println("========================================");
            Serial.println("TEST RECEIVED PACKET");
            Serial.println("========================================");

            JsonDocument document;
            DeserializationError error = deserializeJson(document, receivedPacket);

            if (error) {
                Serial.println("Invalid JSON payload.");
                Serial.printf("JSON Error: %s\n", error.c_str());
                Serial.println();
                Serial.println("Raw Payload:");
                Serial.println(receivedPacket);
            } else {
                // Display the JSON using Radio's function
                Radio::displayJson(document.as<JsonVariantConst>(), "");
            }

            Serial.printf("RECEPTION-TIMESTAMP : %s\n", receptionTimestamp.c_str());
            Serial.printf("RSSI : %.2f dBm\n", receivedRSSI);
            Serial.printf("SNR  : %.2f dB\n", receivedSNR);
            Serial.println();

            // Restart receive mode
            Radio::startReceiveMode();
        }

        delay(10);
    }

    Radio::stopReceiveMode();
    Serial.println();
    Serial.println("Test receive mode stopped.");
}

// ============================================================
// MAIN FEATURE
// ============================================================

void begin()
{
    // Nothing to initialize for RadioTest
}

void feature()
{
    // Check if radio is configured and enabled
    if (!Radio::isConfigured()) {
        Serial.println();
        Serial.println("Radio is not configured.");
        Serial.println("Please configure the radio first (Option 3).");
        return;
    }

    if (!Radio::isEnabled()) {
        Serial.println();
        Serial.println("Radio is disabled.");
        Serial.println("Please enable the radio first (Option 1).");
        return;
    }

    if (!Radio::isReady()) {
        Serial.println();
        Serial.println("Radio is not ready.");
        return;
    }

    while (true) {
        Serial.println();
        Serial.println("========================================");
        Serial.println("RADIO TEST");
        Serial.println("========================================");
        Serial.println();
        Serial.println("Send JSON configuration to run a test.");
        Serial.println();
        Serial.println("BROADCAST example:");
        Serial.println("{");
        Serial.println("  \"ROLE\": \"BROADCAST\",");
        Serial.println("  \"CONFIGURATION\": {");
        Serial.println("    \"FREQUENCY-MHZ\": 865.5,");
        Serial.println("    \"SPREADING-FACTOR\": 7,");
        Serial.println("    \"BANDWIDTH-KHZ\": 125,");
        Serial.println("    \"CODING-RATE\": \"4/5\",");
        Serial.println("    \"TX-POWER-DBM\": 14");
        Serial.println("  }");
        Serial.println("}");
        Serial.println();
        Serial.println("RECEIVE example:");
        Serial.println("{");
        Serial.println("  \"ROLE\": \"RECEIVE\",");
        Serial.println("  \"CONFIGURATION\": {");
        Serial.println("    \"FREQUENCY-MHZ\": 865.5,");
        Serial.println("    \"SPREADING-FACTOR\": 7,");
        Serial.println("    \"BANDWIDTH-KHZ\": 125,");
        Serial.println("    \"CODING-RATE\": \"4/5\"");
        Serial.println("  }");
        Serial.println("}");
        Serial.println();
        Serial.println("Enter JSON (or X to exit):");

        String input = readInput();

        if (input.equalsIgnoreCase("X")) {
            return;
        }

        if (input.length() == 0) {
            continue;
        }

        TestRole role = TestRole::NONE;
        TestConfig config;

        if (!parseTestConfig(input, role, config)) {
            continue;
        }

        // Backup original radio configuration
        RadioConfigBackup backup;
        backupRadioConfig(backup);

        // Apply test configuration
        Serial.println();
        Serial.println("Applying test configuration...");

        applyTestConfigToRadio(config);

        // Reinitialize radio with new config
        if (!reinitializeRadio()) {
            Serial.println("Failed to apply test configuration.");
            restoreRadioConfig(backup);
            reinitializeRadio();  // Try to restore
            continue;
        }

        Serial.println("Test configuration applied successfully.");
        Serial.println();

        // Run the test based on role
        if (role == TestRole::BROADCAST) {
            runTestBroadcast();
        } else if (role == TestRole::RECEIVE) {
            runTestReceive();
        }

        // Restore original configuration after test
        Serial.println("Restoring original radio configuration...");
        restoreRadioConfig(backup);
        reinitializeRadio();
    }
}

} // namespace RadioTest
} // namespace Radio