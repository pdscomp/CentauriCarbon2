/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-21 16:51:49
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-06 19:03:46
 * @Description  : Define exception handling objects.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#include "exception_handler.h"

namespace elegoo
{
namespace common
{
    
BaseException::BaseException(const std::string& message) : message(message) {}

const char* BaseException::what() const noexcept {
    return message.c_str();
}

PinsError::PinsError(const std::string& message)
    : BaseException("PinsError: " + message) {}
    
CommandError::CommandError(const std::string& message)
    : BaseException("CommandError: " + message) {}

Error::Error(const std::string& message) 
    : BaseException("Error: " + message) {}

DripModeEndSignalError::DripModeEndSignalError(const std::string& message) 
    : BaseException("DripModeEndSignalError: " + message) {}

BedMeshError::BedMeshError(const std::string& message) 
    : BaseException("BedMeshError: " + message) {}

ConfigParserError::ConfigParserError(const std::string& message) 
    : BaseException("ConfigParserError: " + message) {}

SerialError::SerialError(const std::string& message) 
    : BaseException("SerialError: " + message) {}

UnicodeDecodeError::UnicodeDecodeError(const std::string& message) 
    : BaseException("UnicodeDecodeError: " + message) {}

ValueError::ValueError(const std::string& message) 
    : BaseException("ValueError: " + message) {}

IOError::IOError(const std::string& message) 
    : BaseException("IOError: " + message) {}

OSError::OSError(const std::string& message) 
    : BaseException("OSError: " + message) {}

KeyError::KeyError(const std::string& message) 
    : BaseException("KeyError: " + message) {}

ImportError::ImportError(const std::string& message) 
    : BaseException("ImportError: " + message) {}

SyntaxError::SyntaxError(const std::string& message) 
    : BaseException("SyntaxError: " + message) {}

TypeError::TypeError(const std::string& message) 
    : BaseException("TypeError: " + message) {}

IndexError::IndexError(const std::string& message) 
    : BaseException("IndexError: " + message) {}

EmptyError::EmptyError(const std::string& message) 
    : BaseException("EmptyError: " + message) {}

EnumerationError::EnumerationError(const std::string& enum_name, 
    const std::string& value) 
    : BaseException("EnumerationError: Unknown value '" 
    + value + "' in enumeration '" + enum_name + "'"),
    enum_name(enum_name), value(value) {}

std::pair<std::string, std::string> EnumerationError::get_enum_params() const {
    return {enum_name, value};
}

WebRequestError::WebRequestError(const std::string& message) 
    : BaseException("WebRequestError: " + message) {}

json WebRequestError::to_dict() const {
    json errorJson;
    errorJson["error"] = "WebRequestError";
    errorJson["message"] = what();
    return errorJson;
}


MMUError::MMUError(const std::string& message) 
    : BaseException("MMUError: " + message) {}

ApiRequestError::ApiRequestError(const std::string& message) 
    : BaseException("ApiRequestError: " + message) {}

json ApiRequestError::to_dict() const {
    json errorJson;
    errorJson["error"] = "ApiRequestError";
    errorJson["message"] = what();
    return errorJson;
}

} // namespace common
} // namespace elegoo