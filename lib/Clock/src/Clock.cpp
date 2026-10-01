#include "Clock.h"
#include <Arduino.h>
#include <time.h>

namespace {

static void printClock(){
    time_t now=time(nullptr);

    if(now<100000){
        Serial.println();
        Serial.println("Clock: TIME NOT SET");
        return;
    }

    struct tm timeInfo;
    localtime_r(&now,&timeInfo);

    char timeString[16];
    char dateString[16];

    strftime(
        timeString,
        sizeof(timeString),
        "%H:%M:%S",
        &timeInfo
    );

    strftime(
        dateString,
        sizeof(dateString),
        "%Y-%m-%d",
        &timeInfo
    );

    Serial.printf(
        "\rDate: %s    Time: %s",
        dateString,
        timeString
    );
}

}

namespace Clock {

void begin(){
}

void service(){
}

bool hasValidTime(){
    return time(nullptr)>=100000;
}

void feature(){
    Serial.println();
    Serial.println("========================================");
    Serial.println("CLOCK");
    Serial.println("========================================");
    Serial.println();

    if(!hasValidTime()){
        Serial.println("System time is not set.");
        Serial.println();
        Serial.println("Synchronize the system clock using NTP.");
        Serial.println();
        Serial.println("Press Enter to return.");
        Serial.println();

        while(true){
            if(Serial.available()){
                String command=Serial.readStringUntil('\n');
                command.trim();
                return;
            }

            delay(10);
        }
    }

    Serial.println("Press X and Enter to return.");
    Serial.println();

    while(true){
        if(Serial.available()){
            String command=Serial.readStringUntil('\n');
            command.trim();

            if(command.equalsIgnoreCase("X")){
                Serial.println();
                Serial.println("Returning to main menu.");
                return;
            }
        }

        printClock();
        delay(1000);
    }
}

}