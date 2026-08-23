#ifndef WIFI_FEATURE_H
#define WIFI_FEATURE_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

static Preferences wifiPrefs;
static bool wifiConfigured=false;
static String wifiSavedSSID="";
static String wifiSavedPassword="";

static String wifiReadLine(){
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

static void wifiLoadConfig(){
  wifiPrefs.begin("wifi",true);
  wifiConfigured=wifiPrefs.getBool("configured",false);
  wifiSavedSSID=wifiPrefs.getString("ssid","");
  wifiSavedPassword=wifiPrefs.getString("password","");
  wifiPrefs.end();
  if(wifiSavedSSID.length()==0) wifiConfigured=false;
}

static void wifiSaveConfig(const String& ssid,const String& password){
  wifiPrefs.begin("wifi",false);
  wifiPrefs.putBool("configured",true);
  wifiPrefs.putString("ssid",ssid);
  wifiPrefs.putString("password",password);
  wifiPrefs.end();
  wifiConfigured=true;
  wifiSavedSSID=ssid;
  wifiSavedPassword=password;
}

static void wifiForgetConfig(){
  wifiPrefs.begin("wifi",false);
  wifiPrefs.clear();
  wifiPrefs.end();
  wifiConfigured=false;
  wifiSavedSSID="";
  wifiSavedPassword="";
  WiFi.disconnect(true,true);
  Serial.println("Wi-Fi configuration removed.");
}

static bool wifiConnect(const String& ssid,const String& password,uint32_t timeoutMs=15000){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(300);
  Serial.printf("Connecting to: %s\n",ssid.c_str());
  WiFi.begin(ssid.c_str(),password.c_str());
  uint32_t start=millis();
  while(WiFi.status()!=WL_CONNECTED&&millis()-start<timeoutMs){
    Serial.print(".");
    delay(250);
  }
  Serial.println();
  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("Connection failed.");
    return false;
  }
  Serial.println("Wi-Fi connected.");
  Serial.printf("SSID : %s\n",WiFi.SSID().c_str());
  Serial.printf("IP   : %s\n",WiFi.localIP().toString().c_str());
  Serial.printf("RSSI : %d dBm\n",WiFi.RSSI());
  return true;
}

static bool wifiConnectSaved(){
  if(!wifiConfigured){
    Serial.println("No saved Wi-Fi configuration.");
    return false;
  }
  return wifiConnect(
    wifiSavedSSID,
    wifiSavedPassword
  );
}

static void wifiScan(){
  Serial.println();
  Serial.println("Scanning for Wi-Fi networks...");
  int count=WiFi.scanNetworks();
  if(count<=0){
    Serial.println("No networks found.");
    return;
  }
  Serial.println();
  Serial.println("========================================");
  Serial.println("AVAILABLE NETWORKS");
  Serial.println("========================================");
  for(int i=0;i<count;i++){
    Serial.printf(
      "%d. %s  [%d dBm] %s\n",
      i+1,
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      WiFi.encryptionType(i)==WIFI_AUTH_OPEN
        ? "OPEN"
        : "SECURED"
    );
  }
  Serial.println();
  Serial.println("Enter network index.");
  Serial.println("X. Cancel");
  Serial.print("> ");
  String selection=wifiReadLine();
  if(selection.equalsIgnoreCase("X")) return;
  int index=selection.toInt();
  if(index<1||index>count){
    Serial.println("Invalid network index.");
    return;
  }
  String selectedSSID=WiFi.SSID(index-1);
  Serial.printf("Selected: %s\n",selectedSSID.c_str());
  Serial.println("Enter password.");
  Serial.println("For an open network, leave blank.");
  Serial.print("> ");
  String password=wifiReadLine();
  Serial.println();
  if(wifiConnect(selectedSSID,password)){
    Serial.println();
    Serial.println("Save this Wi-Fi configuration? (Y/N)");
    Serial.print("> ");
    String answer=wifiReadLine();
    if(answer.equalsIgnoreCase("Y")){
      wifiSaveConfig(selectedSSID,password);
      Serial.println("Wi-Fi configuration saved to NVS.");
    }else{
      Serial.println("Connection kept, configuration not saved.");
    }
  }
  WiFi.scanDelete();
}

static void wifiShowStatus(){
  Serial.println();
  Serial.println("========================================");
  Serial.println("WI-FI STATUS");
  Serial.println("========================================");
  Serial.printf(
    "Configured : %s\n",
    wifiConfigured?"YES":"NO"
  );
  if(WiFi.status()==WL_CONNECTED){
    Serial.println("Status     : CONNECTED");
    Serial.printf("SSID       : %s\n",WiFi.SSID().c_str());
    Serial.printf("IP         : %s\n",WiFi.localIP().toString().c_str());
    Serial.printf("RSSI       : %d dBm\n",WiFi.RSSI());
    Serial.printf("Gateway    : %s\n",WiFi.gatewayIP().toString().c_str());
    Serial.printf("DNS        : %s\n",WiFi.dnsIP().toString().c_str());
  }else{
    Serial.println("Status     : NOT CONNECTED");
  }
}

static void wifiFeature(){
  while(true){
    Serial.println();
    Serial.println("========================================");
    Serial.println("WI-FI");
    Serial.println("========================================");
    wifiShowStatus();
    Serial.println();
    Serial.println("1. Scan / Connect");
    Serial.println("2. Connect Saved Network");
    Serial.println("3. Show Status");
    Serial.println("4. Forget Saved Network");
    Serial.println("X. Back");
    Serial.println();
    Serial.print("Select option: ");
    String command=wifiReadLine();
    command.trim();
    if(command=="1"){
      wifiScan();
    }else if(command=="2"){
      wifiConnectSaved();
    }else if(command=="3"){
      wifiShowStatus();
    }else if(command=="4"){
      if(!wifiConfigured){
        Serial.println("No saved Wi-Fi configuration.");
        continue;
      }
      Serial.println("Forget saved Wi-Fi configuration? (Y/N)");
      Serial.print("> ");
      String answer=wifiReadLine();
      if(answer.equalsIgnoreCase("Y")){
        wifiForgetConfig();
      }
    }else if(command.equalsIgnoreCase("X")){
      return;
    }else{
      Serial.println("Invalid selection.");
    }
  }
}

static bool wifiIsConfigured(){
  return wifiConfigured;
}

#endif