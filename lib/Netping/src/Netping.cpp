#include "Netping.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>

namespace {

String readLine() {
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

bool resolveHost(const String& target, IPAddress& address) {
    Serial.printf("Resolving: %s\n", target.c_str());

    if (!WiFi.hostByName(target.c_str(), address)) {
        Serial.println("DNS resolution failed.");
        return false;
    }

    Serial.printf(
        "Resolved IP: %s\n",
        address.toString().c_str()
    );

    return true;
}

void pingHost(const IPAddress& address) {
    const uint8_t packetCount = 4;
    uint8_t packetsReceived = 0;
    uint32_t totalTime = 0;

    Serial.println("Sending 4 ICMP echo requests...");
    Serial.println("----------------------------------------");

    for (uint8_t i = 0; i < packetCount; i++) {
        uint32_t start = millis();

        bool success = Ping.ping(address, 1);

        uint32_t elapsed = millis() - start;

        if (success) {
            packetsReceived++;
            totalTime += elapsed;

            Serial.printf(
                "Reply from %s: time=%lu ms\n",
                address.toString().c_str(),
                (unsigned long)elapsed
            );
        } else {
            Serial.printf(
                "Request timed out: %lu ms\n",
                (unsigned long)elapsed
            );
        }

        delay(500);
    }

    uint8_t packetsLost = packetCount - packetsReceived;
    uint8_t packetLoss = (packetsLost * 100) / packetCount;

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.printf(
        "Packets sent    : %d\n",
        packetCount
    );
    Serial.printf(
        "Packets received: %d\n",
        packetsReceived
    );
    Serial.printf(
        "Packet loss     : %d%%\n",
        packetLoss
    );

    if (packetsReceived > 0) {
        uint32_t averageTime =
            totalTime / packetsReceived;

        Serial.printf(
            "Average time    : %lu ms\n",
            (unsigned long)averageTime
        );

        Serial.println("PING SUCCESS");
    } else {
        Serial.println("Average time    : N/A");
        Serial.println("PING FAILED");
    }
}

void showHeader() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("PING");
    Serial.println("========================================");
}

}

namespace Netping {

void begin() {
}

void service() {
}

void feature() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println();
        Serial.println("Wi-Fi is not connected.");
        Serial.println("Connect to Wi-Fi before using Ping.");
        return;
    }

    while (true) {
        showHeader();

        Serial.println("Enter IP address or domain.");
        Serial.println("X. Back");
        Serial.print("> ");

        String target = readLine();
        target.trim();

        if (target.equalsIgnoreCase("X")) {
            return;
        }

        if (target.length() == 0) {
            continue;
        }

        IPAddress address;

        if (!resolveHost(target, address)) {
            continue;
        }

        Serial.println();

        pingHost(address);
    }
}

}