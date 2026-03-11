/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:52
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-31 10:18:48
 * @Description  : The msgproto module in Elegoo is responsible for defining and handling
 * the message protocol used for communication between the main control computer and the
 * microcontroller unit (MCU). This module ensures that messages are correctly formatted,
 * serialized, and deserialized, allowing for efficient and reliable data exchange.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include <iomanip>
#include <iostream>
#include <algorithm>

#include "exception_handler.h"
#include "msgproto.h"
#include "printer.h"
#include "zlib.h"

// 定义消息模板
std::map<std::string, int> DefaultMessages =
    {
        {"identify_response offset=%u data=%.*s", 0},
        {"identify offset=%u count=%c", 1}};

// 定义常量
const int T_MESSAGE_MIN = 5;
const int T_MESSAGE_MAX = 64;
const int T_MESSAGE_HEADER_SIZE = 2;
const int T_MESSAGE_TRAILER_SIZE = 3;
const int T_MESSAGE_POS_LEN = 0;
const int T_MESSAGE_POS_SEQ = 1;
const int T_MESSAGE_TRAILER_CRC = 3;
const int T_MESSAGE_TRAILER_SYNC = 1;
const int T_MESSAGE_PAYLOAD_MAX = T_MESSAGE_MAX - T_MESSAGE_MIN;
const int T_MESSAGE_SEQ_MASK = 0x0f;
const int T_MESSAGE_DEST = 0x10;
const int T_MESSAGE_SYNC = 0x7e;

std::vector<uint8_t> crc16_ccitt(const std::vector<uint8_t> &buf)
{
    uint16_t crc = 0xffff;

    for (uint8_t data : buf)
    {
        data ^= crc & 0xff;
        data ^= (data & 0x0f) << 4;

        // 更新 CRC 值
        crc = ((data << 8) | (crc >> 8)) ^ (data >> 4) ^ (data << 3);
    }

    // 返回 CRC 的高位和低位，分别是 crc 的高 8 位和低 8 位
    return {static_cast<uint8_t>(crc >> 8), static_cast<uint8_t>(crc & 0xff)};
}

PT_base::PT_base()
{
    is_int = true;
    is_dynamic_string = false;
    max_length = 5;
    signed_value = false;
}

PT_base::~PT_base()
{
}

void PT_base::encode(std::vector<uint8_t> &out, const Any &strV)
{
    // SPDLOG_DEBUG("PT_base::encode!!!");
    int64_t v = std::stoll(any_cast<std::string>(strV));
    // SPDLOG_WARN("out.size:{},v:{}",out.size(),v);
    if (v >= 0xc000000 || v < -0x4000000)
    {
        out.push_back(((v >> 28) & 0x7f) | 0x80);
    }
    if (v >= 0x180000 || v < -0x80000)
    {
        out.push_back(((v >> 21) & 0x7f) | 0x80);
    }
    if (v >= 0x3000 || v < -0x1000)
    {
        out.push_back(((v >> 14) & 0x7f) | 0x80);
    }
    if (v >= 0x60 || v < -0x20)
    {
        out.push_back(((v >> 7) & 0x7f) | 0x80);
    }
    out.push_back(v & 0x7f);
    // SPDLOG_WARN("out.size:{},v:{}",out.size(),v);
}

std::pair<uint32_t, size_t> PT_base::parse(
    const std::vector<uint8_t> &s, size_t pos)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{},pos:{},s.size:{}",__func__,__LINE__,pos,s.size());
    uint8_t c = s[pos];
    pos++;
    uint32_t v = c & 0x7f;

    if ((c & 0x60) == 0x60)
    {
        v |= -0x20;
    }

    while (c & 0x80)
    {
        c = s[pos];
        pos++;
        v = (v << 7) | (c & 0x7f);
    }

    if (!signed_value)
    {
        v = static_cast<uint32_t>(v) & 0xffffffff;
    }

    return {v, pos};
}

PT_uint32::PT_uint32()
{
}

PT_int32::PT_int32()
{
    signed_value = true;
}

PT_uint16::PT_uint16()
{
    max_length = 3;
}

PT_int16::PT_int16()
{
    signed_value = true;
    max_length = 3;
}

PT_byte::PT_byte()
{
    max_length = 2;
}

PT_string::PT_string()
{
    is_int = false;
    is_dynamic_string = true;
    max_length = 64;
}

void PT_string::encode(
    std::vector<uint8_t> &out, const Any &v)
{
    // SPDLOG_DEBUG("PT_string::encode!!!");
    std::vector<uint8_t> s_strv = any_cast<std::vector<uint8_t>>(v);
    out.push_back(static_cast<uint8_t>(s_strv.size()));
    out.insert(out.end(), s_strv.begin(), s_strv.end());
}

