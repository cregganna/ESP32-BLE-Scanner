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

int scanTime = 30; // seconds
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
    Serial.printf("[%s] ", advertisedDevice.getAddress().toString().c_str());
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
  Serial.println("Scanning...");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setInterval(625); // default 100
  pBLEScan->setWindow(625);  // default 100, less or equal setInterval value
  pBLEScan->setActiveScan(true);
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  //Serial.print("Devices found: ");
  //Serial.println(foundDevices.getCount());
  //Serial.println("Scan done!");
  pBLEScan->stop();
  pBLEScan->clearResults();
}
