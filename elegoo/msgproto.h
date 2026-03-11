/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:54
 * @LastEditors  : Jack
 * @LastEditTime : 2025-01-08 15:11:59
 * @Description  : The msgproto module in Elegoo is responsible for defining and handling 
 * the message protocol used for communication between the main control computer and the 
 * microcontroller unit (MCU). This module ensures that messages are correctly formatted, 
 * serialized, and deserialized, allowing for efficient and reliable data exchange.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <map>
#include <string>
#include <cstdio>
#include <vector>
#include <exception>
#include <functional>
#include <sstream>
#include <memory>
#include "json.h"
#include "any.h"

// class Error : public std::exception 
// {
// public:
//     explicit Error(const std::string& message);
//     virtual const char* what() const noexcept override;

// private:
//     std::string msg;
// };

// class EnumerationError : public Error 
// {
// public:
//     EnumerationError(const std::string& enum_name, std::string value);
//     std::pair<std::string, std::string> get_enum_params() const;

// private:
//     std::string enum_name;
//     std::string value;
// };

std::vector<uint8_t> crc16_ccitt(const std::vector<uint8_t>& buf);

class PT_base
{
public:
    PT_base();
    ~PT_base();

    // virtual void encode(
    //     std::vector<uint8_t>& out, int32_t v);

    virtual void encode(
        std::vector<uint8_t>& out, const Any& v);

    virtual std::pair<uint32_t, size_t> parse(
        const std::vector<uint8_t>& s, size_t pos);

    bool is_int;
    bool is_dynamic_string;
    int max_length;
    bool signed_value;
};

class PT_uint32 : public PT_base 
{
public:
    PT_uint32();
};

class PT_int32 : public PT_base 
{
public:
    PT_int32();
};

class PT_uint16 : public PT_base 
{
public:
    PT_uint16();
};

class PT_int16 : public PT_base 
{
public:
    PT_int16();
};

class PT_byte : public PT_base 
{
public:
    PT_byte();
};

class PT_string : public PT_base 
{
public:
    PT_string();
    void encode(std::vector<uint8_t>& out, const Any& v);
    // std::pair<std::string, size_t> parse(
    //     const std::string& s, size_t pos);
    std::pair<std::string, size_t> parseStr(const std::vector<uint8_t>& s, size_t pos);
};

class PT_progmem_buffer : public PT_string {};

class PT_buffer : public PT_string {};

class Enumeration : public PT_base
{
public:
    Enumeration(std::shared_ptr<PT_base> pt, 
        const std::string& enum_name, 
        const std::map<std::string, int>& enums);

    void encode(
        std::vector<uint8_t>& out, int32_t v);

    void encode(
        std::vector<uint8_t>& out, const std::string& v);

    void encode(
        std::vector<uint8_t>& out, const Any& v) override;

    std::pair<uint32_t, size_t> parse(
        const std::vector<uint8_t>& s, size_t pos);

    // std::pair<std::vector<uint8_t>, size_t> parse(
    //     const std::vector<uint8_t>& s, size_t pos);

private:
    std::shared_ptr<PT_base> pt;
    std::string enum_name;
    int max_length;
    std::map<std::string, int> enums;
    std::map<int, std::string> reverse_enums;
};



std::vector<std::pair<std::string, std::shared_ptr<PT_base>>> 
    lookup_params(const std::string& msgformat, 
    const std::map<std::string, std::map<std::string, int>>& enumerations = {});


std::vector<std::shared_ptr<PT_base>> lookup_output_params(const std::string& msgformat);

std::string convert_msg_format(const std::string& msgformat);

class Format
{
public:
    virtual std::vector<uint8_t> encode(
        const std::vector<Any>& params);

    // virtual std::vector<uint8_t> encode_by_name(
    //     const std::map<std::string, int32_t>& params);
    virtual std::vector<uint8_t> encode_by_name(
        const std::map<std::string, Any>& params);

    virtual std::pair<std::map<std::string, std::string>, size_t> 
        parse(const std::vector<uint8_t> &s, size_t pos);

    virtual std::string format_params(
        const std::map<std::string, std::string>& params);