std::pair<std::string, size_t> PT_string::parseStr(const std::vector<uint8_t> &s, size_t pos)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{},pos:{},s.size:{},s[pos]:{}",__func__,__LINE__,pos,s.size(),s[pos]);
    if (pos >= s.size())
    {
        // throw std::out_of_range("Position is out of range");
        SPDLOG_ERROR("Position is out of range");
        return {};
    }

    uint8_t l = s[pos];
    if (pos + 1 + l > s.size())
    {
        // throw std::out_of_range("Slice goes out of range");
        // SPDLOG_ERROR("Slice goes out of range");
        return {};
    }

    std::string result(s.begin() + pos + 1, s.begin() + pos + 1 + l);
    std::pair<std::string, size_t> p = std::make_pair(result, (size_t)(pos + 1 + l));
    return p;
}

Enumeration::Enumeration(std::shared_ptr<PT_base> pt,
                         const std::string &enum_name, const std::map<std::string, int> &enums)
    : pt(pt), enum_name(enum_name), enums(enums)
{
    is_int = false;
    is_dynamic_string = false;
    max_length = pt->max_length;
    for (const auto &pair : enums)
    {
        reverse_enums[pair.second] = pair.first;
    }
}

void Enumeration::encode(std::vector<uint8_t> &out, const std::string &v)
{
    // SPDLOG_DEBUG("Enumeration  encode");
    auto it = enums.find(v);
    if (it == enums.end())
    {
        // throw EnumerationError(enum_name, v); // 使用 -1 作为占位符
    }

    pt->encode(out, std::to_string(it->second));
}

void Enumeration::encode(std::vector<uint8_t> &out, int32_t v)
{
    // SPDLOG_DEBUG("Enumeration  encode");
    auto it = enums.find(std::to_string(v));
    if (it == enums.end())
    {
        // throw EnumerationError(enum_name, std::to_string(v)); // 使用 -1 作为占位符
    }
    pt->encode(out, std::to_string(it->second));
}

void Enumeration::encode(std::vector<uint8_t> &out, const Any &v)
{
    std::string tv = any_cast<std::string>(v);
    auto it = enums.find(tv);
    if (it == enums.end())
    {
        SPDLOG_ERROR("enum_name not : " + tv);
        return;
        // throw EnumerationError(enum_name, std::to_string(v)); // 使用 -1 作为占位符
    }
    pt->encode(out, std::to_string(it->second));
}

std::pair<uint32_t, size_t> Enumeration::parse(
    const std::vector<uint8_t> &s, size_t pos)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{},pos:{},s.size:{}",__func__,__LINE__,pos,s.size());
    // if(pt->is_int)
    {
        std::pair<int32_t, size_t> result = pt->parse(s, pos);
        auto it = reverse_enums.find(result.first);
        if (it == reverse_enums.end())
        {
            return {};
        }
        std::pair<uint32_t, size_t> p = std::make_pair(it->first, result.second);
        return p;
    }
    // else if(pt->is_dynamic_string)
    // {
    //     std::pair<std::vector<uint8_t>, size_t> result = ((PT_string*)(pt.get()))->parseStr(s, pos);
    //     auto it = reverse_enums.find(result.first);
    //     if (it == reverse_enums.end())
    //     {
    //         return {};
    //     }
    //     return {it->second, result.second};
    // }

    return {};
}

// std::pair<std::vector<uint8_t>, size_t> Enumeration::parse(
//     const std::string& s, size_t pos)
// {
//     SPDLOG_DEBUG("__func__:{},__LINE__:{},pos:{},s.size:{}",__func__,__LINE__,pos,s.size());

//     return {};
// }

std::map<std::string, std::shared_ptr<PT_base>> MessageTypes =
    {
        {"%u", std::make_shared<PT_uint32>()},
        {"%i", std::make_shared<PT_int32>()},
        {"%hu", std::make_shared<PT_uint16>()},
        {"%hi", std::make_shared<PT_int16>()},
        {"%c", std::make_shared<PT_byte>()},
        {"%s", std::make_shared<PT_string>()},
        {"%.*s", std::make_shared<PT_progmem_buffer>()},
        {"%*s", std::make_shared<PT_buffer>()}};

