#include "serial_port.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <error.h>
#include <cstring>
#include <errno.h>
#include <optional>
#include <string>
#include <termios.h>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace remote_agent::serial {
  SerialPort::SerialPort(const std::string& port): _port{port}, _fd(0) {
    SetDefaults();
  }
  
  SerialPort::~SerialPort(){
    if (_fd > 0) {
      tcsetattr(_fd, TCSANOW, &_old_config);
    }
  }
  
  
  void SerialPort::SetDefaults() {
    _config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                             | INLCR | IGNCR | ICRNL | IXON);
    _config.c_oflag &= ~OPOST;
    _config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    _config.c_lflag |= ISIG;
    _config.c_cflag &= ~(CSIZE | PARENB);
    _config.c_cflag |= CS8;
    _config.c_cc[VTIME] = 0;
    _config.c_cc[VMIN] = 1;
  }
  
  Error SerialPort::Open() {
    Error err = std::nullopt;
    int flag;
    
    if (_fd > 0) {
      Close();
      _fd = 0;
    }
    
    switch (_mode) {
      case AccessMode::READ_ONLY:
        flag = (O_RDONLY |  O_NOCTTY | O_SYNC);
        break;
      case AccessMode::WRITE_ONLY:
        flag = (O_WRONLY |  O_NOCTTY | O_SYNC);
        break;
      case AccessMode::READ_WRITE:
        flag = (O_RDWR |  O_NOCTTY | O_SYNC);
        break;
      case AccessMode::NONE:
      default:
        flag = (O_RDONLY |  O_NOCTTY | O_SYNC);
    }
    _fd = open(_port.c_str(), flag);
    if (_fd > 0) {
      tcgetattr(_fd, &_old_config);
      tcflush(_fd, TCIOFLUSH);
      tcsetattr(_fd, TCSANOW, &_config);
    } else {
      err = std::make_pair(ErrorCode::SERIAL_OPEN_FAILED, strerror(errno));
    }
    return err;
  }
  
  Error SerialPort::Close() {
    Error err = std::nullopt;
    if (_fd > 0) {
      tcsetattr(_fd, TCSANOW, &_config);
      int ret  = close(_fd);
      if (ret == -1)
        err = std::make_pair(ErrorCode::SERIAL_CLOSE_FAILED, strerror(errno));
    } else {
      err = std::make_pair(ErrorCode::SERIAL_BAD_FD, "Bad file descriptor");
    }
    return err;
  }
  
  Result SerialPort::Write(const std::vector<uint8_t>& data) {
    intmax_t ret;
    if (_fd <= 0)
      return std::make_pair(0, std::make_pair(ErrorCode::SERIAL_BAD_FD, "Bad file descriptor"));
    ret = write(_fd, data.data(), data.size());
    if (ret < 0)
      return std::make_pair(ret, std::make_pair(ErrorCode::SERIAL_WRITE_FAILED, strerror(errno)));
    tcdrain(_fd);
    return std::make_pair(ret, std::nullopt);
  }
  
  Result SerialPort::Write(const std::string& data) {
    std::vector<uint8_t> data_vec;
    data_vec.assign(data.begin(),data.end());
    return Write(data_vec);
  }
  
  Result SerialPort::Read(std::vector<uint8_t>& data, const size_t length, const uint32_t timeout_ms) {
    intmax_t ret;
    uint32_t time = 0;
    data.reserve(length);
    if (_fd <= 0)
      return std::make_pair(0, std::make_pair(ErrorCode::SERIAL_BAD_FD, "Bad file descriptor"));
    while (time <= timeout_ms) {
      ret = read(_fd, data.data(), length);
      if (ret == -1)
        return std::make_pair(0, std::make_pair(ErrorCode::SERIAL_READ_FAILED, strerror(errno)));
      tcflush(_fd, TCIFLUSH);
      if (errno == EAGAIN || errno == EINTR || ret == 0) {
        std::this_thread::sleep_for(std::chrono::steady_clock::duration(std::chrono::milliseconds(10)));
        time += 10;
        continue;
      }
      return std::make_pair(ret, std::nullopt);
    }
    return std::make_pair(0, std::make_pair(ErrorCode::SERIAL_READ_FAILED, "Unknown error"));
  }
  
  Error SerialPort::SetBaudrate(const uint32_t baudrate) {
    speed_t speed;
    int ret;
    switch(baudrate) {
      case 50:
        speed = B50;
        break;
      case 75:
        speed = B75;
        break;
      case 110:
        speed = B110;
        break;
      case 134:
        speed = B134;
        break;
      case 150:
        speed = B150;
        break;
      case 200:
        speed = B200;
        break;
      case 300:
        speed = B300;
        break;
      case 600:
        speed = B600;
        break;
      case 1200:
        speed = B1200;
        break;
      case 2400:
        speed = B2400;
        break;
      case 4800:
        speed = B4800;
        break;
      case 9600:
        speed = B9600;
        break;
      case 19200:
        speed = B19200;
        break;
      case 38400:
        speed = B38400;
        break;
      case 57600:
        speed = B57600;
        break;
      case 115200:
        speed = B115200;
        break;
      case 230400:
        speed = B230400;
        break;
      case 460800:
        speed = B460800;
        break;
      case 500000:
        speed = B500000;
        break;
      case 576000:
        speed = B576000;
        break;
      case 921600:
        speed = B921600;
        break;
      case 1000000:
        speed = B1000000;
        break;
      case 1152000:
        speed = B1152000;
        break;
      case 2000000:
        speed = B2000000;
        break;
      case 2500000:
        speed = B2500000;
        break;
      case 3000000:
        speed = B3000000;
        break;
      case 3500000:
        speed = B3500000;
        break;
      case 4000000:
        speed = B4000000;
        break;
      
      default:
        speed = B0;
    }
    
    if (speed == B0)
      return std::make_pair(ErrorCode::SERIAL_SET_OPTION_FAILED, std::string("Bad baudrate: ") + std::to_string(baudrate));
    ret = cfsetspeed(&_config, speed);
    if (ret == 0)
      return std::nullopt;
    return std::make_pair(ErrorCode::SERIAL_SET_OPTION_FAILED, strerror(errno));
  }
  
  Error SerialPort::SetParity(const Parity parity) {
    _config.c_cflag &= ~( PARENB | PARODD );
    switch (parity) {
      case Parity::EVEN:
        _config.c_cflag |= PARENB;
        break;
      case Parity::ODD:
        _config.c_cflag |= ( PARENB | PARODD );
        break;
      case Parity::NONE:
        _config.c_cflag &= ~( PARENB | PARODD );
    }
    return std::nullopt;
  }
  
  Error SerialPort::SetAccessMode(const AccessMode mode) {
    _mode = mode;
    return std::nullopt;
  }
  
  Error SerialPort::SetDatabits(const DataBits databits) {
    switch(databits) {
      case DataBits::FIVE:
        _config.c_cflag = ( _config.c_cflag & ~CSIZE ) | CS5;
        break;
      case DataBits::SIX:
        _config.c_cflag = ( _config.c_cflag & ~CSIZE ) | CS6;
        break;
      case DataBits::SEVEN:
        _config.c_cflag = ( _config.c_cflag & ~CSIZE ) | CS7;
        break;
      case DataBits::EIGHT:
        _config.c_cflag = ( _config.c_cflag & ~CSIZE ) | CS8;
        break;
    }
    _config.c_cflag |= CLOCAL | CREAD;
    return std::nullopt;
  }
  
  Error SerialPort::SetStopbits(const StopBits stopbits) {
    switch (stopbits) {
      case StopBits::ONE:
        _config.c_cflag &= ~CSTOPB;
        break;
      case StopBits::TWO:
        _config.c_cflag |= CSTOPB;
        break;
      default:
      //TODO: 1.5 stopbits set
        break;
    }
    return std::nullopt;
  }
  
  Error SerialPort::SetEcho(const bool on) {
    if (on)
      _config.c_lflag |= ECHO;
    else
      _config.c_lflag &= ~ECHO;
    return std::nullopt;
  }
  
  Error SerialPort::SetHandshake(const Handshake handshake) {
    if (handshake == Handshake::SOFTWARE || handshake == Handshake::BOTH)
      _config.c_iflag |= IXON | IXOFF;
    else
      _config.c_iflag &= ~(IXON | IXOFF | IXANY);
    if (handshake == Handshake::HARDWARE || handshake == Handshake::BOTH)
      _config.c_cflag |= CRTSCTS;
    else
      _config.c_cflag &= ~CRTSCTS;
    return std::nullopt;
  }
  
  Error SerialPort::GetBaudrate(uint32_t& baudrate) {
    speed_t speed;
    struct termios tmp;
    if (_fd <= 0)
      return std::make_pair(ErrorCode::SERIAL_BAD_FD, "Bad file descriptor");
    tcgetattr(_fd, &tmp);
    speed = cfgetispeed(&tmp);
    switch (speed) {
      case B50:
        baudrate = 50;
        break;
      case B75:
          baudrate = 75;
          break;
      case B110:
          baudrate = 110;
          break;
      case B134:
          baudrate = 134;
          break;
      case B150:
          baudrate = 150;
          break;
      case B200:
          baudrate = 200;
          break;
      case B300:
          baudrate = 300;
          break;
      case B600:
        baudrate = 600;
        break;
      case B1200:
        baudrate = 1200;
        break;
      case B2400:
        baudrate = 2400;
        break;
      case B4800:
        baudrate = 4800;
        break;
      case B9600:
        baudrate = 9600;
        break;
      case B19200:
        baudrate = 19200;
        break;
      case B38400:
        baudrate = 38400;
        break;
      case B57600:
        baudrate = 57600;
        break;
      case B115200:
        baudrate = 115200;
        break;
      case B230400:
        baudrate = 230400;
        break;
      case B460800:
        baudrate = 460800;
        break;
      case B500000:
        baudrate = 500000;
        break;
      case B576000:
        baudrate = 576000;
        break;
      case B921600:
        baudrate = 921600;
        break;
      case B1000000:
        baudrate = 1000000;
        break;
      case B1152000:
        baudrate = 1152000;
        break;
      case B1500000:
        baudrate = 1500000;
        break;
      case B2000000:
        baudrate = 2000000;
        break;
      case B2500000:
        baudrate = 2500000;
        break;
      case B3000000:
        baudrate = 3000000;
        break;
      case B3500000:
        baudrate = 3500000;
        break;
      case B4000000:
        baudrate = 4000000;
        break;

      default:
        baudrate = 0;
        break; 
    }
    return std::nullopt;
  }
  
  Error SerialPort::GetParity(Parity& parity) {}
}
