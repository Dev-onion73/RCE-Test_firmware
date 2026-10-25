#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "TestMatrix.h"

namespace Radio {
namespace TestMatrix {

static constexpr const char* MATRIX_FILE =
    "/rce_test_matrix.json";

static Configuration configuration;

static bool initialized = false;

// ============================================================
// INPUT
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
// CONFIGURATION HELPERS
// ============================================================

static void clearConfiguration()
{
    configuration.frequencyCount = 0;
    configuration.spreadingFactorCount = 0;
    configuration.bandwidthCount = 0;
    configuration.codingRateCount = 0;
    configuration.txPowerCount = 0;
}

static void loadDefaultConfiguration()
{
    clearConfiguration();

    const float defaultFrequency[] = {
        865.0625f,
        865.5000f,
        866.0000f,
        866.5000f,
        866.9375f,
        867.0000f,
        867.5000f
    };

    const uint8_t defaultSpreadingFactor[] = {
        7,
        8,
        9,
        10,
        11,
        12
    };

    const float defaultBandwidth[] = {
        125.0f,
        250.0f,
        500.0f
    };

    const uint8_t defaultCodingRate[] = {
        5,
        6,
        7,
        8
    };

    const int8_t defaultTxPower[] = {
        11,
        14,
        17,
        20
    };

    configuration.frequencyCount =
        sizeof(defaultFrequency) /
        sizeof(defaultFrequency[0]);

    configuration.spreadingFactorCount =
        sizeof(defaultSpreadingFactor) /
        sizeof(defaultSpreadingFactor[0]);

    configuration.bandwidthCount =
        sizeof(defaultBandwidth) /
        sizeof(defaultBandwidth[0]);

    configuration.codingRateCount =
        sizeof(defaultCodingRate) /
        sizeof(defaultCodingRate[0]);

    configuration.txPowerCount =
        sizeof(defaultTxPower) /
        sizeof(defaultTxPower[0]);

    for (size_t i = 0;
         i < configuration.frequencyCount;
         i++) {

        configuration.frequency[i] =
            defaultFrequency[i];
    }

    for (size_t i = 0;
         i < configuration.spreadingFactorCount;
         i++) {

        configuration.spreadingFactor[i] =
            defaultSpreadingFactor[i];
    }

    for (size_t i = 0;
         i < configuration.bandwidthCount;
         i++) {

        configuration.bandwidth[i] =
            defaultBandwidth[i];
    }

    for (size_t i = 0;
         i < configuration.codingRateCount;
         i++) {

        configuration.codingRate[i] =
            defaultCodingRate[i];
    }

    for (size_t i = 0;
         i < configuration.txPowerCount;
         i++) {

        configuration.txPower[i] =
            defaultTxPower[i];
    }
}

// ============================================================
// CODING RATE PARSER
// ============================================================

static bool parseCodingRate(
    JsonVariantConst value,
    uint8_t& result
)
{
    // JSON numeric representation:
    //
    // 5
    // 6
    // 7
    // 8
    //
    if (value.is<int>()) {

        int denominator =
            value.as<int>();

        if (denominator < 5 ||
            denominator > 8) {

            return false;
        }

        result =
            static_cast<uint8_t>(
                denominator
            );

        return true;
    }

    // JSON string representation:
    //
    // "4/5"
    // "4/6"
    // "4/7"
    // "4/8"
    //
    if (value.is<const char*>()) {

        const char* raw =
            value.as<const char*>();

        if (raw == nullptr) {
            return false;
        }

        String text = raw;
        text.trim();

        if (!text.startsWith("4/")) {
            return false;
        }

        String denominatorText =
            text.substring(2);

        int denominator =
            denominatorText.toInt();

        if (denominator < 5 ||
            denominator > 8) {

            return false;
        }

        result =
            static_cast<uint8_t>(
                denominator
            );

        return true;
    }

    return false;
}

// ============================================================
// JSON -> CONFIGURATION
// ============================================================

static bool loadFromDocument(
    JsonDocument& document
)
{
    JsonVariant root =
        document["test_matrix"];

    if (root.isNull()) {
        return false;
    }

    clearConfiguration();

    // --------------------------------------------------------
    // FREQUENCY
    // --------------------------------------------------------

    JsonArray frequencyValues =
        root["frequency"]["values"]
            .as<JsonArray>();

    if (frequencyValues.isNull()) {
        return false;
    }

    for (JsonVariant value :
         frequencyValues) {

        if (configuration.frequencyCount >=
            MAX_VALUES) {

            break;
        }

        float frequency =
            value.as<float>();

        if (frequency <= 0.0f) {
            continue;
        }

        configuration.frequency[
            configuration.frequencyCount
        ] = frequency;

        configuration.frequencyCount++;
    }

    // --------------------------------------------------------
    // SPREADING FACTOR
    // --------------------------------------------------------

    JsonArray spreadingFactorValues =
        root["spreading_factor"]["values"]
            .as<JsonArray>();

    if (spreadingFactorValues.isNull()) {
        return false;
    }

    for (JsonVariant value :
         spreadingFactorValues) {

        if (configuration.spreadingFactorCount >=
            MAX_VALUES) {

            break;
        }

        int sf =
            value.as<int>();

        if (sf < 5 ||
            sf > 12) {

            continue;
        }

        configuration.spreadingFactor[
            configuration.spreadingFactorCount
        ] = static_cast<uint8_t>(sf);

        configuration.spreadingFactorCount++;
    }

    // --------------------------------------------------------
    // BANDWIDTH
    // --------------------------------------------------------

    JsonArray bandwidthValues =
        root["bandwidth"]["values"]
            .as<JsonArray>();

    if (bandwidthValues.isNull()) {
        return false;
    }

    for (JsonVariant value :
         bandwidthValues) {

        if (configuration.bandwidthCount >=
            MAX_VALUES) {

            break;
        }

        float bandwidth =
            value.as<float>();

        if (bandwidth <= 0.0f) {
            continue;
        }

        configuration.bandwidth[
            configuration.bandwidthCount
        ] = bandwidth;

        configuration.bandwidthCount++;
    }

    // --------------------------------------------------------
    // CODING RATE
    // --------------------------------------------------------

    JsonArray codingRateValues =
        root["coding_rate"]["values"]
            .as<JsonArray>();

    if (codingRateValues.isNull()) {
        return false;
    }

    for (JsonVariant value :
         codingRateValues) {

        if (configuration.codingRateCount >=
            MAX_VALUES) {

            break;
        }

        uint8_t codingRate = 0;

        if (!parseCodingRate(
                value,
                codingRate)) {

            continue;
        }

        configuration.codingRate[
            configuration.codingRateCount
        ] = codingRate;

        configuration.codingRateCount++;
    }

    // --------------------------------------------------------
    // TX POWER
    // --------------------------------------------------------

    JsonArray txPowerValues =
        root["tx_power"]["values"]
            .as<JsonArray>();

    if (txPowerValues.isNull()) {
        return false;
    }

    for (JsonVariant value :
         txPowerValues) {

        if (configuration.txPowerCount >=
            MAX_VALUES) {

            break;
        }

        int power =
            value.as<int>();

        if (power < -9 ||
            power > 22) {

            continue;
        }

        configuration.txPower[
            configuration.txPowerCount
        ] = static_cast<int8_t>(power);

        configuration.txPowerCount++;
    }

    // --------------------------------------------------------
    // ALL DIMENSIONS MUST CONTAIN VALUES
    // --------------------------------------------------------

    return (
        configuration.frequencyCount > 0 &&
        configuration.spreadingFactorCount > 0 &&
        configuration.bandwidthCount > 0 &&
        configuration.codingRateCount > 0 &&
        configuration.txPowerCount > 0
    );
}

// ============================================================
// CONFIGURATION -> JSON
// ============================================================

static void buildDocument(
    JsonDocument& document
)
{
    JsonObject matrix =
        document["test_matrix"]
            .to<JsonObject>();

    // --------------------------------------------------------
    // FREQUENCY
    // --------------------------------------------------------

    JsonObject frequency =
        matrix["frequency"]
            .to<JsonObject>();

    JsonArray frequencyValues =
        frequency["values"]
            .to<JsonArray>();

    for (size_t i = 0;
         i < configuration.frequencyCount;
         i++) {

        frequencyValues.add(
            configuration.frequency[i]
        );
    }

    // --------------------------------------------------------
    // SPREADING FACTOR
    // --------------------------------------------------------

    JsonObject spreadingFactor =
        matrix["spreading_factor"]
            .to<JsonObject>();

    JsonArray spreadingFactorValues =
        spreadingFactor["values"]
            .to<JsonArray>();

    for (size_t i = 0;
         i < configuration.spreadingFactorCount;
         i++) {

        spreadingFactorValues.add(
            configuration.spreadingFactor[i]
        );
    }

    // --------------------------------------------------------
    // BANDWIDTH
    // --------------------------------------------------------

    JsonObject bandwidth =
        matrix["bandwidth"]
            .to<JsonObject>();

    JsonArray bandwidthValues =
        bandwidth["values"]
            .to<JsonArray>();

    for (size_t i = 0;
         i < configuration.bandwidthCount;
         i++) {

        bandwidthValues.add(
            configuration.bandwidth[i]
        );
    }

    // --------------------------------------------------------
    // CODING RATE
    // --------------------------------------------------------

    JsonObject codingRate =
        matrix["coding_rate"]
            .to<JsonObject>();

    JsonArray codingRateValues =
        codingRate["values"]
            .to<JsonArray>();

    for (size_t i = 0;
         i < configuration.codingRateCount;
         i++) {

        String value =
            "4/" +
            String(
                configuration.codingRate[i]
            );

        codingRateValues.add(value);
    }

    // --------------------------------------------------------
    // TX POWER
    // --------------------------------------------------------

    JsonObject txPower =
        matrix["tx_power"]
            .to<JsonObject>();

    JsonArray txPowerValues =
        txPower["values"]
            .to<JsonArray>();

    for (size_t i = 0;
         i < configuration.txPowerCount;
         i++) {

        txPowerValues.add(
            configuration.txPower[i]
        );
    }
}

// ============================================================
// LOAD JSON FILE
// ============================================================

bool load()
{
    if (!LittleFS.exists(MATRIX_FILE)) {

        Serial.println();
        Serial.println(
            "RCE Test Matrix JSON file not found."
        );

        return false;
    }

    File file =
        LittleFS.open(
            MATRIX_FILE,
            "r"
        );

    if (!file) {

        Serial.println();
        Serial.println(
            "Failed to open RCE Test Matrix JSON."
        );

        return false;
    }

    JsonDocument document;

    DeserializationError error =
        deserializeJson(
            document,
            file
        );

    file.close();

    if (error) {

        Serial.println();
        Serial.println(
            "Failed to parse RCE Test Matrix JSON."
        );

        Serial.printf(
            "JSON Error : %s\n",
            error.c_str()
        );

        return false;
    }

    if (!loadFromDocument(document)) {

        Serial.println();
        Serial.println(
            "Invalid RCE Test Matrix configuration."
        );

        return false;
    }

    Serial.println();
    Serial.println(
        "RCE Test Matrix JSON loaded."
    );

    return true;
}

// ============================================================
// SAVE JSON FILE
// ============================================================

bool save()
{
    JsonDocument document;

    buildDocument(document);

    File file =
        LittleFS.open(
            MATRIX_FILE,
            "w"
        );

    if (!file) {

        Serial.println();
        Serial.println(
            "Failed to open RCE Test Matrix JSON for writing."
        );

        return false;
    }

    size_t written =
        serializeJsonPretty(
            document,
            file
        );

    file.close();

    if (written == 0) {

        Serial.println();
        Serial.println(
            "Failed to save RCE Test Matrix JSON."
        );

        return false;
    }

    Serial.println();
    Serial.println(
        "RCE Test Matrix JSON saved."
    );

    return true;
}

// ============================================================
// CONFIGURATION COUNT
// ============================================================

uint64_t configurationCount()
{
    if (
        configuration.frequencyCount == 0 ||
        configuration.spreadingFactorCount == 0 ||
        configuration.bandwidthCount == 0 ||
        configuration.codingRateCount == 0 ||
        configuration.txPowerCount == 0
    ) {
        return 0;
    }

    uint64_t total = 1;

    total *= configuration.frequencyCount;
    total *= configuration.spreadingFactorCount;
    total *= configuration.bandwidthCount;
    total *= configuration.codingRateCount;
    total *= configuration.txPowerCount;

    return total;
}

// ============================================================
// GET CONFIGURATION
// ============================================================

const Configuration& getConfiguration()
{
    return configuration;
}

// ============================================================
// PRINT FREQUENCY SET
// ============================================================

static void printFrequencySet()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "FREQUENCY SET"
    );
    Serial.println(
        "========================================"
    );