std::vector<std::pair<std::string, std::shared_ptr<PT_base>>>
lookup_params(const std::string &msgformat,
              const std::map<std::string, std::map<std::string, int>> &enumerations)
{
    std::vector<std::pair<std::string, std::shared_ptr<PT_base>>> out;
    std::istringstream iss(msgformat);
    std::string token;

    // 跳过第一个部分
    iss >> token;

    // SPDLOG_DEBUG("__func__:{},__LINE__:{},msgformat:{},token:{}",__func__,__LINE__,msgformat,token);
    while (iss >> token)
    {
        // SPDLOG_DEBUG("__func__:{},__LINE__:{},msgformat:{},token:{}",__func__,__LINE__,msgformat,token);
        auto pos = token.find('=');
        std::string name = token.substr(0, pos);
        std::string fmt = (pos != std::string::npos) ? token.substr(pos + 1) : "";

        // SPDLOG_DEBUG("__func__:{},__LINE__:{},name:{},fmt:{}",__func__,__LINE__,name,fmt);
        std::shared_ptr<PT_base> pt = MessageTypes[fmt];

        for (const std::pair<std::string, std::map<std::string, int>> &e : enumerations)
        {
            std::string suffix = "_" + e.first;
            if (name == e.first ||
                (name.size() > e.first.size() && name.rfind(suffix) == name.size() - suffix.size()))
            {
                pt = std::make_shared<Enumeration>(pt, e.first, e.second);
                break;
            }
        }
        // SPDLOG_DEBUG("__func__:{},__LINE__:{},name:{},fmt:{}",__func__,__LINE__,name,fmt);
        out.emplace_back(name, pt);
    }
    return out;
}

std::vector<std::shared_ptr<PT_base>> lookup_output_params(const std::string &msgformat)
{
    std::vector<std::shared_ptr<PT_base>> param_types;
    std::string args = msgformat;

    while (true)
    {
        auto pos = args.find('%');
        if (pos == std::string::npos)
        {
            break;
        }

        if (pos + 1 >= args.size() || args[pos + 1] != '%')
        {
            for (int i = 0; i < 4; ++i)
            {
                std::string fmt = args.substr(pos, 1 + i);
                auto it = MessageTypes.find(fmt);
                if (it != MessageTypes.end() && it->second != nullptr)
                {
                    param_types.push_back(it->second);
                    break;
                }

                if (i == 3)
                { // 如果没有找到有效类型
                    throw std::runtime_error("Invalid output format for '" + msgformat + "'");
                }
            }
        }
        args = args.substr(pos + 1);
    }
    return param_types;
}

std::string convert_msg_format(const std::string &msgformat)
{
    std::string result = msgformat;
    std::vector<std::string> patterns = {"%u", "%i", "%hu", "%hi", "%c", "%.*s", "%*s"};

    for (const auto &pattern : patterns)
    {
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != std::string::npos)
        {
            result.replace(pos, pattern.length(), "%s");
            pos += 2; // Move past the new "%s"
        }
    }

    return result;
}

std::vector<uint8_t> Format::encode(
    const std::vector<Any> &params)
{
    return std::vector<uint8_t>();
}

// std::vector<uint8_t> Format::encode_by_name(
//     const std::map<std::string, int32_t>& params)
std::vector<uint8_t> Format::encode_by_name(
    const std::map<std::string, Any> &params)
{
    return std::vector<uint8_t>();
}

std::pair<std::map<std::string, std::string>, size_t>
Format::parse(const std::vector<uint8_t> &s, size_t pos)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{},pos:{},s.size:{}",__func__,__LINE__,pos,s.size());
    SPDLOG_WARN("Format::parse");
    return std::pair<std::map<std::string, std::string>, size_t>();
}

std::string Format::format_params(
    const std::map<std::string, std::string> &params)
{
    return "";
}

MessageFormat::MessageFormat(
    const std::vector<uint8_t> &msgid_bytes, const std::string &msgformat,
    const std::map<std::string, std::map<std::string, int>> &enumerations)
{
    this->msgid_bytes = msgid_bytes;
    this->msgformat = msgformat;
    debugformat = convert_msg_format(msgformat);
    std::istringstream iss(msgformat);
    std::string first_word;
    iss >> name;
    param_names = lookup_params(msgformat, enumerations);
    for (const std::pair<std::string, std::shared_ptr<PT_base>> &result : param_names)
    {
        param_types.push_back(result.second);
        name_to_type[result.first] = result.second;
    }
}

std::vector<uint8_t> MessageFormat::encode(
    const std::vector<Any> &params)
{
    // SPDLOG_DEBUG("param_types.size():{} {} {}",param_types.size(),params.size(),msgid_bytes.size());
    std::vector<uint8_t> out(msgid_bytes);
    for (size_t i = 0; i < param_types.size(); ++i)
    {
        param_types[i]->encode(out, params[i]);
    }
    return out;
}

// std::vector<uint8_t> MessageFormat::encode_by_name(
//     const std::map<std::string, int32_t>& params)
std::vector<uint8_t> MessageFormat::encode_by_name(
    const std::map<std::string, Any> &params)
{
    // SPDLOG_DEBUG("");
    std::vector<uint8_t> out(msgid_bytes);
    for (const std::pair<std::string, std::shared_ptr<PT_base>> &result : param_names)
    {
        // SPDLOG_INFO("{} : {}___msgid_bytes.size:{}, result.first:{}", __FUNCTION__, __LINE__, msgid_bytes.size(), result.first);
        result.second->encode(out, params.at(result.first));
    }
    return out;
}

