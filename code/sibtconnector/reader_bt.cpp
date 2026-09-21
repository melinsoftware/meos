/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu

************************************************************************/

// SPORTident Reader BT (BLE) implementation using Windows Runtime APIs
// Protocol reference: reader-bt.md v1.4

#pragma comment(lib, "windowsapp")

#include "pch.h"

// WinRT headers for BLE
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>

#define EXPORT_CONNECTOR

#include "../connector_interface.h"
#include "reader_bt.h"

using namespace std;

EXPORT MConWString __cdecl getName() {
  static const wchar_t* name = L"SportIdent BT";
  return name;
}

EXPORT MConWString __cdecl getShortName() {
  static const wchar_t* shortName = L"SI";
  return shortName;
}

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Storage::Streams;

// UUIDs from the Reader BT spec
static const winrt::guid UUID_SETTINGS_SERVICE   { 0xbd510001, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };
static const winrt::guid UUID_SETTINGS_READ_CHAR { 0xbd510002, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };
static const winrt::guid UUID_SETTINGS_WRITE_CHAR{ 0xbd510003, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };

static const winrt::guid UUID_CARD_SERVICE       { 0xbd510011, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };
static const winrt::guid UUID_CARD_STATE_CHAR    { 0xbd510012, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };
static const winrt::guid UUID_CARD_DATA_CHAR     { 0xbd510013, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };

static const winrt::guid UUID_SESSIONS_SERVICE   { 0xbd510031, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };
static const winrt::guid UUID_SESSIONS_CHAR      { 0xbd510032, 0x6aec, 0x4628, {0xa1, 0x46, 0xf3, 0xe9, 0x5b, 0xc4, 0x9e, 0x62} };

