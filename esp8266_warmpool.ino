/*
  ### COMMUNICATION CYCLE ###
Permanance :
  Trame 62 (8)  02 03 0b  b9  0 5a  16  03

Pump :
  Trame 243 (189) 001007d1005ab457463139303830333033313500000000010107d100010001000000000000000000000000000b01b300000000fdda00000000000000000000000000000000000000000030000000000000000000000000000000000000000000ac00b300bb004c00d301e8000000000000000000005aa500000000000000600000003c0000000000ad00000000000000000000000000000000000000000000000000000000000000000000001b023900000000000000000000000081ab

Ctrl :
  Diffusion heure
  Trame 247 (193) 01030bb9005a16300103b457463139303830333033313500000000010107d1000000000000000000150051000200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003ecd
  Ordre
  Trame 247 (193) 010303e9005a14410103b457463139303830333033313500000000010107d10001000200e6000100010000000000010001001e001e00550032001400c000000050002d00c80000009600320046015e01ae00460096ff9c005a015e01ae005000c800000014ff88003c00000190009600960046006400140064001e00c8005a012c006e000000640320003202bc0032025800000006003201f40000000000320258000000010000000000000002001e000300010000000a000c000f001100004b64
 

*/
#define XtoInt(x,y) (int16_t)word(msg[x], msg[y])

#if defined(ESP8266)
#define WIFI_SSID "ERLIPA2" // YOUR WIFI SSID
#define WIFI_PASSWORD "r0ller0n0511" // YOUR WIFI PASSWORD
#define BROKER "192.168.1.11" // YOUR MQTT BROKER IP

#define ON String(F("ON")).c_str()
#define OFF String(F("OFF")).c_str()

#define PUB(x,y) mqtt.publish(x, y)

#define DEBUG true

#define WARMPOOL_RTS 2
#define WARMPOOL_TX 1
#define WARMPOOL_RX 3

//#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <RemoteDebug.h>
#include <CRCx.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <RingBuf.h>            //https://github.com/Locoduino/RingBuffer


ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


// ---- MQTT client --------------------------------------------------------------

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ----             --------------------------------------------------------------

SoftwareSerial PS;

#endif

int TypeMessage = 0;

unsigned long lastCharTime = 0;
unsigned long lastMsgTime = 0;
unsigned long nextState = 0;

struct {
  uint8_t dirty: 1;
  uint8_t sub: 2;
  uint8_t fill: 5;
} state;
uint8_t b_tmp;


RingBuf<uint8_t, 200> msg;
uint8_t m_target = 0xFF;
uint8_t m_mode = 0xFF;

#if defined(ESP8266)
void mqtt_reconnect() {
  int oldstate = mqtt.state();

  // Loop until we're reconnected
  if (!mqtt.connected()) {
    // Attempt to connect
    if (mqtt.connect(String(String("WARMPOOL") + String(millis())).c_str())) {
      mqtt.publish("WARMPOOL/node", "ON", true);
      mqtt.publish("WARMPOOL/debug", "RECONNECT");
      mqtt.publish("WARMPOOL/debug", String(millis()).c_str());
      mqtt.publish("WARMPOOL/debug", String(oldstate).c_str());

      // ... and resubscribe
      mqtt.subscribe("WARMPOOL/command/#");
      mqtt.subscribe("WARMPOOL/set/#");
    }
  }
}
#endif
RemoteDebug Debug;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void setup() {
  state.sub = 0x00;
  m_target = 30;
  m_mode = 0x80;
  state.dirty = false;

  // high speed half duplex, turn off interrupts during tx
#if defined(ESP8266)
  pinMode(WARMPOOL_RTS, OUTPUT);
  digitalWrite(WARMPOOL_RTS, LOW);
  PS.begin(9600, SWSERIAL_8N1, WARMPOOL_RX, WARMPOOL_TX, false);
#endif

#if defined(ESP8266)
//Serial.begin(115200);
  // Init WiFi
  {
    WiFi.setOutputPower(20.5); // this sets wifi to highest power
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long timeout = millis() + 10000;

    while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
      yield();
    }

    // Reset because of no connection
    if (WiFi.status() != WL_CONNECTED) {
      //      hardreset();
    }
  }

  // Init HTTP-Update-Server
  httpUpdater.setup(&httpServer, "admin", "");
  httpServer.begin();
  MDNS.begin("WARMPOOL");
  MDNS.addService("http", "tcp", 80);
  //init OTA
      // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Warmpool OTA ESP8266");

  // No authentication by default
  //ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  // Init MQTT
  mqtt.setServer(BROKER, 1883);
  mqtt.setCallback(callback);
#endif
  lastCharTime = millis();

  mqtt_reconnect();
  
    // Initialize RemoteDebug

  Debug.begin("Warmpool"); // Initialize the WiFi server
  Debug.setResetCmdEnabled(true); // Enable the reset command
  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors

    // End off setup
  
  
}

///////////////////////////////////////////////////////////////////////////////

// see 99_loop

///////////////////////////////////////////////////////////////////////////////

