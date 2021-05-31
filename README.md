# esp8266_warmpool

WIFI-MQTT-Bridge for Warmpool heat pump

  1. Warmpool EZ
  2. irriPool IN75
  3. Hayward ECPI 30MA


and maybe others. Please open an issue if you think yours may be compatible so we can check together.
 

Based on https://github.com/cribskip/esp8266_poolstar
  


# Goal
Control and query our new heat pump and later integrate it to Jeedom or other domotics
# Images


# Remarks / ToDos
- Compatibility
 [x] Warmpool EZ 
 [ ] irriPool IN75
 [ ] Hayward ECPI 30MA
- [x] Reverse engineer up to a usable state
  - Read
    - Tin - Tout
    - Heat - Fan
    - On - Off
    - Target
  - Write
    - On - Off
    - Target
- [ ] Complete Hardware
- [ ] Make docs readable ;-)

Currently, the sensors in the unit are published to MQTT. Target temperature and modes can be set ( on/off) but show some inconsistency still to be fixed.
- Debug is possible on Telnet (see https://github.com/JoaoLopesF/RemoteDebug)
- Upload OTA is possible

# Hardware
  * ESP8266 (NodeMcu for me)
  * RS485 Interface (MAX485 / RS485 Module TTL to RS-485 MCU)
  * Wires & Connectors

# How to implement
  - Wire TX/GPIO1 to DI
  - Wire RX/GPIO3 to RO
  - Wire GPIO2 to DE and RE (Connect all three together)
  - Hook up 3.3V Supply to ESP8266
  - Hook up 5V Supply to MAX485 Board
  - Connect A/Yellow Wire and B/Green wire to MAX485 module (if no proper data is received, try to swap the wires)
