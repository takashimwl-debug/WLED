#include "wled.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

class SwitchBotControl : public Usermod {

  private:
    bool enabled = false;
    bool initDone = false;
    
    // SwitchBot PM1 BLE Einstellungen
    String deviceMacAddress = "";  // MAC-Adresse des SwitchBot PM1
    uint8_t action = 0;      // 0=toggle, 1=on, 2=off
    
    unsigned long lastRequestTime = 0;
    uint16_t minRequestInterval = 1000; // 1 Sekunde Abstand
    
    BLEScan* pBLEScan = nullptr;
    
    static const char _name[];
    static const char _enabled[];
    static const char _deviceMac[];
    static const char _action[];

    bool sendBLECommand(uint8_t actionToSnd);
    uint8_t getCommandByte(uint8_t actionToSnd);

  public:
    inline void enable(bool en) { enabled = en; }
    inline bool isEnabled() { return enabled; }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }

    void setup() override {
      if (!enabled) return;
      
      BLEDevice::init("");
      pBLEScan = BLEDevice::getScan();
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(100);
      pBLEScan->setWindow(99);
      
      initDone = true;
      DEBUG_PRINTLN(F("SwitchBot PM1 initialized"));
    }

    void connected() override {}
    
    void loop() override {
      if (!enabled || !initDone) return;
      
      // Optional: Periodisch Status abfragen
      if (millis() - lastRequestTime > 60000) { // Alle 60 Sekunden
        // Könnte hier Status-Check implementieren
      }
    }

    /**
     * NATIVE WLED HTTP-GET INTERFACE
     * Aufrufbar über: http://<ESP-IP>/win?SB=toggle
     */
    void handleHttpGet(AsyncWebServerRequest *request) {
      if (!enabled || !initDone || deviceMacAddress.isEmpty()) {
        return request->send(400, "text/plain", "SwitchBot not configured");
      }

      if (request->hasParam("SB")) {
        String reqParam = request->getParam("SB")->value();
        uint8_t targetAction = action;

        if (reqParam == "on" || reqParam == "1")        targetAction = 1;
        else if (reqParam == "off" || reqParam == "2")  targetAction = 2;
        else if (reqParam == "toggle" || reqParam == "0") targetAction = 0;

        if (millis() - lastRequestTime > minRequestInterval) {
          bool success = sendBLECommand(targetAction);
          lastRequestTime = millis();
          
          if (success) {
            request->send(200, "text/plain", "SwitchBot PM1 Command Sent");
          } else {
            request->send(500, "text/plain", "Failed to send command");
          }
        } else {
          request->send(429, "text/plain", "Rate Limit - Please Wait");
        }
      }
    }

    void addToJsonInfo(JsonObject& root) override {
      if (!enabled) return;
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray info = user.createNestedArray(FPSTR(_name));
      info.add(F("SwitchBot PM1"));
      info.add(F("Use /win?SB=toggle"));
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_deviceMac)] = deviceMacAddress;
      top[FPSTR(_action)] = action;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;
      
      enabled = top[FPSTR(_enabled)] | false;
      deviceMacAddress = top[FPSTR(_deviceMac)] | "";
      action = top[FPSTR(_action)] | 0;
      
      return true;
    }

    void appendConfigData() override {
      oappend(F("addInfo('SwitchBot:deviceMac',1,'SwitchBot PM1 MAC Address (XX:XX:XX:XX:XX:XX)');"));
    }
};

const char SwitchBotControl::_name[] PROGMEM = "SwitchBot";
const char SwitchBotControl::_enabled[] PROGMEM = "enabled";
const char SwitchBotControl::_deviceMac[] PROGMEM = "deviceMac";
const char SwitchBotControl::_action[] PROGMEM = "action";

bool SwitchBotControl::sendBLECommand(uint8_t actionToSnd) {
  if (deviceMacAddress.isEmpty()) {
    return false;
  }

  BLEAddress bleAddr(deviceMacAddress.c_str());
  BLEClient* pClient = BLEDevice::createClient();
  
  if (!pClient->connect(bleAddr, BLE_ADDR_TYPE_RANDOM)) {
    DEBUG_PRINTLN(F("Failed to connect to SwitchBot PM1"));
    delete pClient;
    return false;
  }

  // SwitchBot Service UUID
  BLEUUID serviceUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  
  if (!pRemoteService) {
    DEBUG_PRINTLN(F("SwitchBot service not found"));
    pClient->disconnect();
    delete pClient;
    return false;
  }

  // Command Characteristic UUID
  BLEUUID charUUID("cba20002-224d-11e6-9fb8-0002a5d5c51b");
  BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(charUUID);
  
  if (!pRemoteChar) {
    DEBUG_PRINTLN(F("SwitchBot command characteristic not found"));
    pClient->disconnect();
    delete pClient;
    return false;
  }

  // Command senden (1 Byte)
  uint8_t commandByte = getCommandByte(actionToSnd);
  uint8_t command[3] = {0x57, 0x01, commandByte}; // 0x57=Switch command, 0x01=press, commandByte=action
  
  pRemoteChar->writeValue(command, 3, false);
  DEBUG_PRINTF("SwitchBot PM1 command sent: %02X\n", commandByte);

  delay(500);
  pClient->disconnect();
  delete pClient;
  return true;
}

uint8_t SwitchBotControl::getCommandByte(uint8_t actionToSnd) {
  switch (actionToSnd) {
    case 0: return 0x00; // Press (Toggle)
    case 1: return 0x01; // Turn On
    case 2: return 0x02; // Turn Off
    default: return 0x00;
  }
}

static SwitchBotControl switchbotControl;
REGISTER_USERMOD(switchbotControl);