    if (configuration.frequencyCount == 0) {
        Serial.println(
            "No values configured."
        );
    }

    for (size_t i = 0;
         i < configuration.frequencyCount;
         i++) {

        Serial.printf(
            "%u. %.4f MHz\n",
            static_cast<unsigned int>(i + 1),
            configuration.frequency[i]
        );
    }

    Serial.println();
}

// ============================================================
// PRINT SPREADING FACTOR SET
// ============================================================

static void printSpreadingFactorSet()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "SPREADING FACTOR SET"
    );
    Serial.println(
        "========================================"
    );

    if (configuration.spreadingFactorCount == 0) {
        Serial.println(
            "No values configured."
        );
    }

    for (size_t i = 0;
         i < configuration.spreadingFactorCount;
         i++) {

        Serial.printf(
            "%u. SF%u\n",
            static_cast<unsigned int>(i + 1),
            configuration.spreadingFactor[i]
        );
    }

    Serial.println();
}

// ============================================================
// PRINT BANDWIDTH SET
// ============================================================

static void printBandwidthSet()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "BANDWIDTH SET"
    );
    Serial.println(
        "========================================"
    );

    if (configuration.bandwidthCount == 0) {
        Serial.println(
            "No values configured."
        );
    }

    for (size_t i = 0;
         i < configuration.bandwidthCount;
         i++) {

        Serial.printf(
            "%u. %.1f kHz\n",
            static_cast<unsigned int>(i + 1),
            configuration.bandwidth[i]
        );
    }

    Serial.println();
}

