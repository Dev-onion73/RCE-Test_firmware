#ifndef NTP_FEATURE_H
#define NTP_FEATURE_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

static Preferences ntpPrefs;
static bool ntpConfigured=false;
static bool ntpEnabled=false;
static bool ntpSynchronized=false;
static bool systemTimeValid=false;
static String ntpHostname="";
static int32_t ntpOffsetSeconds=0;
static uint32_t ntpSyncIntervalSeconds=5;
static uint32_t ntpLastSyncMillis=0;

static String ntpReadLine(){
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

static void ntpLoadConfig(){
  ntpPrefs.begin("ntp",true);
  ntpConfigured=ntpPrefs.getBool("configured",false);
  ntpEnabled=ntpPrefs.getBool("enabled",false);
  ntpHostname=ntpPrefs.getString("hostname","");
  ntpOffsetSeconds=ntpPrefs.getLong("offset",0);
  ntpSyncIntervalSeconds=ntpPrefs.getUInt("interval",5);
  ntpPrefs.end();
  if(ntpHostname.length()==0) ntpConfigured=false;
  if(ntpSyncIntervalSeconds==0) ntpSyncIntervalSeconds=5;
}

static void ntpSaveConfig(){
  ntpPrefs.begin("ntp",false);
  ntpPrefs.putBool("configured",ntpConfigured);
  ntpPrefs.putBool("enabled",ntpEnabled);
  ntpPrefs.putString("hostname",ntpHostname);
  ntpPrefs.putLong("offset",ntpOffsetSeconds);
  ntpPrefs.putUInt("interval",ntpSyncIntervalSeconds);
  ntpPrefs.end();
}

static bool ntpParseOffset(const String& input,int32_t& result){
  String value=input;
  value.trim();
  if(value.length()==0){
    result=0;
    return true;
  }
  char sign='+';
  int index=0;
  if(value[0]=='+'||value[0]=='-'){
    sign=value[0];
    index=1;
  }
  if(index>=value.length()) return false;
  int colon=value.indexOf(':',index);
  int hours=0;
  int minutes=0;
  if(colon>=0){
    String h=value.substring(index,colon);
    String m=value.substring(colon+1);
    if(h.length()==0||m.length()!=2) return false;
    for(size_t i=0;i<h.length();i++){
      if(!isDigit(h[i])) return false;
    }
    for(size_t i=0;i<m.length();i++){
      if(!isDigit(m[i])) return false;
    }
    hours=h.toInt();
    minutes=m.toInt();
  }else{
    String h=value.substring(index);
    if(h.length()==0) return false;
    for(size_t i=0;i<h.length();i++){
      if(!isDigit(h[i])) return false;
    }
    hours=h.toInt();
  }
  if(hours>23||minutes>59) return false;
  result=(hours*3600)+(minutes*60);
  if(sign=='-') result=-result;
  return true;
}

static String ntpFormatOffset(int32_t offset){
  char buffer[16];
  char sign='+';
  if(offset<0){
    sign='-';
    offset=-offset;
  }
  int hours=offset/3600;
  int minutes=(offset%3600)/60;
  snprintf(
    buffer,
    sizeof(buffer),
    "%c%02d:%02d",
    sign,
    hours,
    minutes
  );
  return String(buffer);
}

static bool ntpVerifyHostname(const String& hostname){
  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("Wi-Fi is not connected.");
    return false;
  }
  Serial.printf("Resolving: %s\n",hostname.c_str());
  IPAddress address;
  if(!WiFi.hostByName(hostname.c_str(),address)){
    Serial.println("Hostname verification: FAILED");
    return false;
  }
  Serial.printf("Resolved IP: %s\n",address.toString().c_str());
  Serial.println("Hostname verification: SUCCESS");
  return true;
}

static bool ntpWaitForSync(uint32_t timeoutMs){
  uint32_t start=millis();
  while(millis()-start<timeoutMs){
    struct tm timeInfo;
    if(getLocalTime(&timeInfo,1000)) return true;
    delay(100);
  }
  return false;
}

