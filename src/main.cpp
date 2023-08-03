/*
 * ESP-32 based BLE Scanner
 * Author: Tony.Jewell@Cregganna.Com
 * 
 * Inspiration taken from: https://github.com/pvvx/ATC_MiThermometer.git
 */
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define SCAN_INTERVAL_MILLIS 625
#define SCAN_WINDOW_MILLIS 625
#define SCAN_DURATION_SECS 30
#define SCAN_DELIVER_DUPLICATES true

// Uncomment this and set your address if you want to limit to a single device
// #define HW_FILTER BLEAddress("a4:c1:38:e1:ea:50")

// Active vs Passive Scanning
// ACTIVE: Using Active scanning causes the Scanner (us) to query the device and receive
// responses - this can be used to determine more services on the device incl.
// Device Name etc.
// PASSIVE: Setting this to false will only receive unsolicited beacon broadcasts from the
// devices - if your device periodically sends the data you want then this will be
// the most power efficient method for the remote devices. ie they do not have to
// receive requests, send responses and avoid device Tx collisions.
// #define SCAN_ACTIVELY true /* Activeliy Scan devices for their services */
#define SCAN_ACTIVELY false /* Passively scan - will only pick up unsolicited service broadcasts */

BLEScan* pBLEScan;

static char hex[] = "0123456789abcdef";
void printBuffer(uint8_t* buf, int len) {
  for (int i = 0; i < len; i++) {
    uint8_t b = buf[i];
    Serial.print(hex[b >> 4]);
    Serial.print(hex[b & 0xf]);
  }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

  void parseService(uint8_t *svcStart, uint8_t *svcEnd) {
    uint8_t *svcPtr = svcStart;
    uint8_t svcLength = svcEnd - svcStart;
    if (svcLength < 2) {
      Serial.printf("MalFormed Service length=%u: ", svcLength);
      printBuffer(svcPtr, svcLength);
      return;
    }
    printBuffer(svcPtr, svcLength);
  }

  uint8_t* parseBlock(uint8_t *start, uint8_t *endData) {
    uint8_t *blockPtr = start;
    uint8_t blockLength = *blockPtr++;
    if ((blockPtr + blockLength) > endData || blockLength < 1) {
      Serial.printf("  Malformed Block: Length=%u RemainingDataLength=%d\n", blockLength, (endData - start));
      return endData;
    }
    uint8_t *nextBlock = blockPtr + blockLength;
    uint8_t blockType = *blockPtr++;
    Serial.printf("  %3u 0x%02x ", blockLength, blockType);
    switch(blockType) {
      case 0x16:
        parseService(blockPtr, nextBlock);
        break;
      default:
        printBuffer(blockPtr, nextBlock - blockPtr);
        break;
    }
    Serial.println();
    return nextBlock;
  }

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    BLEAddress address = advertisedDevice.getAddress();
#if defined (HW_FILTER)
    if (address != HW_FILTER) { 
      return;
    }
#endif
    Serial.printf("[%s] ", address.toString().c_str());
    if (advertisedDevice.haveName()) {
      Serial.printf(" %10s", advertisedDevice.getName().c_str());
    }
    if (advertisedDevice.haveRSSI()) {
      Serial.printf(" RSSI=%d", advertisedDevice.getRSSI());
    }
    if (advertisedDevice.haveTXPower()) {
      Serial.printf(" TXP=%d", advertisedDevice.getTXPower());
    }
    if (advertisedDevice.haveAppearance()) {
      Serial.printf(" App=0x%04x", advertisedDevice.getAppearance());
    }
    if (advertisedDevice.haveManufacturerData()) {
      Serial.printf(" %s", advertisedDevice.getManufacturerData().c_str());
    }
    Serial.println();
    uint8_t *block = advertisedDevice.getPayload();
    uint8_t *endData = block + advertisedDevice.getPayloadLength();
    while (block < endData) {
      block = parseBlock(block, endData);
    }

    if (advertisedDevice.getServiceUUIDCount() > 0) {
      Serial.printf("  Services(%d):\n", advertisedDevice.getServiceUUIDCount());
        for (int i = 0; i < advertisedDevice.getServiceUUIDCount(); i++) {
          Serial.printf("    %s\n", advertisedDevice.getServiceUUID(i).toString().c_str());
        }
    }

    if (advertisedDevice.getServiceDataUUIDCount()) {
      Serial.printf("  ServiceData(%d):\n", advertisedDevice.getServiceDataUUIDCount());
        for (int i = 0; i < advertisedDevice.getServiceDataUUIDCount(); i++) {
          std::string data = advertisedDevice.getServiceData(i);
          Serial.printf("    %s %3d ", advertisedDevice.getServiceDataUUID(i).toString().c_str(), data.length());
          printBuffer((uint8_t*)data.c_str(), data.length());
          Serial.println();
        }
    }
  }
};

void setup() {
  Serial.begin(PIO_MONITOR_SPEED);
  Serial.printf("BLE %s Scanning...\n", SCAN_ACTIVELY? "Active": "Passive");
  BLEDevice::init("ESP32-BLE-Scanner");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), SCAN_DELIVER_DUPLICATES);
  pBLEScan->setInterval(SCAN_INTERVAL_MILLIS);
  pBLEScan->setWindow(SCAN_WINDOW_MILLIS);
  pBLEScan->setActiveScan(SCAN_ACTIVELY);
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION_SECS, false);
  pBLEScan->stop();
  pBLEScan->clearResults();
}
