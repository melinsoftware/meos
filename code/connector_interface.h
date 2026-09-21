#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <utility>

/** String type used by connection devices. Points to zero terminated wide strings. */
typedef const wchar_t* MConWString;

/** Device status output */
struct MConDeviceStatus {
  // Maximum number of messages in structure
  constexpr static int maxMessage = 32;
  // Name of device
  MConWString name = nullptr; 
  // Optional messages associated with device
  MConWString message[maxMessage] = {nullptr}; 
  // Boolean indicating of the corresponding message is a warning
  bool isWarningMessage[maxMessage] = { false };
  // The number of status messages
  int numStatusMessage = 0;
  // Boolean indicating of the device is connected and active
  bool connected = false;
  // Boolena indicating if the device can itself determine the type of punches
  // such as START, FINISH, or if it only sends a unit code that needs to be mapped
  // by the user to the appropriate function
  bool requireFunctionMapping = false;
  bool reserved1 = false;
  bool reserved2 = false;  
};

/** Initialization input/output*/
struct MConDeviceProps {
  bool useSubSecond = false; // Input. The device should use sub second precision if supported.
  bool supportAutoDetect = false; // Output. Set to true if auto detect is supported.
  bool reserved1 = false;
  bool reserved2 = false;
  int reservedParam1 = 0;
  int reservedParam2 = 0;
  int reservedParam3 = 0;
};

/** Types of punches */
enum class MConPunchType {
  Unknown = -1,
  PunchControl = 0,
  PunchStart = 1,
  PunchFinish = 2,
  PunchCheck = 3
};

/** Representation of a single punch not from card readout.
    Is typically a start time, a finish time, or a radio control */
struct MConFreePunch {
  // Type of punch, if this can be determined
  MConPunchType type = MConPunchType::Unknown;
  int code; // Control code (for PunchControl). 
            // Should be in range 0 < code < 1000
            // and use code >= 30 for controls.

  int unit; // Identifier of the timing device / unit used
  int cardNo; // Card number
  int time; // Time (in milliseconds since 00:00:00)
};

struct MConPunch {
  int code = 0; // Control code (or optionally unit id for start/finish/check)
  int time = 0; // Time (in milliseconds since 00:00:00). Use -1 if no time is available.
};

struct MConCard {
  int cardNo = 0; // Card number
  int nPunch = 0; // Number of punches in card
  MConPunch punch[192];
  MConPunch start;
  MConPunch finish;
  MConPunch check;
  wchar_t firstName[21]; // Optional, first name (zero terminated)
  wchar_t lastName[21];// Optional, last name (zero terminated)
  wchar_t club[41]; // Optional, club name (zero terminated)
  bool use12HourClock = false; // If true, a 12 hour clouck is used (instead of 24 hour)
  int miliVolt = 0; // SIAC voltage  
};

// Interface method for log output
typedef void (__cdecl *MConLogMessage)(MConWString message, bool isWarning);
// Interface method of adding a card
typedef void (__cdecl *MConAddCard)(const MConCard* card);
// Interface method for adding a punch
typedef void (__cdecl *MConAddPunch)(const MConFreePunch* punch);

#ifdef EXPORT_CONNECTOR
  #define EXPORT __declspec(dllexport)

