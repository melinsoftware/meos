#pragma once
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
    Eksoppsv&auml;gen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

// SPORTident Reader BT (BLE) support
// Protocol reference: reader-bt.md v1.4

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

struct SICard;
class SportIdent;

struct SegmentBuffer {
  std::vector<uint8_t> data;
  uint32_t totalLength = 0;
  bool active = false;

  void reset() { data.clear(); totalLength = 0; active = false; }
};

struct ReaderBTDevice {
  std::wstring name;
  std::wstring address; // BLE device ID string
  uint64_t bluetoothAddress = 0;
  std::atomic<bool> connected = false;

  // Platform-specific BLE handles (stored as void* to avoid WinRT headers in .h)
  void* bleDevice_ = nullptr;
  void* cardStateChar_ = nullptr;
  void* cardDataChar_ = nullptr;
  void* settingsReadChar_ = nullptr;

  SegmentBuffer segmentBuffer_;

  ReaderBTDevice() = default;
  ReaderBTDevice(const ReaderBTDevice& in) : name(in.name), address(in.address), bluetoothAddress(in.bluetoothAddress) {
  }
};

class ReaderBT {
public:
  ReaderBT();
  ~ReaderBT();

  bool isConnected(int ix) const { return get(ix).connected.load(); }
  const std::wstring& getDeviceName(int ix) const { return get(ix).name; }

  void init(bool useSubsecondModeIn, MConAddCard addCardPtrIn) {
    addCardPtr = addCardPtrIn;
    useSubsecondMode = useSubsecondModeIn;
  }

  int enumerateDevices(int maxNumEntries, MConWString* unitVectorDescr, bool* unitVectorConnected);
  void status(int deviceIndex, MConDeviceStatus* status) const;

  int autoDetectToOpen(int maxNumEntries, int* toOpen) const;
  int openConnection(int deviceIndex, MConLogMessage logger);
  void closeConnection(int deviceIndex);

  void unload();

  MConWString monitorThread(int deviceIndex);

private:

  ReaderBTDevice& get(int ix, bool create);

  const ReaderBTDevice& get(int ix) const;

  // Message IDs
  static constexpr uint16_t MSG_SEGMENTED = 0xA101;
  static constexpr uint16_t MSG_CARD_STATE = 0x1101;
  static constexpr uint16_t MSG_CARD_READOUT_MINIMAL = 0x1102;
  static constexpr uint16_t MSG_CARD_READOUT_COMPLETE = 0x1103;
  static constexpr uint16_t MSG_FEEDBACK = 0x1110;
  static constexpr uint16_t MSG_DEVICE_INFO = 0x010E;
  static constexpr uint16_t MSG_READ_ALL_SETTINGS = 0x010C;

  bool useSubsecondMode = false;
  
  MConAddCard addCardPtr = nullptr;

  // Connect to a device. Returns true on success.
  bool connect(ReaderBTDevice& device);

  // Disconnect
  void disconnect(ReaderBTDevice& device);

  // Scan for Reader BT devices (blocking, up to timeoutMs)
  std::vector<ReaderBTDevice> scan(int timeoutMs = 5000);

  std::vector<ReaderBTDevice> scanResult;
  std::map<std::wstring, ReaderBTDevice> devices;
  std::vector<std::wstring> reportedId;
  // Service/Characteristic UUIDs (as GUID)
  // Settings service: bd510001-6aec-4628-a146-f3e95bc49e62
  // Card readout service: bd510011-6aec-4628-a146-f3e95bc49e62
  // Card state char: bd510012-...
  // Card data char: bd510013-...



  void processNotification(ReaderBTDevice& device, uint16_t charId, const std::vector<uint8_t> &rawData);
  void processMessage(const ReaderBTDevice& device, const std::vector<uint8_t> &message);
  void decodeCardData(const uint8_t *payload, size_t len, uint16_t msgId);
  void requestCardReadout(const ReaderBTDevice& device, bool complete);
  void sendFeedback(const ReaderBTDevice& device, uint8_t feedbackType);


  std::mutex mutex_;

  // Event registration tokens for cleanup
  int64_t cardStateToken_ = 0;
  int64_t cardDataToken_ = 0;

  bool autoReadout_ = true; // Request readout automatically on card insert
};
