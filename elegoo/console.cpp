/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:29
 * @LastEditors  : Ben
 * @LastEditTime : 2025-04-19 17:02:33
 * @Description  : The console module in Elegoo is responsible for handling 
 * user interactions through the command-line interface (CLI). It provides 
 * a way for users to send commands, view status information, and manage 
 * the printer directly from the terminal. 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "console.h"
#include <iostream>
#include <getopt.h>
#include <cstdlib>
#include <unistd.h>
#include <regex>
#include <sstream>
#include "utilities.h"
#include "msgproto.h"

const std::string help_txt = "\
  This is a debugging console for the Elegoo micro-controller.\
  In addition to mcu commands, the following artificial commands are\
  available:\
    DELAY : Send a command at a clock time (eg, \"DELAY 9999 get_uptime\")\
    FLOOD : Send a command many times (eg, \"FLOOD 22 .01 get_uptime\")\
    SUPPRESS : Suppress a response message (eg, \"SUPPRESS analog_in_state 4\")\
    SET   : Create a local variable (eg, \"SET myvar 123.4\")\
    DUMP  : Dump memory (eg, \"DUMP 0x12345678 100 32\")\
    FILEDUMP : Dump to file (eg, \"FILEDUMP data.bin 0x12345678 100 32\")\
    STATS : Report serial statistics\
    LIST  : List available mcu commands, local commands, and local variables\
    HELP  : Show this text\
  All commands also support evaluation by enclosing an expression in { }.\
  For example, \"reset_step_clock oid=4 clock={clock + freq}\".  In addition\
  to user defined variables (via the SET command) the following builtin\
  variables may be used in expressions:\
    clock : The current mcu clock time (as estimated by the host)\
    freq  : The mcu clock frequency";

KeyboardReader::KeyboardReader(std::shared_ptr<SelectReactor> reactor,
    std::string serialport, int baud, 
    std::string canbus_iface,
    int canbus_nodeid) : reactor(reactor),
    serialport(serialport), baud(baud),
    canbus_iface(canbus_iface),canbus_nodeid(canbus_nodeid)
{
    ser = std::make_shared<SerialReader>(reactor);
    start_time = get_monotonic();
    clocksync = std::make_shared<ClockSync>(reactor);
    fd = fileno(stdin);
    elegoo::common::set_nonblock(fd);
    mcu_freq = 0;

    reactor->register_fd(fd,[this](double eventtime) { 
        process_kbd(eventtime); 
    });

    reactor->register_callback([this](double eventtime) { 
        return connect(eventtime); 
    });

    local_commands["SET"] = [this](std::vector<std::string>& parts) { command_SET(parts); };
    local_commands["DUMP"] = [this](std::vector<std::string>& parts) { command_DUMP(parts); };
    local_commands["FILEDUMP"] = [this](std::vector<std::string>& parts) { command_FILEDUMP(parts); };
    local_commands["DELAY"] = [this](std::vector<std::string>& parts) { command_DELAY(parts); };
    local_commands["FLOOD"] = [this](std::vector<std::string>& parts) { command_FLOOD(parts); };
    local_commands["SUPPRESS"] = [this](std::vector<std::string>& parts) { command_SUPPRESS(parts); };
    local_commands["STATS"] = [this](std::vector<std::string>& parts) { command_STATS(parts); };
    local_commands["LIST"] = [this](std::vector<std::string>& parts) { command_LIST(parts); };
    local_commands["HELP"] = [this](std::vector<std::string>& parts) { command_HELP(parts); };

    std::cout << "create SelectReactor success!" << std::endl;
}

KeyboardReader::~KeyboardReader()
{

}

