#ifndef PING_FEATURE_H
#define PING_FEATURE_H
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>

static String pingReadLine(){
  String input;
  while(true){
    while(Serial.available()){
      char c=Serial.read();
      if(c=='\n'||c=='\r'){
        if(input.length()>0){
          Serial.println();
          return input;
        }
      }else{
        input+=c;
        Serial.print(c);
      }
    }
    delay(5);
  }
}

static void pingFeature(){
  while(true){
    Serial.println();
    Serial.println("========================================");
    Serial.println("PING");
    Serial.println("========================================");
    Serial.println("Enter IP address or domain.");
    Serial.println("Enter X to exit.");
    Serial.print("> ");

    String target=pingReadLine();
    target.trim();

    if(target.equalsIgnoreCase("X")){
      Serial.println();
      Serial.println("Exit ping? (Y/N)");
      Serial.print("> ");

      String answer=pingReadLine();
      answer.trim();

      if(answer.equalsIgnoreCase("Y")){
        return;
      }

      continue;
    }

    if(target.length()==0){
      continue;
    }

    Serial.println();
    Serial.printf("Resolving: %s\n",target.c_str());

    IPAddress address;

    if(!WiFi.hostByName(target.c_str(),address)){
      Serial.println("DNS resolution failed.");
      continue;
    }

    Serial.printf("Resolved IP: %s\n",address.toString().c_str());
    Serial.println("Sending 4 ICMP echo requests...");
    Serial.println("----------------------------------------");

    const uint8_t packetCount=4;
    uint8_t packetsReceived=0;
    uint32_t totalTime=0;

    for(uint8_t i=0;i<packetCount;i++){
      uint32_t start=millis();

      bool success=Ping.ping(address,1);

      uint32_t elapsed=millis()-start;

      if(success){
        packetsReceived++;
        totalTime+=elapsed;

        Serial.printf(
          "Reply from %s: time=%lu ms\n",
          address.toString().c_str(),
          (unsigned long)elapsed
        );
      }else{
        Serial.printf(
          "Request timed out: %lu ms\n",
          (unsigned long)elapsed
        );
      }

      delay(500);
    }

    uint8_t packetsLost=packetCount-packetsReceived;
    uint8_t packetLoss=(packetsLost*100)/packetCount;

    Serial.println();
    Serial.println("----------------------------------------");
    Serial.printf("Packets sent    : %d\n",packetCount);
    Serial.printf("Packets received: %d\n",packetsReceived);
    Serial.printf("Packet loss     : %d%%\n",packetLoss);

    if(packetsReceived>0){
      uint32_t averageTime=totalTime/packetsReceived;
      Serial.printf("Average time    : %lu ms\n",(unsigned long)averageTime);
      Serial.println("PING SUCCESS");
    }else{
      Serial.println("Average time    : N/A");
      Serial.println("PING FAILED");
    }
  }
}

#endif