std::pair<std::map<std::string, std::string>, size_t>
MessageFormat::parse(const std::vector<uint8_t> &s, size_t pos)
{
    pos += msgid_bytes.size();
    std::map<std::string, std::string> out;
    for (const std::pair<std::string, std::shared_ptr<PT_base>> &result : param_names)
    {
        if (result.second->is_int)
        {
            std::pair<int32_t, size_t> value = result.second->parse(s, pos);
            if (result.second->signed_value)
                out[result.first] = std::to_string((int32_t)value.first);
            else
                out[result.first] = std::to_string((uint32_t)value.first);
            // out[result.first] = std::to_string(value.first);
            pos = value.second;
        }
        else if (result.second->is_dynamic_string)
        {
            std::pair<std::string, size_t> value = ((PT_string *)(result.second.get()))->parseStr(s, pos);
            out[result.first] = value.first;
            pos = value.second;
        }
        else
        {
            std::pair<int32_t, size_t> value = result.second->parse(s, pos);
            out[result.first] = std::to_string(value.first);
            pos = value.second;
        }
    }
    return {out, pos};
}

std::string s2h(const std::string &input)
{
    std::ostringstream oss;
    for (unsigned char c : input)
    {
        oss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
}

std::string MessageFormat::format_params(
    const std::map<std::string, std::string> &params)
{
    std::vector<std::string> out;
    for (const std::pair<std::string, std::shared_ptr<PT_base>> &result : param_names)
    {
        std::string v = params.at(result.first);
        if (result.second->is_dynamic_string)
        {
            v = "'" + s2h(v) + "'";
        }
        out.push_back(v);
    }

    std::string outmsg = debugformat;
    for (auto const &s : out)
    {
        size_t pos = outmsg.find("%s");
        if (pos != std::string::npos)
            outmsg.replace(pos, 2, s);
    }
    return outmsg;
}

OutputFormat::OutputFormat(
    const std::vector<uint8_t> &msgid_bytes, const std::string &msgformat)
{
    this->msgid_bytes = msgid_bytes;
    this->msgformat = msgformat;
    name = "#output";
    debugformat = convert_msg_format(msgformat);
    param_types = lookup_output_params(msgformat);
}

std::pair<std::map<std::string, std::string>, size_t>
OutputFormat::parse(const std::vector<uint8_t> &s, size_t pos)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{},pos:{},s.size:{}",__func__,__LINE__,pos,s.size());
    pos += msgid_bytes.size();
    std::vector<std::string> out;

    for (const std::shared_ptr<PT_base> &t : param_types)
    {
        if (t->is_int)
        {

            std::pair<int32_t, size_t> value = t->parse(s, pos);
            if (t->signed_value)
                out.push_back(std::to_string((int32_t)value.first));
            else
                out.push_back(std::to_string((uint32_t)value.first));
            pos = value.second;
        }
        else if (t->is_dynamic_string)
        {
            std::pair<std::string, size_t> value = ((PT_string *)t.get())->parseStr(s, pos);
            out.push_back(value.first);
            pos = value.second;
        }
        else
        {
            std::pair<int32_t, size_t> value = t->parse(s, pos);
            out.push_back(std::to_string(value.first));
            pos = value.second;
        }
    }

    std::string outmsg = debugformat;
    for (auto const &s : out)
    {
        size_t pos = outmsg.find("%s");
        if (pos != std::string::npos)
            outmsg.replace(pos, 2, s);
    }

    std::map<std::string, std::string> result;
    result["#msg"] = outmsg;
    return {result, pos};
}

std::string OutputFormat::format_params(
    const std::map<std::string, std::string> &params)
{
    return "#output \"" + params.at("#msg") + "\"";
}

UnknownFormat::UnknownFormat()
{
    name = "#unknown";
}

std::string UnknownFormat::format_params(
    const std::map<std::string, std::string> &params)
{
    return "#unknown \"" + params.at("#msg") + "\"";
}

std::pair<std::map<std::string, std::string>, size_t>
UnknownFormat::parse(const std::vector<uint8_t> &s, size_t pos)
{
    SPDLOG_WARN("UnknownFormat::parse");
    std::pair<int32_t, size_t> result = PT_int32().parse(s, pos);
    const std::vector<uint8_t> msg = s;
    return {{}, s.size() - T_MESSAGE_TRAILER_SIZE};
}

MessageParser::MessageParser(const std::string &warn_prefix)
{
    this->warn_prefix = warn_prefix;
    this->unknown = std::make_shared<UnknownFormat>();
    this->msgid_parser = std::make_shared<PT_int32>();
    init_messages(DefaultMessages);
    SPDLOG_INFO("create MessageParser success!");
}

MessageParser::~MessageParser()
{
}