double KeyboardReader::connect(double eventtime)
{
    output(help_txt);
    output(std::string(20, '=') + " attempting to connect " + std::string(20, '='));

    if (!canbus_iface.empty()) 
    {
        ser->connect_canbus(serialport, canbus_nodeid, canbus_iface);
    } 
    else if (baud) 
    {
        ser->connect_uart(serialport, baud);
    } 
    else 
    {
        ser->connect_pipe(serialport);
    }
    
    std::shared_ptr<MessageParser> msgparser = ser->get_msgparser();
    int message_count = msgparser->get_messages().size();
    auto info = msgparser->get_version_info();
    output("Loaded " + std::to_string(message_count) + " commands (" + info.first + " / " + info.second + ")");
    
    std::ostringstream mcu_config;
    for (const auto& val : msgparser->get_constants()) {
        mcu_config << val.first << "=" << val.second << " ";
    }
    output("MCU config: " + mcu_config.str());
    clocksync->connect(ser);

    // ser->handle_default(handle_default); 
    ser->register_response([this](const json& params) 
        { handle_output(params); }, "#output");
    
    mcu_freq = msgparser->get_constant_float("CLOCK_FREQ");

    output(std::string(20, '=') + "       connected       " + std::string(20, '='));
    return reactor->NEVER;
}

void KeyboardReader::output(const std::string& msg)
{
    std::cout << msg << std::endl;
}

void KeyboardReader::handle_default(const json& params)
{
    double tdiff = params["#receive_time"].get<double>() - start_time;  
    // std::string msg = ser->get_msgparser()->format_params(params);
    // std::ostringstream output_stream;
    // output_stream << tdiff << ": " << msg;
    // output(output_stream.str());
}

void KeyboardReader::handle_output(const json& params)
{
    double tdiff = params["#receive_time"].get<double>() - start_time;  

    output(std::to_string(tdiff) + " " + params["#name"].get<std::string>() + " " + params["#msg"].get<std::string>());
}

void KeyboardReader::handle_suppress(const json& params)
{
    // 无用，后面删除
}

void KeyboardReader::update_evals(double eventtime)
{
    eval_globals["freq"] = mcu_freq;
    eval_globals["clock"] = clocksync->get_clock(eventtime);
}

void KeyboardReader::command_SET(std::vector<std::string>& parts)
{
    if (parts.size() < 3) 
    {
        std::cerr << "Error: Invalid number of arguments." << std::endl;
        return;
    }

    std::string val = parts[2];
    
    try {
        float numeric_val = std::stof(val);
        eval_globals[parts[1]] = numeric_val;
    } catch (const std::invalid_argument& e) {
        output("Error: Invalid float value");
    }
}

void KeyboardReader::command_DUMP(std::vector<std::string>& parts, const std::string& filename)
{
    int addr = std::stoi(parts.at(1), nullptr, 0);
    int count = std::stoi(parts.at(2), nullptr, 0);

    // 根据 addr 和 count 计算 order
    int order = std::vector<int>{2, 0, 1, 0}[(addr | count) & 3];

    // 检查是否有额外的参数，指定 order
    if (parts.size() > 3) 
    {
        std::map<std::string, int> map = {{"32", 2}, {"16", 1}, {"8", 0}};
        if (map.find(parts[3]) != map.end()) 
        {
            order = map[parts[3]];
        } 
        else 
        {
            output("Error: Invalid order value.");
            return;
        }
    }

    int bsize = 1 << order;
    std::vector<int> vals;

    // 计算要查询的块数
    int num_blocks = (count + bsize - 1) >> order;

    for (int i = 0; i < num_blocks; ++i) 
    {
        int caddr = addr + (i << order);
        std::string cmd = "debug_read order=" + std::to_string(order) + " addr=" + std::to_string(caddr);   
        // auto params = ser.send_with_response(cmd, "debug_result");
        // vals.push_back(params["val"]);
    }
}

void KeyboardReader::command_FILEDUMP(std::vector<std::string>& parts)
{
    if (parts.size() > 1)
    {
        std::vector<std::string> args(parts.begin() + 1, parts.end());
        std::string filename = parts[1];
        command_DUMP(args, filename);
    }
}

void KeyboardReader::command_DELAY(std::vector<std::string>& parts)
{
    int val = 0;
    try {
        val = std::stoi(parts[1]);
    } catch (const std::invalid_argument& e) {
        output("Error: Invalid integer value.");
        return;
    } catch (const std::out_of_range& e) {
        output("Error: Integer value out of range.");
        return;
    }

    std::ostringstream oss;
    for (size_t i = 2; i < parts.size(); ++i) 
    {
        if (i > 2) 
        {
            oss << " "; 
        }
        oss << parts[i];
    }
    std::string msg = oss.str();

    try {
        ser->send(msg, val);
    } catch (const std::runtime_error& e) {
        output(e.what());
    }
}