// ============================================================
// PRINT CODING RATE SET
// ============================================================

static void printCodingRateSet()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "CODING RATE SET"
    );
    Serial.println(
        "========================================"
    );

    if (configuration.codingRateCount == 0) {
        Serial.println(
            "No values configured."
        );
    }

    for (size_t i = 0;
         i < configuration.codingRateCount;
         i++) {

        Serial.printf(
            "%u. 4/%u\n",
            static_cast<unsigned int>(i + 1),
            configuration.codingRate[i]
        );
    }

    Serial.println();
}

// ============================================================
// PRINT TX POWER SET
// ============================================================

static void printTxPowerSet()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "TX POWER SET"
    );
    Serial.println(
        "========================================"
    );

    if (configuration.txPowerCount == 0) {
        Serial.println(
            "No values configured."
        );
    }

    for (size_t i = 0;
         i < configuration.txPowerCount;
         i++) {

        Serial.printf(
            "%u. %d dBm\n",
            static_cast<unsigned int>(i + 1),
            configuration.txPower[i]
        );
    }

    Serial.println();
}

// ============================================================
// FREQUENCY
// ============================================================

static void addFrequency()
{
    if (configuration.frequencyCount >= MAX_VALUES) {

        Serial.println(
            "Maximum number of values reached."
        );

        return;
    }

    Serial.print(
        "Enter frequency in MHz: "
    );

    String input =
        readInput();

    float value =
        input.toFloat();

    if (value <= 0.0f) {

        Serial.println(
            "Invalid frequency."
        );

        return;
    }

    configuration.frequency[
        configuration.frequencyCount
    ] = value;

    configuration.frequencyCount++;

    Serial.println(
        "Frequency added."
    );
}