int MessageParser::check_packet(const std::vector<uint8_t> &s)
{
    if (s.size() < T_MESSAGE_MIN)
    {
        return 0;
    }

    uint8_t msglen = s[T_MESSAGE_POS_LEN];
    if (msglen < T_MESSAGE_MIN || msglen > T_MESSAGE_MAX)
    {
        return -1;
    }

    uint8_t msgseq = s[T_MESSAGE_POS_SEQ];
    if ((msgseq & ~T_MESSAGE_SEQ_MASK) != T_MESSAGE_DEST)
    {
        return -1;
    }

    if (s.size() < msglen)
    {
        return 0;
    }

    if (s[msglen - T_MESSAGE_TRAILER_SYNC] != T_MESSAGE_SYNC)
    {
        return -1;
    }

    std::vector<uint8_t> msgcrc(s.begin() + msglen - T_MESSAGE_TRAILER_CRC,
                                s.begin() + msglen - T_MESSAGE_TRAILER_CRC + 2);

    std::vector<uint8_t> crc = crc16_ccitt(std::vector<uint8_t>(s.begin(),
                                                                s.begin() + msglen - T_MESSAGE_TRAILER_SIZE));

    if (crc != msgcrc)
    {
        return -1;
    }

    return msglen;
}

std::vector<std::string> MessageParser::dump(const std::vector<uint8_t> &s)
{
    uint8_t msgseq = s[T_MESSAGE_POS_SEQ];
    std::vector<std::string> out;
    std::ostringstream seq_oss;
    seq_oss << "seq: " << std::hex << std::setw(2) << std::setfill('0') << (int)msgseq;
    out.push_back(seq_oss.str());

    int32_t msgid;
    size_t param_pos;
    size_t pos = T_MESSAGE_HEADER_SIZE;
    std::map<std::string, std::string> params;
    while (true)
    {
        std::tie(msgid, param_pos) = msgid_parser->parse(s, pos);
        auto it = messages_by_id.find(msgid);
        std::shared_ptr<Format> mid = (it != messages_by_id.end()) ? it->second : unknown;
        std::tie(params, pos) = mid->parse(s, pos);
        out.push_back(mid->format_params(params));
        if (pos >= s.size() - T_MESSAGE_TRAILER_SIZE)
            break;
    }
    return out;
}

std::string MessageParser::format_params(const std::map<std::string, std::string> &params)
{
    std::ostringstream result;
    auto name_it = params.find("#name");
    std::string name = (name_it != params.end()) ? name_it->second : "";
    auto mid_it = messages_by_name.find(name);
    if (mid_it != messages_by_name.end())
    {
        return mid_it->second->format_params(params);
    }

    auto msg_it = params.find("#msg");
    if (msg_it != params.end())
    {
        result << name << " " << msg_it->second;
        return result.str();
    }
    // 默认返回整个 params 的内容
    return map_to_string(params);
}

json MessageParser::parse(const std::vector<uint8_t> &s)
{
    std::pair<int32_t, size_t> t = msgid_parser->parse(s, T_MESSAGE_HEADER_SIZE);
    auto mid_it = messages_by_id.find(t.first);
    std::shared_ptr<Format> mid = (mid_it != messages_by_id.end()) ? mid_it->second : unknown;

    std::pair<std::map<std::string, std::string>, size_t> result = mid->parse(s, T_MESSAGE_HEADER_SIZE);

    // 检查结束位置是否正确
    if (result.second != s.size() - T_MESSAGE_TRAILER_SIZE)
    {
        // throw std::runtime_error("Extra data at end of message");
        // SPDLOG_ERROR("Extra data at end of message");
        return {};
    }

    // 添加名称参数
    result.first["#name"] = mid->name;
    json jsonParams;
    for (auto it = result.first.begin(); it != result.first.end(); it++)
    {
        jsonParams[it->first] = it->second;
    }
    // return result.first;
    return jsonParams;
}

std::vector<uint8_t> MessageParser::encode_msgblock(uint8_t seq, const std::vector<uint8_t> &cmd)
{
    // 计算消息长度
    size_t msglen = T_MESSAGE_MIN + cmd.size();

    // 合并序列号
    seq = (seq & T_MESSAGE_SEQ_MASK) | T_MESSAGE_DEST;

    // 构建输出向量
    std::vector<uint8_t> out;
    out.reserve(msglen);

    out.push_back(static_cast<uint8_t>(msglen));
    out.push_back(seq);
    out.insert(out.end(), cmd.begin(), cmd.end());

    // 添加 CRC
    std::vector<uint8_t> crc = crc16_ccitt(out);
    out.push_back(static_cast<uint8_t>(crc[0] & 0xFF));
    out.push_back(static_cast<uint8_t>((crc[1] >> 8) & 0xFF));
    out.push_back(T_MESSAGE_SYNC);

    return out;
}