void KeyboardReader::command_FLOOD(std::vector<std::string>& parts)
{
    int count = std::stoi(parts.at(1));
    float delay = std::stof(parts.at(2));

    std::ostringstream msg_stream;
    for (size_t i = 3; i < parts.size(); ++i) 
    {
        if (i > 3) 
        {
            msg_stream << " ";  // 添加空格分隔符
        }
        msg_stream << parts[i];
    }

    std::string msg = msg_stream.str();
    int delay_clock = static_cast<int>(delay * mcu_freq);
    int msg_clock = static_cast<int>(clocksync->get_clock(get_monotonic()) + mcu_freq * 0.200);

    try {
        for (int i = 0; i < count; ++i) 
        {
            int next_clock = msg_clock + delay_clock;
            ser->send(msg, msg_clock, next_clock);
            msg_clock = next_clock;
        }
    } catch (const std::runtime_error& e) {
        output(e.what());
        return;
    }
}

void KeyboardReader::command_SUPPRESS(std::vector<std::string>& parts)
{
    try 
    {
        std::string name = parts.at(1); 

        int oid = 0;
        if (parts.size() > 2) 
        {
            oid = std::stoi(parts.at(2)); // std::stoi将字符串转换为整数
        }
    } 
    catch (const std::invalid_argument& e) 
    {
        output(e.what());  // 捕获解析整数时的异常
    } 
    catch (const std::out_of_range& e) 
    {
        output(e.what());  // 捕获越界访问异常
    } 
    catch (const std::exception& e) 
    {
        output(e.what());  // 捕获所有其他标准异常
    }

    // ser->register_response();
}

void KeyboardReader::command_STATS(std::vector<std::string>& parts)
{
    double curtime = get_monotonic();
    output(ser->stats(curtime) + " " + clocksync->stats(curtime));
}

void KeyboardReader::command_LIST(std::vector<std::string>& parts)
{
    update_evals(get_monotonic());
    //依赖msgproto实现
}

void KeyboardReader::command_HELP(std::vector<std::string>& parts)
{
    output(help_txt);
}

std::string KeyboardReader::translate(const std::string& line, double eventtime)
{
    std::string str = line;
    std::regex re_eval(R"(\{([^}]*)\})");
    std::vector<std::string> evalparts;
    std::sregex_token_iterator iter(line.begin(), line.end(), re_eval, {-1, 1});
    std::sregex_token_iterator end;

    for (; iter != end; ++iter) 
    {
        evalparts.push_back(*iter);
    }

    if(evalparts.size() > 1)
    {
        update_evals(eventtime);
        for (size_t i = 1; i < evalparts.size(); i += 2) 
        {
            double e = evaluate_expression(evalparts[i], eval_globals);

            // 如果 e 是浮点数，转换为整数
            if (e == static_cast<int>(e)) {
                e = static_cast<int>(e);
            }
            evalparts[i] = std::to_string(e);
        }
        
        str = "";
        for (const auto& part : evalparts) 
        {
            str += part;
        }
        output(str);
    }

    str = trim(str);
    if(!str.empty())
    {
        std::vector<std::string> parts = split(str);

        if (local_commands.find(parts[0]) != local_commands.end()) 
        {
            local_commands[parts[0]](parts);
            return "";  
        }
    }
    
    return str;
}