void loop() {
#if defined(ESP8266)
  if (WiFi.status() != WL_CONNECTED) ESP.restart();
  if (!mqtt.connected()) mqtt_reconnect();
  _yield();
#endif


  // Message retrival
  while (!valid_msg()) {
  Debug.handle();
  ArduinoOTA.handle();
    _yield();
    bool entete=false;
    //rdebugV(".");
    if (PS.available()) {
      rdebugV("X");
      lastCharTime = millis();
      char x = PS.read();
      if (msg.size() == 0) {
          if (x == 0x00 || x == 0x01 || x == 0x02) {
            msg.push(x);
            debugD("Détection trame %s ", String(x, HEX).c_str());
           // PUB("WARMPOOL/got", String(x, HEX).c_str());
          }
      } else if (msg.size() > 0) {
        msg.push(x);
        rdebugD("%s",String(x, HEX).c_str());
        if (msg.size() > 195) {
            //PUB("WARMPOOL/debug", "droped");
            debugD("Message trop long - réinitialisation");
            msg.clear();
        }
      }

      if (msg.size() == 2) {
        if (msg[0]==0x00 and msg[1]==0x10) {
          debugD("Write broadcast");
          entete=true;
        }
        if (msg[0]==0x01 and msg[1]==0x10) {
          debugD("Write 01");
          entete=true;
        }
        if (msg[0]==0x02 and msg[1]==0x10) {
          debugD("Write 02");
          entete=true;
        }
        if (msg[0]==0x01 and msg[1]==0x03) {
          debugD("Read Olding 01");
          entete=true;
        }
        if (msg[0]==0x02 and msg[1]==0x03) {
          debugD("Read Olding 02");
          entete=true;
        }
        
        if (!entete) {
            
            debugD("Mauvais entete %s %s", String(msg[0], HEX).c_str(), String(msg[1], HEX).c_str());
           msg.clear();
        }
      }
    }
  }
  
  _yield();
 
  String msg_dump;
  uint8_t sz = msg.size();
  for (uint8_t i = 0; i < sz; i++) {
    if (msg[i] < 0x10) msg_dump += "0";
    msg_dump += String(msg[i], HEX);
    msg_dump += " ";
  }
  

//#if true
  // Output message to MQTT
  msg_dump.toUpperCase();
  debugW("Message complet de taille %d : %s",sz,msg_dump.c_str());
 // PUB("WARMPOOL/msg", msg_dump.c_str());
//#endif

  _yield();
  
  if (sz==8) {
    TypeMessage=0;
    if (msg_dump.startsWith("01 03 03 E9")) TypeMessage=1; // Réponse lecture Consigne
    if (msg_dump.startsWith("01 03 0B B9")) TypeMessage=2; // Réponse lecture Heure
    if (msg_dump.startsWith("01 03 04 43")) TypeMessage=3; // Réponse ?
    if (msg_dump.startsWith("01 10 0B B9")) TypeMessage=0; // Réponse Ecriture OK
    if (TypeMessage!=0) debugW("Type message %d",TypeMessage);
    debugW("Type message %d",TypeMessage);
    
  } else if (msg_dump.startsWith("00 10 07 D1")) { // températures
     PUB("WARMPOOL/R10", String(XtoInt(27, 28)).c_str());  
     PUB("WARMPOOL/R11", String(XtoInt(29, 30)).c_str());
     PUB("WARMPOOL/R18", String(XtoInt(43, 44)).c_str());
     PUB("WARMPOOL/R19", String(XtoInt(45, 46)).c_str());
     PUB("WARMPOOL/R22", String(XtoInt(51, 52)).c_str());
     PUB("WARMPOOL/R33", String(XtoInt(73, 73)).c_str());
    PUB("WARMPOOL/Tin", String(XtoInt(97, 98)).c_str());
    PUB("WARMPOOL/Tout", String(XtoInt(99, 100)).c_str());
    PUB("WARMPOOL/T1", String(XtoInt(95, 96)).c_str());
    PUB("WARMPOOL/T4", String(XtoInt(101, 102)).c_str());
    PUB("WARMPOOL/T5", String(XtoInt(103, 104)).c_str());
    PUB("WARMPOOL/T6", String(XtoInt(105, 106)).c_str());
    PUB("WARMPOOL/R55", String(XtoInt(117, 118)).c_str());
    
    PUB("WARMPOOL/R59", String(XtoInt(125, 126)).c_str());
    PUB("WARMPOOL/R61", String(XtoInt(129, 130)).c_str());
    PUB("WARMPOOL/R64", String(XtoInt(135, 136)).c_str());
    PUB("WARMPOOL/R82", String(XtoInt(171, 172)).c_str());
    PUB("WARMPOOL/R83", String(XtoInt(173, 174)).c_str());
    debugW("Ecriture Broadcast 2001 Temp");

  } else if (msg_dump.startsWith("01 03 B4")) {  // traitement réponses
    if (TypeMessage==2) {
      uint8_t data[] = {msg[32], msg[34], msg[36]};
      String s;
      for (uint8_t x : data) {
      s += String(x, HEX) + " ";
      }
      PUB("WARMPOOL/heure", s.c_str());
      TypeMessage=0;
      debugW("Réponse 3001 Heure");
    }
    if (TypeMessage==1) {
      PUB("WARMPOOL/Target", String(XtoInt(27, 28)).c_str());
      TypeMessage=0;
      debugW("Réponse 1001");
    }
    if (TypeMessage==3) {
       debugW("Réponse 1091");
    }
       
  
  } else if (msg_dump.startsWith("01 10 0B B9 00 0B 16")) { //action
    debugW("Ecriture 3001");

  } 
  mqtt.loop();

  nextState = millis() + 5000;
  msg.clear();
}