std::shared_ptr<Format> MessageParser::lookup_command(const std::string &msgformat)
{
    std::vector<std::string> parts =
        elegoo::common::split(elegoo::common::strip(msgformat));
    std::string msgname = parts[0];

    auto it = messages_by_name.find(msgname);
    if (it == messages_by_name.end())
    {
        throw std::runtime_error("Unknown command: " + msgname);
    }

    if (msgformat != it->second->msgformat)
    {
        throw std::runtime_error("Command format mismatch: " + msgformat + " vs " + it->second->msgformat);
    }

    return it->second;
}

int MessageParser::lookup_msgid(const std::string &msgformat)
{
    auto it = msgid_by_format.find(msgformat);
    if (it == msgid_by_format.end())
    {
        throw std::runtime_error("Unknown command: " + msgformat);
    }
    return it->second;
}

std::vector<uint8_t> MessageParser::create_command(const std::string &msg)
{
    std::vector<std::string> parts = elegoo::common::split(elegoo::common::strip(msg));
    if (parts.empty())
    {
        return {};
    }

    std::string msgname = parts[0];
    // SPDLOG_INFO("{} : {}___ msgname :{}", __FUNCTION__, __LINE__, msgname);
    auto it = messages_by_name.find(msgname);
    if (it == messages_by_name.end())
    {
        // throw std::runtime_error("Unknown command: " + msgname);
        SPDLOG_ERROR("Unknown command:{}", msgname);
        return {};
    }

    std::map<std::string, std::string> argparts;
    std::map<std::string, Any> any_argparts;
    try
    {
        for (size_t i = 1; i < parts.size(); ++i)
        {
            auto key_value = split_once(parts[i], '=');
            argparts[key_value.first] = key_value.second;
            // SPDLOG_INFO("{} : {}___ key_value.first :{} -- key_value.second:{}", __FUNCTION__, __LINE__, key_value.first, key_value.second);
        }

        for (const std::pair<std::string, std::string> &arg : argparts)
        {
            // SPDLOG_INFO("{} : {}___ arg.first:{}, arg.second:{}", __FUNCTION__, __LINE__,arg.first , arg.second);
            std::shared_ptr<PT_base> t = it->second->name_to_type[arg.first];
            Any tval;

            if (t->is_dynamic_string)
            {
                // SPDLOG_INFO("{} : {}___ arg.second:{}", __FUNCTION__, __LINE__, arg.second);
                tval = parse_buffer(arg.second);
            }
            else if (t->is_int)
            {
                tval = arg.second;
            }
            else
            {
                tval = arg.second;
            }
            // SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
            any_argparts[arg.first] = tval;
        }
        // SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
    }
    catch (const std::exception &e)
    {
        throw;
    }
    catch (...)
    {
        throw std::runtime_error("Unable to extract params from: " + msgname);
    }

    try
    {
        return it->second->encode_by_name(any_argparts);
    }
    catch (const std::exception &e)
    {
        throw;
    }
    catch (...)
    {
        throw std::runtime_error("Unable to encode: " + msgname);
    }
}

void MessageParser::fill_enumerations(const nlohmann::json &enumerations)
{
    for (auto jsenum : enumerations.items())
    {
        if (jsenum.value().is_object())
        {
            for (auto jsenumobj : jsenum.value().items())
            {
                if (jsenumobj.value().is_number_integer())
                {
                    // SPDLOG_INFO("jsenum.key:{},jsenumobj.key:{},jsenum.value:{}",jsenum.key(),jsenumobj.key(),(int)jsenumobj.value());
                    this->enumerations[jsenum.key()][jsenumobj.key()] = jsenumobj.value().get<int64_t>();
                }
                else if (jsenumobj.value().is_array())
                {
                    // SPDLOG_INFO("jsenumobj.value().size:{}",jsenumobj.value().size());
                    if (2 == jsenumobj.value().size())
                    {
                        int64_t start = jsenumobj.value()[0].get<int64_t>();
                        int64_t count = jsenumobj.value()[1].get<int64_t>();
                        std::string enumkey = jsenumobj.key();
                        enumkey = enumkey.substr(0, enumkey.size() - 1);
                        // SPDLOG_INFO("start:{},count:{},enumkey:{}",start,count,enumkey);
                        for (auto i = 0; i < count; i++)
                        {
                            // std::string numstr = std::to_string(start + i);
                            std::string numstr = std::to_string(i);
                            this->enumerations[jsenum.key()][enumkey + numstr] = start + i;
                        }
                    }
                }
            }
        }
    }
}