static bool ntpSyncNow(){
  if(!ntpConfigured||!ntpEnabled) return false;
  if(WiFi.status()!=WL_CONNECTED){
    ntpSynchronized=false;
    return false;
  }
  configTime(0,0,ntpHostname.c_str());
  if(!ntpWaitForSync(10000)){
    ntpSynchronized=false;
    return false;
  }
  ntpSynchronized=true;
  systemTimeValid=true;
  ntpLastSyncMillis=millis();
  return true;
}

static String ntpCurrentTime(){
  if(!systemTimeValid) return "NOT SET";
  time_t now;
  time(&now);
  now+=ntpOffsetSeconds;
  struct tm timeInfo;
  gmtime_r(&now,&timeInfo);
  char buffer[16];
  strftime(buffer,sizeof(buffer),"%H:%M:%S",&timeInfo);
  return String(buffer);
}

static String ntpCurrentDate(){
  if(!systemTimeValid) return "NOT SET";
  time_t now;
  time(&now);
  now+=ntpOffsetSeconds;
  struct tm timeInfo;
  gmtime_r(&now,&timeInfo);
  char buffer[20];
  strftime(buffer,sizeof(buffer),"%d-%b-%Y",&timeInfo);
  return String(buffer);
}

static void ntpShowStatus(){
  Serial.println();
  Serial.println("========================================");
  Serial.println("NTP STATUS");
  Serial.println("========================================");
  Serial.printf("Configured : %s\n",ntpConfigured?"YES":"NO");
  Serial.printf("Enabled    : %s\n",ntpEnabled?"YES":"NO");
  Serial.printf("Synchronized: %s\n",ntpSynchronized?"YES":"NO");
  if(ntpConfigured){
    Serial.printf("Server     : %s\n",ntpHostname.c_str());
    Serial.printf("UTC Offset : %s\n",ntpFormatOffset(ntpOffsetSeconds).c_str());
    Serial.printf("Interval   : %lu seconds\n",(unsigned long)ntpSyncIntervalSeconds);
  }
  Serial.printf("Date       : %s\n",ntpCurrentDate().c_str());
  Serial.printf("Time       : %s\n",ntpCurrentTime().c_str());
}

static void ntpSetup(){
  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("Wi-Fi must be connected before NTP setup.");
    return;
  }
  Serial.println();
  Serial.println("NTP SERVER SETUP");
  Serial.println("Enter hostname.");
  Serial.println("Example: time.cloudflare.com");
  Serial.println("X. Cancel");
  Serial.print("> ");
  String hostname=ntpReadLine();
  hostname.trim();
  if(hostname.equalsIgnoreCase("X")) return;
  if(hostname.length()==0){
    Serial.println("Hostname cannot be empty.");
    return;
  }
  if(!ntpVerifyHostname(hostname)) return;
  Serial.println("Enter UTC offset.");
  Serial.println("Blank = UTC.");
  Serial.println("Examples: +05:30, -04:00, +01:00");
  Serial.print("> ");
  String offsetInput=ntpReadLine();
  int32_t offsetSeconds;
  while(!ntpParseOffset(offsetInput,offsetSeconds)){
    Serial.println("Invalid UTC offset.");
    Serial.print("> ");
    offsetInput=ntpReadLine();
  }
  Serial.println("Enter sync interval in seconds.");
  Serial.println("Blank = 5 seconds.");
  Serial.print("> ");
  String intervalInput=ntpReadLine();
  uint32_t interval=5;
  if(intervalInput.length()>0){
    bool valid=true;
    for(size_t i=0;i<intervalInput.length();i++){
      if(!isDigit(intervalInput[i])){
        valid=false;
        break;
      }
    }
    if(valid&&intervalInput.toInt()>0){
      interval=intervalInput.toInt();
    }
  }
  Serial.println();
  Serial.printf("Server  : %s\n",hostname.c_str());
  Serial.printf("Offset  : %s\n",ntpFormatOffset(offsetSeconds).c_str());
  Serial.printf("Interval: %lu seconds\n",(unsigned long)interval);
  Serial.println("Save configuration? (Y/N)");
  Serial.print("> ");
  String answer=ntpReadLine();
  if(!answer.equalsIgnoreCase("Y")) return;
  ntpHostname=hostname;
  ntpOffsetSeconds=offsetSeconds;
  ntpSyncIntervalSeconds=interval;
  ntpConfigured=true;
  ntpEnabled=true;
  ntpSynchronized=false;
  ntpSaveConfig();
  Serial.println("NTP configuration saved to NVS.");
  Serial.println("Performing initial synchronization...");
  if(ntpSyncNow()){
    Serial.println("NTP synchronization successful.");
  }else{
    Serial.println("NTP synchronization failed.");
    Serial.println("Configuration remains saved.");
  }
}

