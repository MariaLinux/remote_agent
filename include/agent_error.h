#pragma once

#include <string>
#include <utility>
#include <optional>

namespace remote_agent {
  enum class ErrorCode {
      NETWORK,
      REJECTION,
      BAD_CONFIG,
      MEMORY_ERROR,
      FILE_ALREADY_EXISTS,
      FILE_OPEN_FAILED,
      FILE_CREATE_FAILED,
      FILE_CLOSE_FAILED,
      UNARCHIVE_FAILED,
      NO_NEW_MAIL,
      SERIAL_OPEN_FAILED,
      SERIAL_NOT_CONNECTED,
      SERIAL_SET_OPTION_FAILED,
      SERIAL_READ_FAILED,
      SERIAL_WRITE_FAILED,
      SERIAL_CLOSE_FAILED,
      SERIAL_BAD_FD,
      UNKNOWN
  };
  
  using Error = std::optional<std::pair<ErrorCode, std::string>>;
}