#include <Arduino.h>
#include <WiFi.h>
#include "wifi_feature.h"
#include "ping_feature.h"
#include "ntp_feature.h"


static const uint32_t SERIAL_BAUD=115200;

static String readMainCommand(){
  String input;
  while(true){
    while(Serial.available()){
      char c=Serial.read();
      if(c=='\n'||c=='\r'){
        if(input.length()>0){
          Serial.println();
          input.trim();
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

static void showMainMenu(){
  Serial.println();
  Serial.println("========================================");
  Serial.println("ESP32 NETWORK UTILITY");
  Serial.println("========================================");
  if(WiFi.status()==WL_CONNECTED){
    Serial.println("Wi-Fi Status : CONNECTED");
    Serial.printf("SSID         : %s\n",WiFi.SSID().c_str());
    Serial.printf("IP           : %s\n",WiFi.localIP().toString().c_str());
  }else{
    Serial.println("Wi-Fi Status : NOT CONNECTED");
  }
  if(ntpHasValidSystemTime()){
    Serial.printf("Time         : %s\n",ntpCurrentTime().c_str());
    Serial.printf("Date         : %s\n",ntpCurrentDate().c_str());
    if(ntpIsSynchronized()){
      Serial.println("NTP Status   : SYNCHRONIZED");
    }else if(ntpIsConfigured()&&ntpIsEnabled()){
      Serial.println("NTP Status   : ENABLED / NOT SYNCED");
    }else if(ntpIsConfigured()){
      Serial.println("NTP Status   : DISABLED");
    }else{
      Serial.println("NTP Status   : NOT CONFIGURED");
    }
  }else{
    Serial.println("Time         : NOT SET");
    if(ntpIsConfigured()&&ntpIsEnabled()){
      Serial.println("NTP Status   : ENABLED / NOT SYNCED");
    }else if(ntpIsConfigured()){
      Serial.println("NTP Status   : DISABLED");
    }else{
      Serial.println("NTP Status   : NOT CONFIGURED");
    }
  }
  Serial.println();
  Serial.println("1. Wi-Fi");
  Serial.println("2. Ping");
  Serial.println("3. NTP Time");
//   Serial.println("4. LoRa Radio");
  Serial.println("X. Exit");
  Serial.println();
  Serial.print("Select feature: ");
}

static void featureMenu(){
  while(true){
    showMainMenu();
    String command=readMainCommand();
    command.trim();
    if(command=="1"){
      wifiFeature();
    }else if(command=="2"){
      pingFeature();
    }else if(command=="3"){
      ntpFeature();
    }
    else if(command.equalsIgnoreCase("X")){
      Serial.println("Menu idle.");
      return;
    }else{
      Serial.println("Invalid selection.");
    }
  }
}

void setup(){
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  WiFi.mode(WIFI_STA);
  wifiLoadConfig();
  ntpLoadConfig();
  Serial.println();
  Serial.println("========================================");
  Serial.println("ESP32 NETWORK UTILITY");
  Serial.println("========================================");
  Serial.println("Persistent configuration loaded.");
  if(wifiIsConfigured()){
    Serial.println("Saved Wi-Fi configuration detected.");
    wifiConnectSaved();
  }
  if(ntpIsConfigured()){
    Serial.println("Saved NTP configuration detected.");
    if(ntpIsEnabled()){
      Serial.println("NTP is enabled.");
      if(WiFi.status()==WL_CONNECTED){
        if(ntpSyncNow()){
          Serial.println("Startup NTP synchronization successful.");
        }else{
          Serial.println("Startup NTP synchronization failed.");
        }
      }else{
        Serial.println("Waiting for Wi-Fi before NTP synchronization.");
      }
    }else{
      Serial.println("NTP is disabled.");
    }
  }
  Serial.println("Boot complete.");
  featureMenu();
}

void loop(){
  ntpService();
  delay(100);
}