static void removeFrequency()
{
    if (configuration.frequencyCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printFrequencySet();

    Serial.print(
        "Enter value number to remove: "
    );

    String input =
        readInput();

    int index =
        input.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.frequencyCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    for (size_t i = index;
         i + 1 < configuration.frequencyCount;
         i++) {

        configuration.frequency[i] =
            configuration.frequency[i + 1];
    }

    configuration.frequencyCount--;

    Serial.println(
        "Frequency removed."
    );
}

static void editFrequencyValue()
{
    if (configuration.frequencyCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printFrequencySet();

    Serial.print(
        "Enter value number to edit: "
    );

    String indexInput =
        readInput();

    int index =
        indexInput.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.frequencyCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    Serial.print(
        "Enter new frequency in MHz: "
    );

    String valueInput =
        readInput();

    float value =
        valueInput.toFloat();

    if (value <= 0.0f) {

        Serial.println(
            "Invalid frequency."
        );

        return;
    }

    configuration.frequency[index] =
        value;

    Serial.println(
        "Frequency updated."
    );
}

static void configureFrequencySet()
{
    while (true) {

        printFrequencySet();

        Serial.println("1. Add Value");
        Serial.println("2. Remove Value");
        Serial.println("3. Edit Value");
        Serial.println("4. Clear Set");
        Serial.println("5. Restore Default Set");
        Serial.println("X. Back");
        Serial.print("Select: ");

        String command =
            readInput();

        command.toUpperCase();

        if (command == "1") {

            addFrequency();

        } else if (command == "2") {

            removeFrequency();

        } else if (command == "3") {

            editFrequencyValue();

        } else if (command == "4") {

            configuration.frequencyCount = 0;

            Serial.println(
                "Frequency set cleared."
            );

        } else if (command == "5") {

            const float defaults[] = {
                865.0625f,
                865.5000f,
                866.0000f,
                866.5000f,
                866.9375f,
                867.0000f,
                867.5000f
            };

            configuration.frequencyCount =
                sizeof(defaults) /
                sizeof(defaults[0]);

            for (size_t i = 0;
                 i < configuration.frequencyCount;
                 i++) {

                configuration.frequency[i] =
                    defaults[i];
            }

            Serial.println(
                "Frequency default set restored."
            );

        } else if (command == "X") {

            return;

        } else {

            Serial.println(
                "Invalid selection."
            );
        }
    }
}

// ============================================================
// SPREADING FACTOR
// ============================================================

static void addSpreadingFactor()
{
    if (
        configuration.spreadingFactorCount >=
        MAX_VALUES
    ) {

        Serial.println(
            "Maximum number of values reached."
        );

        return;
    }

    Serial.print(
        "Enter spreading factor: "
    );

    String input =
        readInput();

    int value =
        input.toInt();

    if (value < 5 ||
        value > 12) {

        Serial.println(
            "Invalid spreading factor. Use 5-12."
        );

        return;
    }

    configuration.spreadingFactor[
        configuration.spreadingFactorCount
    ] = static_cast<uint8_t>(value);

    configuration.spreadingFactorCount++;

    Serial.println(
        "Spreading factor added."
    );
}

static void removeSpreadingFactor()
{
    if (configuration.spreadingFactorCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printSpreadingFactorSet();

    Serial.print(
        "Enter value number to remove: "
    );

    String input =
        readInput();

    int index =
        input.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.spreadingFactorCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    for (size_t i = index;
         i + 1 < configuration.spreadingFactorCount;
         i++) {

        configuration.spreadingFactor[i] =
            configuration.spreadingFactor[i + 1];
    }

    configuration.spreadingFactorCount--;

    Serial.println(
        "Spreading factor removed."
    );
}

static void editSpreadingFactorValue()
{
    if (configuration.spreadingFactorCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printSpreadingFactorSet();

    Serial.print(
        "Enter value number to edit: "
    );

    String indexInput =
        readInput();

    int index =
        indexInput.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.spreadingFactorCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    Serial.print(
        "Enter new spreading factor: "
    );

    String valueInput =
        readInput();

    int value =
        valueInput.toInt();

    if (value < 5 ||
        value > 12) {

        Serial.println(
            "Invalid spreading factor."
        );

        return;
    }

    configuration.spreadingFactor[index] =
        static_cast<uint8_t>(value);

    Serial.println(
        "Spreading factor updated."
    );
}

static void configureSpreadingFactorSet()
{
    while (true) {

        printSpreadingFactorSet();

        Serial.println("1. Add Value");
        Serial.println("2. Remove Value");
        Serial.println("3. Edit Value");
        Serial.println("4. Clear Set");
        Serial.println("5. Restore Default Set");
        Serial.println("X. Back");
        Serial.print("Select: ");

        String command =
            readInput();

        command.toUpperCase();

        if (command == "1") {

            addSpreadingFactor();

        } else if (command == "2") {

            removeSpreadingFactor();

        } else if (command == "3") {

            editSpreadingFactorValue();

        } else if (command == "4") {

            configuration.spreadingFactorCount = 0;

            Serial.println(
                "Spreading factor set cleared."
            );

        } else if (command == "5") {

            const uint8_t defaults[] = {
                7,
                8,
                9,
                10,
                11,
                12
            };

            configuration.spreadingFactorCount =
                sizeof(defaults) /
                sizeof(defaults[0]);

            for (size_t i = 0;
                 i < configuration.spreadingFactorCount;
                 i++) {

                configuration.spreadingFactor[i] =
                    defaults[i];
            }

            Serial.println(
                "Spreading factor default set restored."
            );

        } else if (command == "X") {

            return;

        } else {

            Serial.println(
                "Invalid selection."
            );
        }
    }
}

// ============================================================
// BANDWIDTH
// ============================================================

static void addBandwidth()
{
    if (configuration.bandwidthCount >= MAX_VALUES) {

        Serial.println(
            "Maximum number of values reached."
        );

        return;
    }

    Serial.print(
        "Enter bandwidth in kHz: "
    );

    String input =
        readInput();

    float value =
        input.toFloat();

    if (value <= 0.0f) {

        Serial.println(
            "Invalid bandwidth."
        );

        return;
    }

    configuration.bandwidth[
        configuration.bandwidthCount
    ] = value;

    configuration.bandwidthCount++;

    Serial.println(
        "Bandwidth added."
    );
}

static void removeBandwidth()
{
    if (configuration.bandwidthCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printBandwidthSet();

    Serial.print(
        "Enter value number to remove: "
    );

    String input =
        readInput();

    int index =
        input.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.bandwidthCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    for (size_t i = index;
         i + 1 < configuration.bandwidthCount;
         i++) {

        configuration.bandwidth[i] =
            configuration.bandwidth[i + 1];
    }

    configuration.bandwidthCount--;

    Serial.println(
        "Bandwidth removed."
    );
}

static void editBandwidthValue()
{
    if (configuration.bandwidthCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printBandwidthSet();

    Serial.print(
        "Enter value number to edit: "
    );

    String indexInput =
        readInput();

    int index =
        indexInput.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.bandwidthCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    Serial.print(
        "Enter new bandwidth in kHz: "
    );

    String valueInput =
        readInput();

    float value =
        valueInput.toFloat();

    if (value <= 0.0f) {

        Serial.println(
            "Invalid bandwidth."
        );

        return;
    }

    configuration.bandwidth[index] =
        value;

    Serial.println(
        "Bandwidth updated."
    );
}

static void configureBandwidthSet()
{
    while (true) {

        printBandwidthSet();

        Serial.println("1. Add Value");
        Serial.println("2. Remove Value");
        Serial.println("3. Edit Value");
        Serial.println("4. Clear Set");
        Serial.println("5. Restore Default Set");
        Serial.println("X. Back");
        Serial.print("Select: ");

        String command =
            readInput();

        command.toUpperCase();

        if (command == "1") {

            addBandwidth();

        } else if (command == "2") {

            removeBandwidth();

        } else if (command == "3") {

            editBandwidthValue();

        } else if (command == "4") {

            configuration.bandwidthCount = 0;

            Serial.println(
                "Bandwidth set cleared."
            );

        } else if (command == "5") {

            const float defaults[] = {
                125.0f,
                250.0f,
                500.0f
            };

            configuration.bandwidthCount =
                sizeof(defaults) /
                sizeof(defaults[0]);

            for (size_t i = 0;
                 i < configuration.bandwidthCount;
                 i++) {

                configuration.bandwidth[i] =
                    defaults[i];
            }

            Serial.println(
                "Bandwidth default set restored."
            );

        } else if (command == "X") {

            return;

        } else {

            Serial.println(
                "Invalid selection."
            );
        }
    }
}

// ============================================================
// CODING RATE
// ============================================================

static void addCodingRate()
{
    if (configuration.codingRateCount >= MAX_VALUES) {

        Serial.println(
            "Maximum number of values reached."
        );

        return;
    }

    Serial.print(
        "Enter coding rate denominator [5-8]: "
    );

    String input =
        readInput();

    int value =
        input.toInt();

    if (value < 5 ||
        value > 8) {

        Serial.println(
            "Invalid coding rate."
        );

        return;
    }

    configuration.codingRate[
        configuration.codingRateCount
    ] = static_cast<uint8_t>(value);

    configuration.codingRateCount++;

    Serial.println(
        "Coding rate added."
    );
}

static void removeCodingRate()
{
    if (configuration.codingRateCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printCodingRateSet();

    Serial.print(
        "Enter value number to remove: "
    );

    String input =
        readInput();

    int index =
        input.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.codingRateCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    for (size_t i = index;
         i + 1 < configuration.codingRateCount;
         i++) {

        configuration.codingRate[i] =
            configuration.codingRate[i + 1];
    }

    configuration.codingRateCount--;

    Serial.println(
        "Coding rate removed."
    );
}

static void editCodingRateValue()
{
    if (configuration.codingRateCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printCodingRateSet();

    Serial.print(
        "Enter value number to edit: "
    );

    String indexInput =
        readInput();

    int index =
        indexInput.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.codingRateCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    Serial.print(
        "Enter new coding rate denominator [5-8]: "
    );

    String valueInput =
        readInput();

    int value =
        valueInput.toInt();

    if (value < 5 ||
        value > 8) {

        Serial.println(
            "Invalid coding rate."
        );

        return;
    }

    configuration.codingRate[index] =
        static_cast<uint8_t>(value);

    Serial.println(
        "Coding rate updated."
    );
}

static void configureCodingRateSet()
{
    while (true) {

        printCodingRateSet();

        Serial.println("1. Add Value");
        Serial.println("2. Remove Value");
        Serial.println("3. Edit Value");
        Serial.println("4. Clear Set");
        Serial.println("5. Restore Default Set");
        Serial.println("X. Back");
        Serial.print("Select: ");

        String command =
            readInput();

        command.toUpperCase();

        if (command == "1") {

            addCodingRate();

        } else if (command == "2") {

            removeCodingRate();

        } else if (command == "3") {

            editCodingRateValue();

        } else if (command == "4") {

            configuration.codingRateCount = 0;

            Serial.println(
                "Coding rate set cleared."
            );

        } else if (command == "5") {

            const uint8_t defaults[] = {
                5,
                6,
                7,
                8
            };

            configuration.codingRateCount =
                sizeof(defaults) /
                sizeof(defaults[0]);

            for (size_t i = 0;
                 i < configuration.codingRateCount;
                 i++) {

                configuration.codingRate[i] =
                    defaults[i];
            }

            Serial.println(
                "Coding rate default set restored."
            );

        } else if (command == "X") {

            return;

        } else {

            Serial.println(
                "Invalid selection."
            );
        }
    }
}

// ============================================================
// TX POWER
// ============================================================

static void addTxPower()
{
    if (configuration.txPowerCount >= MAX_VALUES) {

        Serial.println(
            "Maximum number of values reached."
        );

        return;
    }

    Serial.print(
        "Enter TX power in dBm: "
    );

    String input =
        readInput();

    int value =
        input.toInt();

    if (value < -9 ||
        value > 22) {

        Serial.println(
            "Invalid TX power. Use -9 to 22 dBm."
        );

        return;
    }

    configuration.txPower[
        configuration.txPowerCount
    ] = static_cast<int8_t>(value);

    configuration.txPowerCount++;

    Serial.println(
        "TX power added."
    );
}

static void removeTxPower()
{
    if (configuration.txPowerCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printTxPowerSet();

    Serial.print(
        "Enter value number to remove: "
    );

    String input =
        readInput();

    int index =
        input.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.txPowerCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    for (size_t i = index;
         i + 1 < configuration.txPowerCount;
         i++) {

        configuration.txPower[i] =
            configuration.txPower[i + 1];
    }

    configuration.txPowerCount--;

    Serial.println(
        "TX power removed."
    );
}

static void editTxPowerValue()
{
    if (configuration.txPowerCount == 0) {

        Serial.println(
            "No values configured."
        );

        return;
    }

    printTxPowerSet();

    Serial.print(
        "Enter value number to edit: "
    );

    String indexInput =
        readInput();

    int index =
        indexInput.toInt() - 1;

    if (
        index < 0 ||
        index >= static_cast<int>(
            configuration.txPowerCount
        )
    ) {

        Serial.println(
            "Invalid selection."
        );

        return;
    }

    Serial.print(
        "Enter new TX power in dBm: "
    );

    String valueInput =
        readInput();

    int value =
        valueInput.toInt();

    if (value < -9 ||
        value > 22) {

        Serial.println(
            "Invalid TX power."
        );

        return;
    }

    configuration.txPower[index] =
        static_cast<int8_t>(value);

    Serial.println(
        "TX power updated."
    );
}

static void configureTxPowerSet()
{
    while (true) {

        printTxPowerSet();

        Serial.println("1. Add Value");
        Serial.println("2. Remove Value");
        Serial.println("3. Edit Value");
        Serial.println("4. Clear Set");
        Serial.println("5. Restore Default Set");
        Serial.println("X. Back");
        Serial.print("Select: ");

        String command =
            readInput();

        command.toUpperCase();

        if (command == "1") {

            addTxPower();

        } else if (command == "2") {

            removeTxPower();

        } else if (command == "3") {

            editTxPowerValue();

        } else if (command == "4") {

            configuration.txPowerCount = 0;

            Serial.println(
                "TX power set cleared."
            );

        } else if (command == "5") {

            const int8_t defaults[] = {
                11,
                14,
                17,
                20
            };

            configuration.txPowerCount =
                sizeof(defaults) /
                sizeof(defaults[0]);

            for (size_t i = 0;
                 i < configuration.txPowerCount;
                 i++) {

                configuration.txPower[i] =
                    defaults[i];
            }

            Serial.println(
                "TX power default set restored."
            );

        } else if (command == "X") {

            return;

        } else {

            Serial.println(
                "Invalid selection."
            );
        }
    }
}

// ============================================================
// SUMMARY
// ============================================================

static void showSummary()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "RCE TEST MATRIX SUMMARY"
    );
    Serial.println(
        "========================================"
    );

    // --------------------------------------------------------
    // FREQUENCY
    // --------------------------------------------------------

    Serial.println();
    Serial.printf(
        "Frequency        : %u values\n",
        static_cast<unsigned int>(
            configuration.frequencyCount
        )
    );

    for (size_t i = 0;
         i < configuration.frequencyCount;
         i++) {

        Serial.printf(
            "  %.4f MHz\n",
            configuration.frequency[i]
        );
    }

    // --------------------------------------------------------
    // SPREADING FACTOR
    // --------------------------------------------------------

    Serial.println();
    Serial.printf(
        "Spreading Factor : %u values\n",
        static_cast<unsigned int>(
            configuration.spreadingFactorCount
        )
    );

    for (size_t i = 0;
         i < configuration.spreadingFactorCount;
         i++) {

        Serial.printf(
            "  SF%u\n",
            configuration.spreadingFactor[i]
        );
    }

    // --------------------------------------------------------
    // BANDWIDTH
    // --------------------------------------------------------

    Serial.println();
    Serial.printf(
        "Bandwidth        : %u values\n",
        static_cast<unsigned int>(
            configuration.bandwidthCount
        )
    );

    for (size_t i = 0;
         i < configuration.bandwidthCount;
         i++) {

        Serial.printf(
            "  %.1f kHz\n",
            configuration.bandwidth[i]
        );
    }

    // --------------------------------------------------------
    // CODING RATE
    // --------------------------------------------------------

    Serial.println();
    Serial.printf(
        "Coding Rate      : %u values\n",
        static_cast<unsigned int>(
            configuration.codingRateCount
        )
    );

    for (size_t i = 0;
         i < configuration.codingRateCount;
         i++) {

        Serial.printf(
            "  4/%u\n",
            configuration.codingRate[i]
        );
    }

    // --------------------------------------------------------
    // TX POWER
    // --------------------------------------------------------

    Serial.println();
    Serial.printf(
        "TX Power         : %u values\n",
        static_cast<unsigned int>(
            configuration.txPowerCount
        )
    );

    for (size_t i = 0;
         i < configuration.txPowerCount;
         i++) {

        Serial.printf(
            "  %d dBm\n",
            configuration.txPower[i]
        );
    }

    // --------------------------------------------------------
    // TOTAL
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "========================================"
    );

    uint64_t total =
        configurationCount();

    Serial.println(
        "TOTAL CONFIGURATIONS"
    );

    Serial.printf(
        "%u × %u × %u × %u × %u = %llu\n",
        static_cast<unsigned int>(
            configuration.frequencyCount
        ),
        static_cast<unsigned int>(
            configuration.spreadingFactorCount
        ),
        static_cast<unsigned int>(
            configuration.bandwidthCount
        ),
        static_cast<unsigned int>(
            configuration.codingRateCount
        ),
        static_cast<unsigned int>(
            configuration.txPowerCount
        ),
        static_cast<unsigned long long>(
            total
        )
    );

    Serial.println();

    Serial.printf(
        "Configurations per direction : %llu\n",
        static_cast<unsigned long long>(
            total
        )
    );

    Serial.printf(
        "TX + RX                       : %llu\n",
        static_cast<unsigned long long>(
            total * 2
        )
    );

    Serial.println();
}

// ============================================================
// TEST MATRIX MENU
// ============================================================

static void showTestMatrixMenu()
{
    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "RCE TEST MATRIX CONFIGURATION"
    );
    Serial.println(
        "========================================"
    );

    Serial.printf(
        "Total Configurations : %llu\n",
        static_cast<unsigned long long>(
            configurationCount()
        )
    );

    Serial.println();

    Serial.println(
        "1. Edit Frequency Set"
    );

    Serial.println(
        "2. Edit Spreading Factor Set"
    );

    Serial.println(
        "3. Edit Bandwidth Set"
    );

    Serial.println(
        "4. Edit Coding Rate Set"
    );

    Serial.println(
        "5. Edit TX Power Set"
    );

    Serial.println(
        "6. Test Matrix Summary"
    );

    Serial.println(
        "7. Load JSON Configuration"
    );

    Serial.println(
        "8. Save JSON Configuration"
    );

    Serial.println(
        "9. Restore Default Matrix"
    );

    Serial.println(
        "X. Back"
    );

    Serial.print(
        "Select: "
    );
}

// ============================================================
// BEGIN
// ============================================================

void begin()
{
    if (initialized) {
        return;
    }

    if (!LittleFS.begin(true)) {

        Serial.println();
        Serial.println(
            "Failed to mount LittleFS."
        );

        loadDefaultConfiguration();

        initialized = true;

        return;
    }

    if (!load()) {

        Serial.println();
        Serial.println(
            "Using default RCE Test Matrix."
        );

        loadDefaultConfiguration();
    }

    initialized = true;
}

// ============================================================
// FEATURE
// ============================================================

void feature()
{
    if (!initialized) {
        begin();
    }

    while (true) {

        showTestMatrixMenu();

        String command =
            readInput();

        command.toUpperCase();

        if (command == "1") {

            configureFrequencySet();

        } else if (command == "2") {

            configureSpreadingFactorSet();

        } else if (command == "3") {

            configureBandwidthSet();

        } else if (command == "4") {

            configureCodingRateSet();

        } else if (command == "5") {

            configureTxPowerSet();

        } else if (command == "6") {

            showSummary();

        } else if (command == "7") {

            if (load()) {

                Serial.println(
                    "JSON configuration loaded successfully."
                );
            }

        } else if (command == "8") {

            save();

        } else if (command == "9") {

            loadDefaultConfiguration();

            Serial.println();
            Serial.println(
                "Default RCE Test Matrix restored in memory."
            );

            Serial.println(
                "Use option 8 to save it to JSON."
            );

        } else if (command == "X") {

            return;

        } else {

            Serial.println(
                "Invalid selection."
            );
        }
    }
}

} // namespace TestMatrix
} // namespace Radio