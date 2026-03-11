#pragma once

#include <exception>
#include <map>
#include <string>

namespace znp {

class CameraException : public std::exception {
 public:
  enum ErrorCode {
    NO_ERROR = 0,
    INITIALIZATION_ERROR,
    COMMUNICATION_ERROR,
    HARDWARE_ERROR,
    CONFIGURATION_ERROR,
    RUNTIME_ERROR
  };

  explicit CameraException(ErrorCode code, const std::string& message)
      : code_(code),
        message_("Camera Exception [" + std::to_string(code) + "]: " +
                 message) {}

  const char* what() const noexcept override { return message_.c_str(); }

  ErrorCode getErrorCode() const { return code_; }

 private:
  ErrorCode code_;
  std::string message_;
};

}  // namespace znp