void KeyboardReader::process_kbd(double eventtime)
{
    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

    if (bytes_read > 0) 
    {
        data += std::string(buffer, bytes_read);

        std::vector<std::string> kbdlines;
        std::string line;
        std::istringstream stream(data);

        while (std::getline(stream, line)) 
        {
            kbdlines.push_back(line);
        }

        for (size_t i = 0; i < kbdlines.size() - 1; ++i) 
        {
            std::string line = kbdlines[i];
            line = trim(line);

            size_t cpos = line.find('#');
            if (cpos != std::string::npos) 
            {
                line = line.substr(0, cpos);
                if (line.empty()) {
                    continue; 
                }
            }

            std::string msg = translate(trim(line), eventtime);
            if (msg.empty()) 
            {
                continue;
            }

            try 
            {
                ser->send(msg);
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }

        data = kbdlines.back();
    }
}

std::string KeyboardReader::trim(const std::string& str, 
    bool remove_front, bool remove_back) 
{
    size_t start = 0;
    size_t end = str.length();

    if (remove_front) 
    {
        while (start < end && std::isspace(str[start])) 
        {
            ++start;
        }
    }

    if (remove_back) 
    {
        while (end > start && std::isspace(str[end - 1])) 
        {
            --end;
        }
    }

    // 返回处理后的子字符串
    return str.substr(start, end - start);
}

std::vector<std::string> KeyboardReader::split(const std::string& str) 
{
    std::vector<std::string> parts;
    std::istringstream stream(str);
    std::string part;
    while (stream >> part) {
        parts.push_back(part);
    }
    return parts;
}

// 解析表达式并计算结果
double KeyboardReader::evaluate_expression(const std::string& expr, const std::map<std::string, double>& variables) {
    std::istringstream ss(expr);
    std::string token;
    double result = 0;
    char op = '+';  // 当前操作符，初始为加法

    while (ss >> token) {
        double value;

        // 处理变量或数值
        if (variables.find(token) != variables.end()) {
            value = variables.at(token);  // 如果是变量，从map中获取值
        } else {
            try {
                value = std::stod(token);  // 尝试将token转换为double
            } catch (const std::invalid_argument&) {
                throw std::runtime_error("Unable to evaluate: " + expr);
            }
        }

        // 根据当前操作符应用计算
        switch (op) {
            case '+': result += value; break;
            case '-': result -= value; break;
            case '*': result *= value; break;
            case '/': result /= value; break;
            default: throw std::runtime_error("Unsupported operator");
        }

        // 尝试读取下一个操作符
        if (!(ss >> op)) {
            break;  // 没有更多的操作符，退出循环
        }
    }

    return result;
}


int main(int argc, char* argv[])
{
    // 变量定义
    bool verbose = false;
    int baud = 0;
    std::string canbus_iface;
    int canbus_nodeid = 64;
    int debuglevel = 0;

    const option long_options[] = {
        {"verbose", no_argument, nullptr, 'v'},
        {"baud", required_argument, nullptr, 'b'},
        {"canbus_iface", required_argument, nullptr, 'c'},
        {"canbus_nodeid", required_argument, nullptr, 'i'},
        {0, 0, 0, 0}
    };

    int opt;
    while (opt = getopt_long(argc, argv, "vb:c:i:", long_options, nullptr) != -1) 
    {
        switch (opt) 
        {
            case 'v':
                verbose = true;
                debuglevel = 1;
                break;
            case 'b':
                baud = std::atoi(optarg);
                break;
            case 'c':
                canbus_iface = optarg;
                break;
            case 'i':
                canbus_nodeid = std::atoi(optarg);
                break;
            case '?': // 处理未知选项
            default:
                std::cerr << "Usage: " << argv[0] << " [-v] [-b baud] [-c canbus_iface] [-i canbus_nodeid]\n";
                return 1;
        }
    }

    if (optind >= argc) 
    {
        std::cerr << "Incorrect number of arguments. Expected <serialdevice>\n";
        std::cerr << "Usage: " << argv[0] << " [-v] [-b baud] [-c canbus_iface] [-i canbus_nodeid] <serialdevice>\n";
        return 1;
    }

    std::string serialport = argv[optind];
    if (baud == 0 && !(serialport.rfind("/dev/rpmsg_", 0) == 0 || serialport.rfind("/tmp/", 0) == 0)) 
    {
        baud = 250000;
    }

    std::shared_ptr<SelectReactor> reactor = std::make_shared<PollReactor>();
    std::shared_ptr<KeyboardReader> keyboard = std::make_shared<KeyboardReader>(reactor,
        serialport, baud, canbus_iface, canbus_nodeid);

    // reactor->run();
}