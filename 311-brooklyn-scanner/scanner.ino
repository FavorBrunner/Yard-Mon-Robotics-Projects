/*
  ESP32 NYC 311 Brooklyn Live Scanner
  ====================================
  Real-time public data terminal pulling NYC 311 service requests
  and displaying live Brooklyn activity on an I2C LCD with buzzer alerts.
  LCD I2C: SDA=21, SCL=22 (0x27). Buzzer: GPIO25.
  API: data.cityofnewyork.us (erm2-nwe9 dataset), 60s refresh.
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
const char* ssid="YOUR_SSID";const char* password="YOUR_PASSWORD";
LiquidCrystal_I2C lcd(0x27,16,2);
const int BUZZER_PIN=25;
const char* apiHost="data.cityofnewyork.us";
const char* apiPath="/resource/erm2-nwe9.json?$select=created_date,complaint_type,descriptor,borough,incident_zip&$where=borough=%27BROOKLYN%27&$order=created_date%20DESC&$limit=15";
const unsigned long UPDATE_INTERVAL=60UL*1000UL;
unsigned long lastUpdate=0;
const unsigned long SCROLL_INTERVAL=250;
const int MAX_RECORDS=10;
int recordCount=0;
String reqTimes[10],reqDescriptions[10],reqBoros[10],reqZips[10];
String prevSignature="";
int currentRecordIndex=0;int scrollPos=0;unsigned long lastScroll=0;
String extractJsonString(const String& json,const String& key){String pattern="\""+key+"\"";int idx=json.indexOf(pattern);if(idx==-1)return"";idx=json.indexOf(":",idx);if(idx==-1)return"";idx=json.indexOf("\"",idx);if(idx==-1)return"";int start=idx+1;int end=json.indexOf("\"",start);if(end==-1)return"";return json.substring(start,end);}
String truncateTo(const String& s,uint8_t maxLen){if(s.length()<=maxLen)return s;return s.substring(0,maxLen);}
void lcdShowMessage(const String& line1,const String& line2=""){lcd.clear();lcd.setCursor(0,0);lcd.print(truncateTo(line1,16));lcd.setCursor(0,1);lcd.print(truncateTo(line2,16));}
void lcdBlinkAlert(int times,unsigned long delayMs){for(int i=0;i<times;i++){lcd.noBacklight();delay(delayMs);lcd.backlight();delay(delayMs);}}
void buzzerBeepAlert(int times,unsigned long delayMs){for(int i=0;i<times;i++){digitalWrite(BUZZER_PIN,HIGH);delay(delayMs);digitalWrite(BUZZER_PIN,LOW);delay(delayMs);}}
String buildDescription(const String& ct,const String& d){if(!ct.length()&&!d.length())return"311 Request";if(ct.length()&&!d.length())return ct;if(!ct.length()&&d.length())return d;return ct+": "+d;}
bool fetchAndStore311(){if(WiFi.status()!=WL_CONNECTED){lcdShowMessage("WiFi not ready");return false;}WiFiClientSecure client;client.setInsecure();HTTPClient https;String url=String("https://")+apiHost+apiPath;if(!https.begin(client,url)){lcdShowMessage("HTTP begin err");return false;}int httpCode=https.GET();if(httpCode!=200){lcdShowMessage("HTTP error:",String(httpCode));https.end();return false;}String payload=https.getString();https.end();if(payload.length()<10){lcdShowMessage("No data");return false;}int arrayStart=payload.indexOf('[');int arrayEnd=payload.lastIndexOf(']');if(arrayStart==-1||arrayEnd==-1||arrayEnd<=arrayStart){lcdShowMessage("Bad JSON");return false;}String body=payload.substring(arrayStart+1,arrayEnd);recordCount=0;int idx=0;while(recordCount<MAX_RECORDS){int startObj=body.indexOf('{',idx);if(startObj==-1)break;int endObj=body.indexOf('}',startObj);if(endObj==-1)break;String obj=body.substring(startObj,endObj+1);String fullDateTime=extractJsonString(obj,"created_date");String complaintType=extractJsonString(obj,"complaint_type");String descriptor=extractJsonString(obj,"descriptor");String borough=extractJsonString(obj,"borough");String incidentZip=extractJsonString(obj,"incident_zip");bool keep=true;if(borough.length()>0&&!borough.equalsIgnoreCase("BROOKLYN"))keep=false;if(keep){String timePart="??:??";if(fullDateTime.length()>=16)timePart=fullDateTime.substring(11,16);reqTimes[recordCount]=timePart;reqDescriptions[recordCount]=buildDescription(complaintType,descriptor);reqBoros[recordCount]=borough;reqZips[recordCount]=incidentZip;recordCount++;}idx=endObj+1;}if(recordCount==0){lcdShowMessage("No 311 found");return false;}String newSig=reqTimes[0]+"|"+reqDescriptions[0]+"|"+reqZips[0];bool isNew=(newSig!=prevSignature);prevSignature=newSig;return isNew;}
String boroughShort(const String& b){if(b.equalsIgnoreCase("BROOKLYN"))return"BK";if(b.equalsIgnoreCase("MANHATTAN"))return"MN";if(b.equalsIgnoreCase("BRONX"))return"BX";if(b.equalsIgnoreCase("QUEENS"))return"QN";if(b.equalsIgnoreCase("STATEN ISLAND"))return"SI";return"NYC";}
String formatTopLine(int i){String line1=reqTimes[i];if(line1.length()<5)line1="??:??";line1+=" "+boroughShort(reqBoros[i]);if(reqZips[i].length()>0&&line1.length()<11)line1+=" "+reqZips[i];return truncateTo(line1,16);}
void showImmediate311(int i){lcdShowMessage(formatTopLine(i),truncateTo(reqDescriptions[i],16));}
void setup(){Wire.begin(21,22);lcd.init();lcd.backlight();lcdShowMessage("Booting...","ESP32 311 Live");pinMode(BUZZER_PIN,OUTPUT);digitalWrite(BUZZER_PIN,LOW);WiFi.mode(WIFI_STA);WiFi.begin(ssid,password);lcdShowMessage("Connecting to","WiFi...");unsigned long t=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-t<20000){delay(500);}if(WiFi.status()==WL_CONNECTED)lcdShowMessage("WiFi connected",WiFi.localIP().toString());else lcdShowMessage("WiFi failed","Check creds");delay(2000);bool isNew=fetchAndStore311();if(recordCount>0){showImmediate311(0);if(isNew){lcdBlinkAlert(3,200);buzzerBeepAlert(3,100);}}lastUpdate=millis();lastScroll=millis();}
void loop(){unsigned long now=millis();if(now-lastUpdate>=UPDATE_INTERVAL){lcdShowMessage("Updating...","311 Brooklyn");bool isNew=fetchAndStore311();if(recordCount>0){showImmediate311(0);if(isNew){lcdBlinkAlert(3,200);buzzerBeepAlert(3,100);}}currentRecordIndex=0;scrollPos=0;lastUpdate=now;lastScroll=now;}if(recordCount>0&&(now-lastScroll>=SCROLL_INTERVAL)){String desc=reqDescriptions[currentRecordIndex];String buffer=desc;while(buffer.length()<16)buffer+=" ";buffer+="   ";int maxPos=buffer.length()-16;if(maxPos<0)maxPos=0;if(scrollPos>maxPos){scrollPos=0;currentRecordIndex=(currentRecordIndex+1)%recordCount;desc=reqDescriptions[currentRecordIndex];buffer=desc;while(buffer.length()<16)buffer+=" ";buffer+="   ";maxPos=buffer.length()-16;if(maxPos<0)maxPos=0;}String window=buffer.substring(scrollPos,scrollPos+16);lcdShowMessage(formatTopLine(currentRecordIndex),window);scrollPos++;lastScroll=now;}delay(20);}