    std::vector<uint8_t> msgid_bytes;
    std::string msgformat;
    std::string debugformat;
    std::vector<std::shared_ptr<PT_base>> param_types;
    std::string name;
    std::map<std::string,  std::shared_ptr<PT_base>> name_to_type;
};

class MessageFormat : public Format
{
public:
    MessageFormat(const std::vector<uint8_t>& msgid_bytes, const std::string& msgformat,
        const std::map<std::string, std::map<std::string, int>>& enumerations = {});

    std::vector<uint8_t> encode(const std::vector<Any>& params) override;

    std::vector<uint8_t> encode_by_name(const std::map<std::string, Any>& params) override;

    std::pair<std::map<std::string, std::string>, size_t> parse(const std::vector<uint8_t> &s, size_t pos) override;

    std::string format_params(const std::map<std::string, std::string>& params) override;

private:
    std::vector<std::pair<std::string, std::shared_ptr<PT_base>>>  param_names;
};



class OutputFormat : public Format
{
public:

    OutputFormat(
        const std::vector<uint8_t>& msgid_bytes, 
        const std::string& msgformat);

    std::pair<std::map<std::string, std::string>, size_t> 
        parse(const std::vector<uint8_t> &s, size_t pos) override;

    std::string format_params(
        const std::map<std::string, std::string>& params) override;

};

class UnknownFormat : public Format
{
public:
    UnknownFormat();

    std::pair<std::map<std::string, std::string>, size_t> 
        parse(const std::vector<uint8_t> &s, size_t pos);

    std::string format_params(
        const std::map<std::string, std::string>& params) override;

    std::string repr(const std::string& v);
};

class MessageParser
{

public:
    MessageParser(const std::string& warn_prefix="");
    ~MessageParser();

    int check_packet(const std::vector<uint8_t>& s);
    std::vector<std::string> dump(const std::vector<uint8_t>& s);
    std::string format_params(const std::map<std::string, std::string>& params);
    json parse(const std::vector<uint8_t>& s);
    std::vector<uint8_t> encode_msgblock(uint8_t seq, const std::vector<uint8_t>& cmd);
    std::shared_ptr<Format> lookup_command(const std::string& msgformat);
    int lookup_msgid(const std::string& msgformat);
    std::vector<uint8_t> create_command(const std::string& msg);
    void fill_enumerations(const nlohmann::json& enumerations);
    std::vector<uint8_t> decompress_data(const std::vector<uint8_t>& data);
    void config_to_map(const nlohmann::json& jsonconfig);
    void process_identify(const std::vector<uint8_t>& data, bool decompress = true);
    std::string get_raw_data_dictionary();
    std::pair<std::string, std::string> get_version_info();
    std::vector<std::tuple<int, std::string, std::string>> get_messages();
    std::map<std::string, std::map<std::string, int>> get_enumerations(); 
    std::map<std::string, std::string> get_constants();
    std::string get_constant_str(const std::string& name, std::string default_value="");
    float get_constant_float(const std::string& name, const float* default_value=nullptr);
    int get_constant_int(const std::string& name, const int* default_value=nullptr);
private:
    void error();
    std::vector<uint8_t> parse_buffer(const std::string& value);
    void init_messages(const std::map<std::string, int>& msg, 
        const std::vector<int>& command_ids = {}, 
        const std::vector<int>& output_ids = {});
    std::string map_to_string(const std::map<std::string, std::string>& params);
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter); 
    std::pair<std::string, std::string> split_once(const std::string& arg, char delimiter);
private:
    std::shared_ptr<UnknownFormat> unknown;
    std::map<std::string, std::map<std::string, int>> enumerations;
    std::vector<std::tuple<int, std::string, std::string>> messages;
    std::map<int, std::shared_ptr<Format>> messages_by_id;
    std::map<std::string, std::shared_ptr<Format>> messages_by_name;
    std::map<std::string, int> msgid_by_format;
    std::map<std::string, std::string> config;
    std::shared_ptr<PT_int32> msgid_parser;
    std::string version;
    std::string build_versions;
    std::string raw_identify_data;
    std::string warn_prefix;
};