static void ntpEnable(){
  if(!ntpConfigured){
    Serial.println("NTP is not configured.");
    return;
  }
  ntpEnabled=true;
  ntpSynchronized=false;
  ntpSaveConfig();
  Serial.println("NTP enabled.");
  if(ntpSyncNow()){
    Serial.println("NTP synchronization successful.");
  }else{
    Serial.println("NTP synchronization failed. Automatic retry remains enabled.");
  }
}

static void ntpDisable(){
  ntpEnabled=false;
  ntpSynchronized=false;
  ntpLastSyncMillis=0;
  ntpSaveConfig();
  Serial.println("NTP disabled.");
  Serial.println("NTP configuration retained.");
  Serial.println("Local system clock continues running.");
}

static void ntpFeature(){
  while(true){
    Serial.println();
    Serial.println("========================================");
    Serial.println("NTP TIME");
    Serial.println("========================================");
    ntpShowStatus();
    Serial.println();
    if(!ntpConfigured){
      Serial.println("1. Setup NTP");
    }else if(!ntpEnabled){
      Serial.println("1. Enable NTP");
      Serial.println("2. Show Time");
      Serial.println("3. Reconfigure NTP");
    }else{
      Serial.println("1. Sync Now");
      Serial.println("2. Show Time");
      Serial.println("3. Reconfigure NTP");
      Serial.println("4. Disable NTP");
    }
    Serial.println("X. Back");
    Serial.print("Select option: ");
    String command=ntpReadLine();
    command.trim();
    if(!ntpConfigured){
      if(command=="1"){
        ntpSetup();
        continue;
      }
      if(command.equalsIgnoreCase("X")) return;
      Serial.println("Invalid selection.");
      continue;
    }
    if(!ntpEnabled){
      if(command=="1"){
        ntpEnable();
        continue;
      }
      if(command=="2"){
        Serial.printf("Date: %s\n",ntpCurrentDate().c_str());
        Serial.printf("Time: %s\n",ntpCurrentTime().c_str());
        continue;
      }
      if(command=="3"){
        ntpSetup();
        continue;
      }
      if(command.equalsIgnoreCase("X")) return;
      Serial.println("Invalid selection.");
      continue;
    }
    if(command=="1"){
      if(ntpSyncNow()){
        Serial.println("NTP synchronization successful.");
      }else{
        Serial.println("NTP synchronization failed.");
      }
      continue;
    }
    if(command=="2"){
      Serial.printf("Date: %s\n",ntpCurrentDate().c_str());
      Serial.printf("Time: %s\n",ntpCurrentTime().c_str());
      continue;
    }
    if(command=="3"){
      ntpSetup();
      continue;
    }
    if(command=="4"){
      Serial.println("Disable NTP? (Y/N)");
      Serial.print("> ");
      String answer=ntpReadLine();
      if(answer.equalsIgnoreCase("Y")) ntpDisable();
      continue;
    }
    if(command.equalsIgnoreCase("X")) return;
    Serial.println("Invalid selection.");
  }
}

static void ntpService(){
  if(!ntpConfigured||!ntpEnabled) return;
  if(WiFi.status()!=WL_CONNECTED){
    ntpSynchronized=false;
    return;
  }
  if(!ntpSynchronized||millis()-ntpLastSyncMillis>=ntpSyncIntervalSeconds*1000UL){
    ntpSyncNow();
  }
}

static bool ntpIsConfigured(){
  return ntpConfigured;
}

static bool ntpIsEnabled(){
  return ntpEnabled;
}

static bool ntpIsSynchronized(){
  return ntpSynchronized;
}

static bool ntpHasValidSystemTime(){
  return systemTimeValid;
}

#endif