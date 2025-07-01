#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <termios.h>
#include <thread>
#include <utility>
#include <vector>

#include "agent_error.h"

namespace remote_agent::serial {
  enum class Parity {
    NONE,
    ODD,
    EVEN
  };
  
  enum class AccessMode {
    READ_ONLY  = 0,
    WRITE_ONLY = 1,
    READ_WRITE = 2,
    NONE       = 255
  };
  
  enum class Handshake {
    NONE,
    SOFTWARE,
    HARDWARE,
    BOTH
  };
  
  enum class DataBits {
    FIVE,
    SIX,
    SEVEN,
    EIGHT
  };
  
  enum class StopBits {
    ONE,
    ONE_POINT_FIVE,
    TWO
  };
  
  using Result = std::pair<intmax_t, Error>;
  using ReadCallback = std::function<void(std::shared_ptr<std::vector<uint32_t>>)>;
  
  class SerialPort {
    public:
    SerialPort (const std::string& port);
    ~SerialPort();
    
    Error Open();
    Error Close();
    Result Write(const std::vector<uint8_t>& data);
    Result Write(const std::string& data);
    Result Read(std::vector<uint8_t>& data, const size_t length = 1024, const uint32_t timeout_ms = 0);
    Error enable_async_read(ReadCallback callback);
    Error disable_async_read();
    
    void SetDefaults();
    
    Error SetBaudrate(const uint32_t baudrate);
    Error SetParity(const Parity parity);
    Error SetAccessMode(const AccessMode mode);
    Error SetDatabits(const DataBits databits);
    Error SetStopbits(const StopBits stopbits);
    Error SetEcho(const bool on);
    Error SetHandshake(const Handshake handshake);
    Error GetBaudrate(uint32_t& baudrate);
    Error GetParity(Parity& parity);
    Error get_access_mode(AccessMode& mode);
    Error get_databits(DataBits& databits);
    Error get_stopbits(StopBits& stopbits);
    Error get_echo(bool& on);
    Error get_handshake(Handshake& handshake);
    
    Error get_file_descriptor();
    
    private:
      std::string _port;
      AccessMode _mode;
      struct termios _config;
      struct termios _old_config;
      std::unique_ptr<std::thread> _thread;
      int _fd;
    
  };
}