extern "C" {
  /** Initialize library. Note that the library can be initialized several times (with different props),
      but initialization can never occur while any device is active.
      deviceHelp (optional) set to a string giving some information/instructions about the device type.
      startParameterName (optional) set to a string with information about a startup parameter.
                                    Use the form "parameter name|default value|parameter help" */
  EXPORT void __cdecl init(MConDeviceProps *deviceProps,
                           MConWString *deviceHelp,
                           MConWString *startParameterName,
                           MConAddCard addCard,
                           MConAddPunch addPunch);
  /** Get a user friendly name of the connecton type */
  EXPORT MConWString __cdecl getName();
  /** Get a short name (programming tag) of the card type. Used to distingush different card types
   *   in data exchange systems. Use a asterix as first character if not associated with a specific
   *   card type. (like "*tcp") 
   */
  EXPORT MConWString __cdecl getShortName();
  /** Enumarete available devices (up to the maxNumEntries). Set the name and boolean flag if the device is active.
   *  Return number of devices. Note: deviceIndex refers to the index of the *last* call to enumerateDevices. 
   */
  EXPORT int __cdecl enumerateDevices(int maxNumEntries, MConWString *deviceVectorDescr, bool *deviceVectorConnected);
  /** Returned detailed status for a device */
  EXPORT void __cdecl status(int deviceIndex, MConDeviceStatus *status);
  /** Optional method. Define a subset if the devices found by enumarateDevices in deviceIndices vector.
   *  return the number of entries set. The maximum is maxNumEntries.
   *  
   *  MeOS will call openConnection (listen = false, parameter = "") on all returned connections
   */
  EXPORT int __cdecl autoDetect(int maxNumEntries, int *deviceIndices);
  /** Open/start a specified device. If listenOnly is true, the device can be put in listen mode even
   *  if there is no feedback from the device itself (one way communication). The parameter is an optional
   *  device parameter, and the logger can be used to output information to the user.
   * 
   *  Return values: 0 Failed. 1 successful. 2 could open connection but got no response (listenOnly was false).
   *  In case 2, the user is asked to try again in listen only mode (which should then return 1 in the same state,
   *  and keep the connection open).
   * 
   *  The connection to the device should be open only if 1 was returned.
   */
  EXPORT int __cdecl openConnection(int deviceIndex, bool listenOnly, MConWString parameter, MConLogMessage logger);
  /** Close the specified device. Note that the deviceIndex refers to the last call of enumerateDevices, and
   *  might thus not be the same as the deviecIndex used to start the device (if the number of devices is dynamic).
   *  If there is a monitor thread running (see monitorThread), the library is supposed to signal to that
   *  method to exist and closeConnection should not return until monitorThread has signaled back that it is
   *  returning immenently. (That is: after a call to close connection monitorThread should not add more card data,
   *  and should return nullptr/clean as soon as possible).
   */
  EXPORT void __cdecl closeConnection(int deviceIndex);
  /** Called on a separate thread after opening the connection. Is supposed to monitor the device and
   *  call addCard/addPunch on incoming data. If the device connection is lost, it should return and optionally
   *  return an information message about the cause of the lost connection. In this case, there will be no separate call
   *  to closeConnection, so any cleanup must be done internally.
   *
   *  If on the other hand, closeConnection is called for the device (from some other thread),
   *  this method is also required to return promptly (within a second or so) and
   *  with nullptr as return value (clean exit)
   */
  EXPORT MConWString __cdecl monitorThread(int deviceIndex);
  /** Close all open connections and exist any running monitorThread.
  */
  EXPORT void __cdecl unload();
}
#else

class DynamicConnection {  
  std::wstring name;
  std::wstring shortName;

  void (__cdecl *initPtr)(MConDeviceProps* supportProps, MConWString *deviceHelp, MConWString* startParameterName, MConAddCard addCard, MConAddPunch addPunch) = nullptr;
  int (__cdecl *enumerateDevicesPtr)(int maxNumEntries, MConWString* deviceVectorDescr, bool* deviceVectorConnected) = nullptr;
  void (__cdecl *statusPtr)(int deviceIndex, MConDeviceStatus* status) = nullptr;
  int (__cdecl *autoDetectPtr)(int maxNumEntries, int *deviceIx) = nullptr;

  int (__cdecl *openConnectionPtr)(int deviceIndex, bool listenOnly, MConWString parameter, MConLogMessage logger) = nullptr;
  void (__cdecl *closeConnectionPtr)(int deviceIndex) = nullptr;
  MConWString (__cdecl *monitorThreadPtr)(int deviceIndex) = nullptr;
  void(__cdecl* unloadPtr)() = nullptr;

  HMODULE hModule = 0;

  FARPROC loadProc(const char* name, bool allowMissing = false) const;

  std::wstring parameterName;
  std::wstring parameterDefault;
  std::wstring parameterHelp;

  std::wstring deviceHelpStr;

  bool supportAuto = false;

  int numDevices = 0;

public:

  struct DeviceStatus {
    std::wstring name;
    std::vector<std::pair<std::wstring, bool>> messages;
    bool connected = false;
    bool requireFunctionMapping = false;    
  };

  DynamicConnection(const std::wstring& library);
  ~DynamicConnection();

  void unload();
  void init(bool useSubSeconds, MConAddCard addCard, MConAddPunch addPunch);

  std::vector<std::pair<std::wstring, bool>> enumerateDevices();
  void status(int deviceIndex, DeviceStatus &status) const;
  std::vector<int> autoDetect();
  int openConnection(int deviceIndex, bool listenOnly, const std::wstring &parameter, MConLogMessage logger);
  void closeConnection(int deviceIndex);
  std::wstring monitorThread(int deviceIndex);

  const std::wstring& getName() const {
    return name;
  }

  const std::wstring& getShortName() const {
    return shortName;
  }

  const std::wstring &deviceHelp() const {
    return deviceHelpStr;
  }

  bool needInputParameter(std::wstring &name, std::wstring &defValue, std::wstring &help) const;

  std::wstring getDeviceName(int deviceIndex) const;

  bool hasAnyOpenUnkownUnit() const;
  bool supportAutoDetect() const {
    return autoDetectPtr != nullptr && supportAuto;
  }
};

#endif