// Helper: read uint16 little-endian
static uint16_t readU16(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
// Helper: read uint32 little-endian
static uint32_t readU32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// Internal stored WinRT objects
struct ReaderBTInternal {
  BluetoothLEDevice device{ nullptr };
  GattCharacteristic cardStateChar{ nullptr };
  GattCharacteristic cardDataChar{ nullptr };
  GattCharacteristic settingsReadChar{ nullptr };
  event_token cardStateToken;
  event_token cardDataToken;
};

static ReaderBTInternal *getInternal(void *p) {
  return static_cast<ReaderBTInternal*>(p);
}

ReaderBT::ReaderBT() {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

ReaderBT::~ReaderBT() {
  unload();
}

std::vector<ReaderBTDevice> ReaderBT::scan(int timeoutMs) {
  std::vector<ReaderBTDevice> results;
  std::mutex resultMutex;

  BluetoothLEAdvertisementWatcher watcher;

  // Filter by service UUID
  BluetoothLEAdvertisementFilter filter;
  BluetoothLEAdvertisement ad;
  ad.ServiceUuids().Append(UUID_SETTINGS_SERVICE);
  filter.Advertisement(ad);
  watcher.AdvertisementFilter(filter);

  watcher.ScanningMode(BluetoothLEScanningMode::Active);

  auto receivedToken = watcher.Received([&](BluetoothLEAdvertisementWatcher const &,
                                             BluetoothLEAdvertisementReceivedEventArgs const &args) {
    ReaderBTDevice dev;
    dev.name = args.Advertisement().LocalName().c_str();
    dev.bluetoothAddress = args.BluetoothAddress();

    // Format address as wstring
    wchar_t addrBuf[32];
    swprintf_s(addrBuf, L"%012llX", args.BluetoothAddress());
    dev.address = addrBuf;

    if (dev.name.empty()) {
      dev.name = L"Reader BT " + dev.address;
    }

    std::lock_guard<std::mutex> lock(resultMutex);
    // Avoid duplicates
    for (auto &existing : results) {
      if (existing.bluetoothAddress == dev.bluetoothAddress)
        return;
    }
    results.push_back(dev);
  });

  watcher.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
  watcher.Stop();
  watcher.Received(receivedToken);

  return results;
}

bool ReaderBT::connect(ReaderBTDevice &device) {
  if (device.connected.load())
    disconnect(device);

  try {
    auto bleDevice = BluetoothLEDevice::FromBluetoothAddressAsync(device.bluetoothAddress).get();
    if (!bleDevice)
      return false;

    // Get card readout service
    auto cardServiceResult = bleDevice.GetGattServicesForUuidAsync(UUID_CARD_SERVICE).get();
    if (cardServiceResult.Status() != GattCommunicationStatus::Success ||
        cardServiceResult.Services().Size() == 0)
      return false;

    auto cardService = cardServiceResult.Services().GetAt(0);

    // Get card state characteristic
    auto cardStateResult = cardService.GetCharacteristicsForUuidAsync(UUID_CARD_STATE_CHAR).get();
    if (cardStateResult.Status() != GattCommunicationStatus::Success ||
        cardStateResult.Characteristics().Size() == 0)
      return false;

    auto cardStateChar = cardStateResult.Characteristics().GetAt(0);

    // Get card data characteristic
    auto cardDataResult = cardService.GetCharacteristicsForUuidAsync(UUID_CARD_DATA_CHAR).get();
    if (cardDataResult.Status() != GattCommunicationStatus::Success ||
        cardDataResult.Characteristics().Size() == 0)
      return false;

    auto cardDataChar = cardDataResult.Characteristics().GetAt(0);

    // Subscribe to card state notifications
    auto cccdResult = cardStateChar.WriteClientCharacteristicConfigurationDescriptorAsync(
      GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
    if (cccdResult != GattCommunicationStatus::Success)
      return false;

    // Subscribe to card data notifications
    cccdResult = cardDataChar.WriteClientCharacteristicConfigurationDescriptorAsync(
      GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
    if (cccdResult != GattCommunicationStatus::Success)
      return false;

    // Store internal handles
    auto *internal = new ReaderBTInternal();
    internal->device = bleDevice;
    internal->cardStateChar = cardStateChar;
    internal->cardDataChar = cardDataChar;

    // Register notification handlers
    internal->cardStateToken = cardStateChar.ValueChanged(
      [this, &device](GattCharacteristic const &, GattValueChangedEventArgs const &args) {
        auto reader = DataReader::FromBuffer(args.CharacteristicValue());
        reader.ByteOrder(ByteOrder::LittleEndian);
        uint32_t len = reader.UnconsumedBufferLength();
        std::vector<uint8_t> data(len);
        for (uint32_t i = 0; i < len; i++)
          data[i] = reader.ReadByte();
        processNotification(device, 0x0012, data);
      });

    internal->cardDataToken = cardDataChar.ValueChanged(
      [this, &device](GattCharacteristic const &, GattValueChangedEventArgs const &args) {
        auto reader = DataReader::FromBuffer(args.CharacteristicValue());
        reader.ByteOrder(ByteOrder::LittleEndian);
        uint32_t len = reader.UnconsumedBufferLength();
        std::vector<uint8_t> data(len);
        for (uint32_t i = 0; i < len; i++)
          data[i] = reader.ReadByte();
        processNotification(device, 0x0013, data);
      });

    device.bleDevice_ = internal;
    device.connected.store(true);
    
    // Optionally get settings service
    auto settingsResult = bleDevice.GetGattServicesForUuidAsync(UUID_SETTINGS_SERVICE).get();
    if (settingsResult.Status() == GattCommunicationStatus::Success &&
        settingsResult.Services().Size() > 0) {
      auto settingsService = settingsResult.Services().GetAt(0);
      auto readCharResult = settingsService.GetCharacteristicsForUuidAsync(UUID_SETTINGS_READ_CHAR).get();
      if (readCharResult.Status() == GattCommunicationStatus::Success &&
          readCharResult.Characteristics().Size() > 0) {
        internal->settingsReadChar = readCharResult.Characteristics().GetAt(0);
        // Subscribe to settings notifications
        internal->settingsReadChar.WriteClientCharacteristicConfigurationDescriptorAsync(
          GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
      }
    }

    return true;
  }
  catch (...) {
    return false;
  }
}

void ReaderBT::disconnect(ReaderBTDevice& device) {
  if (!device.connected.load())
    return;

  device.connected.store(false);

  auto *internal = getInternal(device.bleDevice_);
  if (internal) {
    try {
      if (internal->cardStateChar) {
        internal->cardStateChar.ValueChanged(internal->cardStateToken);
        internal->cardStateChar.WriteClientCharacteristicConfigurationDescriptorAsync(
          GattClientCharacteristicConfigurationDescriptorValue::None).get();
      }
      if (internal->cardDataChar) {
        internal->cardDataChar.ValueChanged(internal->cardDataToken);
        internal->cardDataChar.WriteClientCharacteristicConfigurationDescriptorAsync(
          GattClientCharacteristicConfigurationDescriptorValue::None).get();
      }
      internal->device.Close();
    }
    catch (...) {}
    delete internal;
    device.bleDevice_ = nullptr;
  }

  device.segmentBuffer_.reset();
}

void ReaderBT::processNotification(ReaderBTDevice& device, uint16_t charId, const std::vector<uint8_t> &rawData) {
  if (rawData.size() < 4)
    return;

  auto &segmentBuffer_ = device.segmentBuffer_;
  // Parse base header
  uint16_t instructionCode = readU16(rawData.data());
  uint16_t payloadLength = readU16(rawData.data() + 2);

  if (rawData.size() < 4u + payloadLength)
    return;

  if (instructionCode == MSG_SEGMENTED) {
    // Handle segmented reassembly
    std::lock_guard<std::mutex> lock(mutex_);
    if (payloadLength < 1)
      return;

    uint8_t packetFlag = rawData[4];
    const uint8_t *segData = rawData.data() + 5;
    size_t segLen = payloadLength - 1;

    if (packetFlag == 0x01) {
      // First packet
      if (payloadLength < 5) return;
      uint32_t totalLen = readU32(rawData.data() + 5);
      segData = rawData.data() + 9;
      segLen = payloadLength - 5;

      segmentBuffer_.reset();
      segmentBuffer_.totalLength = totalLen;
      segmentBuffer_.active = true;
      segmentBuffer_.data.insert(segmentBuffer_.data.end(), segData, segData + segLen);
    }
    else if (packetFlag == 0x00) {
      // Continuation
      if (!segmentBuffer_.active) return;
      segmentBuffer_.data.insert(segmentBuffer_.data.end(), segData, segData + segLen);
    }
    else if (packetFlag == 0x02) {
      // Last packet
      if (!segmentBuffer_.active) return;
      segmentBuffer_.data.insert(segmentBuffer_.data.end(), segData, segData + segLen);

      if (segmentBuffer_.data.size() == segmentBuffer_.totalLength) {
        processMessage(device, segmentBuffer_.data);
      }
      segmentBuffer_.reset();
    }
  }
  else {
    // Non-segmented message
    processMessage(device, rawData);
  }
}

void ReaderBT::processMessage(const ReaderBTDevice& device, const std::vector<uint8_t> &message) {
  if (message.size() < 4)
    return;

  uint16_t instructionCode = readU16(message.data());
  uint16_t payloadLength = readU16(message.data() + 2);

  if (message.size() < 4u + payloadLength)
    return;

  const uint8_t *payload = message.data() + 4;

  switch (instructionCode) {
  case MSG_CARD_STATE:
    if (payloadLength >= 7) {
      uint32_t cardNumber = readU32(payload);
      uint8_t state = payload[4];
      // uint16_t codeNumber = readU16(payload + 5);

      if (state == 1 && autoReadout_) {
        // Card inserted - request complete readout
        requestCardReadout(device, true);
      }
    }
    break;

  case MSG_CARD_READOUT_MINIMAL:
  case MSG_CARD_READOUT_COMPLETE:
    decodeCardData(payload, payloadLength, instructionCode);
    // Send OK feedback
    sendFeedback(device, 1);
    break;

  default:
    break;
  }
}

void ReaderBT::decodeCardData(const uint8_t *payload, size_t len, uint16_t msgId) {
  if (len < 7 || !addCardPtr)
    return;

  uint32_t cardNumber = readU32(payload);
  // uint8_t cardFamily = payload[4];
  uint16_t punchCount = readU16(payload + 5);

  size_t punchesOffset = 7;
  size_t punchesSize = punchCount * 8;

  if (len < punchesOffset + punchesSize)
    return;

  MConCard sic;
  sic.cardNo = cardNumber;

  for (uint16_t i = 0; i < punchCount && i < 192; i++) {
    const uint8_t *punchData = payload + punchesOffset + i * 8;

    // uint8_t controlInfo = punchData[0];
    uint8_t punchType = punchData[1];
    uint16_t controlCode = readU16(punchData + 2);
    uint32_t timeInMs = readU32(punchData + 4);

    // Convert ms-in-week to seconds-in-day (approximate, ignoring day boundary for now)
    // timeInMs is ms since Sunday 00:00:00
    // Convert to time of day in tenths of seconds
    uint32_t time = timeInMs % (24 * 3600* 1000);
  
    switch (punchType) {
    case 1: // Clear
      sic.check.code = 1;
      sic.check.time = time;
      break;
    case 2: // Check
      sic.check.code = controlCode;
      sic.check.time = time;
      break;
    case 3: // Start
    case 4: // StartReserve
      sic.start.code = controlCode;
      sic.start.time = time;
      break;
    case 5: // Finish
    case 6: // FinishReserve
      sic.finish.code = controlCode;
      sic.finish.time = time;
      break;
    case 7: // Control
      if (sic.nPunch < 192) {
        sic.punch[sic.nPunch].code = controlCode;
        sic.punch[sic.nPunch].time = time;
        sic.nPunch++;
      }
      break;
    default:
      break;
    }
  }

  // Parse owner data for complete readout
  if (msgId == MSG_CARD_READOUT_COMPLETE) {
    size_t offset = punchesOffset + punchesSize;
    uint8_t cardFamily = payload[4];

    // Skip SIAC production data if applicable
    if (cardFamily == 15 && cardNumber > 8000000 && cardNumber < 8800000) {
      if (offset + 13 <= len) {
        // Extract battery voltage
        sic.miliVolt = readU16(payload + offset + 6);
        offset += 13;
      }
    }

    // Owner data
    if (offset < len) {
      // First byte is character_set
      // uint8_t charSet = payload[offset];
      offset++;

      // Parse semicolon-separated NUL-terminated string
      if (offset < len) {
        std::string ownerStr(reinterpret_cast<const char*>(payload + offset));
        // Split by semicolons
        std::vector<std::string> fields;
        size_t pos = 0;
        while (pos < ownerStr.size()) {
          size_t semicolon = ownerStr.find(';', pos);
          if (semicolon == std::string::npos) {
            fields.push_back(ownerStr.substr(pos));
            break;
          }
          fields.push_back(ownerStr.substr(pos, semicolon - pos));
          pos = semicolon + 1;
        }

        // First name, Last name
        if (fields.size() > 0) {
          std::wstring fn(fields[0].begin(), fields[0].end());
          wcsncpy_s(sic.firstName, fn.c_str(), 20);
        }
        if (fields.size() > 1) {
          std::wstring ln(fields[1].begin(), fields[1].end());
          wcsncpy_s(sic.lastName, ln.c_str(), 20);
        }
        if (fields.size() > 4) {
          std::wstring cl(fields[4].begin(), fields[4].end());
          wcsncpy_s(sic.club, cl.c_str(), 40);
        }
      }
    }
  }

  addCardPtr(&sic);
}

void ReaderBT::requestCardReadout(const ReaderBTDevice& device, bool complete) {
  if (!device.connected)
    return;

  auto *internal = getInternal(device.bleDevice_);
  if (!internal || !internal->cardDataChar)
    return;

  // Build request message: 4-byte header, no payload
  uint16_t msgId = complete ? MSG_CARD_READOUT_COMPLETE : MSG_CARD_READOUT_MINIMAL;
  std::vector<uint8_t> request(4);
  request[0] = uint8_t(msgId & 0xFF);
  request[1] = uint8_t((msgId >> 8) & 0xFF);
  request[2] = 0; // payload_length = 0
  request[3] = 0;

  try {
    DataWriter writer;
    writer.ByteOrder(ByteOrder::LittleEndian);
    for (auto b : request)
      writer.WriteByte(b);

    internal->cardDataChar.WriteValueAsync(writer.DetachBuffer(),
      GattWriteOption::WriteWithResponse).get();
  }
  catch (...) {}
}

void ReaderBT::sendFeedback(const ReaderBTDevice& device, uint8_t feedbackType) {
  if (!device.connected)
    return;

  auto *internal = getInternal(device.bleDevice_);
  if (!internal || !internal->cardDataChar)
    return;

  // Build feedback message: 4-byte header + 1 byte payload
  std::vector<uint8_t> msg(5);
  msg[0] = uint8_t(MSG_FEEDBACK & 0xFF);
  msg[1] = uint8_t((MSG_FEEDBACK >> 8) & 0xFF);
  msg[2] = 1; // payload_length = 1
  msg[3] = 0;
  msg[4] = feedbackType;

  try {
    DataWriter writer;
    writer.ByteOrder(ByteOrder::LittleEndian);
    for (auto b : msg)
      writer.WriteByte(b);

    internal->cardDataChar.WriteValueAsync(writer.DetachBuffer(),
      GattWriteOption::WriteWithResponse).get();
  }
  catch (...) {}
}


ReaderBTDevice& ReaderBT::get(int ixFind, bool create) {
  int ix = 1;
  for (auto& [key, bt] : devices) {
    if (ix == ixFind)
      return bt;
    ix++;
  }
  throw std::exception("Invalid index");
}

const ReaderBTDevice& ReaderBT::get(int ixFind) const {
  int ix = 1;
  for (auto& [key, bt] : devices) {
    if (ix == ixFind)
      return bt;
    ix++;
  }
  throw std::exception("Invalid index");
}

int ReaderBT::enumerateDevices(int maxNumEntries, MConWString* unitVectorDescr, bool* unitVectorConnected) {
  static auto name = L"SportIdent BT";

  unitVectorDescr[0] = name;
  unitVectorConnected[0] = false;

  int ix = 1;
  for (auto& [key, bt] : devices) {
    unitVectorDescr[ix] = bt.name.c_str();
    unitVectorConnected[ix] = bt.connected.load();
    ix++;
  }

  return ix;
}

void ReaderBT::status(int deviceIndex, MConDeviceStatus* status) const {
  if (deviceIndex == 0) {
    status->connected = false;
    status->name = getName();
    return;
  }
  auto &res = get(deviceIndex);
  status->connected = res.connected.load();
  status->name = res.name.c_str();
}

int ReaderBT::autoDetectToOpen(int maxNumEntries, int* toOpen) const {
  toOpen[0] = 0; // Auto detect
  return 1;
}

int ReaderBT::openConnection(int deviceIndex, MConLogMessage logger) {
  if (deviceIndex == 0) {
    // Detect and open all
    scanResult = scan(5000);
    bool any = false;
    for (auto& s : scanResult) {
      auto dev = devices.emplace(s.address, s);
      if (!dev.first->second.connected.load()) {
        if (connect(dev.first->second)) {
          any = true;
        }
      }
    }
    return any;
  }

  auto& res = get(deviceIndex, true);
  return connect(res) ? 1 : 0;
}

void ReaderBT::closeConnection(int deviceIndex) {
  if (deviceIndex == 0)
    unload();
  else {
    auto& res = get(deviceIndex, true);
    disconnect(res);
  }
}

void ReaderBT::unload() {
  for (auto& dev : devices)
    disconnect(dev.second);
}

MConWString ReaderBT::monitorThread(int deviceIndex) {
  return nullptr;
}

ReaderBT si_bt;

EXPORT int __cdecl autoDetect(int maxNumEntries, int* deviceIx) {
  return si_bt.autoDetectToOpen(maxNumEntries, deviceIx);
}

EXPORT int __cdecl enumerateDevices(int maxNumEntries, MConWString* unitVectorDescr, bool* unitVectorConnected) {
  return si_bt.enumerateDevices(maxNumEntries, unitVectorDescr, unitVectorConnected);
}

EXPORT void __cdecl status(int deviceIndex, MConDeviceStatus* status) {
  si_bt.status(deviceIndex, status);
}

EXPORT int __cdecl openConnection(int deviceIndex, bool listenOnly, MConWString parameter, MConLogMessage logger) {
  return si_bt.openConnection(deviceIndex, logger);
}

EXPORT void __cdecl closeConnection(int deviceIndex) {
  si_bt.closeConnection(deviceIndex);
}

EXPORT MConWString __cdecl monitorThread(int deviceIndex) {
  return si_bt.monitorThread(deviceIndex);
}

EXPORT void __cdecl init(MConDeviceProps* deviceProps,
  MConWString* deviceHelp,
  MConWString* startParameterName,
  MConAddCard addCard, MConAddPunch addPunch) {
  deviceProps->supportAutoDetect = true;
  
  *startParameterName = nullptr; // No parameter
  static const wchar_t* sihelp = L"info:si_btsystem";
  *deviceHelp = sihelp;
  si_bt.init(deviceProps->useSubSecond, addCard);
}

EXPORT void __cdecl unload() {
  si_bt.unload();
}