std::vector<uint8_t> MessageParser::decompress_data(const std::vector<uint8_t> &data)
{
#define DECOMPDATA_SIZE 20 * 1024
    // 这里使用栈内存可能会爆栈，利用vector使用堆内存就没有大小限制
    // 根据源数据大小估算需要的解压缓冲区大小
    unsigned long compLen = data.size();
    unsigned long decompLen = DECOMPDATA_SIZE;
    // printf("decompress_data compLen:%ld, decompLen:%ld\n", compLen, decompLen);
    std::vector<uint8_t> decompdata(decompLen);
    int ret = uncompress2(decompdata.data(), &decompLen, data.data(), &compLen);
    decompdata.resize(decompLen);
#if 0
    char name[256];
    snprintf(name, sizeof(name), "/tmp/nfs/eeb001/scripts/tap/dict_%s.json", warn_prefix.c_str());
    FILE *fp = fopen(name, "wb+");
    if (fp)
    {
        fprintf(fp, "%s", decompdata.data());
        fclose(fp);
    }
#endif
    // printf("decompress_data compLen:%ld, decompLen:%ld, decompbuff:\n%s\n", compLen, decompLen, decompdata.data());
    return decompdata;
}

void MessageParser::config_to_map(const nlohmann::json &jsonconfig)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);

    for (auto jscfg : jsonconfig.items())
    {
        if (jscfg.value().is_string())
        {
            // SPDLOG_INFO("jscfg.key():{},jscfg.value():{}",jscfg.key(),(std::string)jscfg.value());
            config[jscfg.key()] = jscfg.value().get<std::string>();
        }
        else if (jscfg.value().is_number_integer())
        {
            // SPDLOG_INFO("jscfg.key():{},jscfg.value():{}",jscfg.key(),(int)jscfg.value());
            config[jscfg.key()] = std::to_string(jscfg.value().get<int64_t>());
            // SPDLOG_INFO("jscfg.key().first:{},second:{}",config.find(jscfg.key())->first,config.find(jscfg.key())->second);
        }
    }
}

void MessageParser::process_identify(const std::vector<uint8_t> &data, bool decompress)
{
    if (data.size() == 0)
    {
        return;
    }
    try
    {
        std::vector<uint8_t> decompressed_data = data;

        // Decompress data if needed
        if (decompress)
        {
            decompressed_data = decompress_data(data);
        }

        // Convert to string for JSON parsing
        std::string raw_data(decompressed_data.begin(), decompressed_data.end());
        raw_identify_data = raw_data;

        // SPDLOG_INFO("raw_identify_data.size:{},raw_identify_data:\n{}",raw_identify_data.size(),raw_identify_data);
        if (raw_identify_data.size() <= 0)
        {
            return;
        }
        // Parse JSON
        auto json_data = nlohmann::json::parse(raw_data);
        // std::cout << json_data.dump(4) << std::endl;
        fill_enumerations(json_data.value("enumerations", nlohmann::json{}));

        // Retrieve commands, responses, and output
        auto commands = json_data.value("commands", nlohmann::json{});
        auto responses = json_data.value("responses", nlohmann::json{});
        auto output = json_data.value("output", nlohmann::json{});

        // SPDLOG_INFO("commands.size:{},responses.size:{},output.size:{}",commands.size(),responses.size(),output.size());
        // Combine all messages
        std::map<std::string, int> all_messages;
        std::vector<int> command_ids;
        std::vector<int> output_ids;

        for (auto &command : commands.items())
        {
            all_messages[command.key()] = command.value();
            command_ids.push_back(command.value());
        }
        for (auto &response : responses.items())
        {
            all_messages[response.key()] = response.value();
        }
        for (auto &out : output.items())
        {
            all_messages[out.key()] = out.value();
            output_ids.push_back(out.value());
        }

        // Initialize messages
        init_messages(all_messages, command_ids, output_ids);

        // Update config and version information
        nlohmann::json jsonconfig;
        jsonconfig = json_data.value("config", nlohmann::json{});
        config_to_map(jsonconfig);
        if (json_data["version"].is_string())
        {
            version = json_data.value("version", "");
        }
        else
        {
            SPDLOG_ERROR("ERROR");
        }
        if (json_data["build_versions"].is_string())
        {
            build_versions = json_data.value("version", "");
        }
        else
        {
            SPDLOG_ERROR("ERROR");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during identify: " << e.what() << std::endl;
        throw; // Re-throw the exception after logging
    }
}

std::string MessageParser::get_raw_data_dictionary()
{
    return raw_identify_data;
}

std::pair<std::string, std::string> MessageParser::get_version_info()
{
    return {version, build_versions};
}

std::vector<std::tuple<int, std::string, std::string>> MessageParser::get_messages()
{
    return messages;
}

std::map<std::string, std::map<std::string, int>> MessageParser::get_enumerations()
{
    return enumerations;
}

std::map<std::string, std::string> MessageParser::get_constants()
{
    return config;
}

std::string MessageParser::get_constant_str(const std::string &name, std::string default_value)
{
    SPDLOG_DEBUG("name:{},config[name]:{}", name, config[name]);
    auto it = config.find(name);
    if (it == config.end())
    {
        if (!default_value.empty())
        {
            return default_value;
        }
        throw std::runtime_error("Firmware constant '" + name + "' not found");
    }

    try
    {
        return it->second;
    }
    catch (...)
    {
        throw std::runtime_error("Unable to parse firmware constant " + name + ": " + it->second);
    }
}

float MessageParser::get_constant_float(const std::string &name, const float *default_value)
{
    SPDLOG_DEBUG("name:{},config[name]:{}", name, config[name]);
    auto it = config.find(name);
    if (it == config.end())
    {
        if (default_value != nullptr)
        {
            return *default_value;
        }
        throw std::runtime_error("Firmware constant '" + name + "' not found");
    }

    try
    {
        return std::stof(it->second);
    }
    catch (...)
    {
        if (default_value != nullptr)
        {
            return *default_value;
        }
        throw std::runtime_error("Unable to parse firmware constant " + name + ": " + it->second);
    }
}

int MessageParser::get_constant_int(const std::string &name, const int *default_value)
{
    SPDLOG_DEBUG("name:{},config[name]:{}", name, config[name]);
    auto it = config.find(name);
    if (it == config.end())
    {
        if (default_value != nullptr)
        {
            return *default_value;
        }
        throw std::runtime_error("Firmware constant '" + name + "' not found");
    }

    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        if (default_value != nullptr)
        {
            return *default_value;
        }
        throw std::runtime_error("Unable to parse firmware constant " + name + ": " + it->second);
    }
}

void MessageParser::error()
{
}

std::vector<uint8_t> MessageParser::parse_buffer(const std::string &value)
{
    if (value.empty())
    {
        return {};
    }

    uint64_t tval = std::stoull(value, nullptr, 16);
    std::vector<uint8_t> out;

    for (size_t i = 0; i < value.length() / 2; ++i)
    {
        out.push_back(tval & 0xff);
        tval >>= 8;
    }

    std::reverse(out.begin(), out.end());
    return out;
}

void MessageParser::init_messages(const std::map<std::string, int> &msg,
                                  const std::vector<int> &command_ids,
                                  const std::vector<int> &output_ids)
{
    for (const std::pair<std::string, int> &result : msg)
    {
        std::string msgtype = "response";
        if (std::find(command_ids.begin(), command_ids.end(), result.second) != command_ids.end())
        {
            msgtype = "command";
        }
        else if (std::find(output_ids.begin(), output_ids.end(), result.second) != output_ids.end())
        {
            msgtype = "output";
        }

        messages.emplace_back(result.second, msgtype, result.first);
        msgid_by_format[result.first] = result.second;
        // SPDLOG_DEBUG("result.first {} ===== result.second {}", result.first, result.second);

        std::vector<uint8_t> msgid_bytes;
        // msgid_parser->encode(msgid_bytes, result.second);

        msgid_parser->encode(msgid_bytes, std::to_string(result.second));
        if (msgtype == "output")
        {
            messages_by_id[result.second] =
                std::make_shared<OutputFormat>(msgid_bytes, result.first);
        }
        else
        {
            std::shared_ptr<MessageFormat> message =
                std::make_shared<MessageFormat>(msgid_bytes, result.first, enumerations);
            messages_by_id[result.second] = message;
            messages_by_name[message->name] = message;
        }
    }
}

std::string MessageParser::map_to_string(const std::map<std::string, std::string> &params)
{
    std::ostringstream oss;
    oss << "{";
    for (auto it = params.begin(); it != params.end(); ++it)
    {
        if (it != params.begin())
        {
            oss << ", ";
        }
        oss << it->first << ": " << it->second;
    }
    oss << "}";
    return oss.str();
}

std::string MessageParser::trim(const std::string &str)
{
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> MessageParser::split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::pair<std::string, std::string> MessageParser::split_once(const std::string &arg, char delimiter)
{
    size_t pos = arg.find(delimiter);
    if (pos == std::string::npos)
    {
        // 如果没有找到等号，则返回原字符串和空字符串
        return {arg, ""};
    }

    // 返回等号前后的部分
    std::string key = arg.substr(0, pos);
    std::string value = arg.substr(pos + 1); // 等号后的部分
    return {key, value};
}

int parseValue(const std::string &value)
{
    if (value.empty())
    {
        throw std::invalid_argument("Input string is empty");
    }

    if (value.substr(0, 2) == "0x" || value.substr(0, 2) == "0X")
    {
        return std::stoi(value, nullptr, 16);
    }
    else if (value.substr(0, 2) == "0b" || value.substr(0, 2) == "0B")
    {
        int result = 0;
        for (size_t i = 2; i < value.size(); ++i)
        {
            if (value[i] == '1')
            {
                result = (result << 1) | 1;
            }
            else if (value[i] == '0')
            {
                result <<= 1;
            }
            else
            {
                throw std::invalid_argument("Invalid binary string");
            }
        }
        return result;
    }
    else
    {
        return std::stoi(value); // 十进制
    }
}