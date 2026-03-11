/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-04-19 16:50:11
 * @LastEditors  : Ben
 * @LastEditTime : 2025-06-27 14:38:19
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "utilities.h"
#include "json.h"
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <pty.h>
#include <algorithm>
#include <sstream>
#include <regex>
#include <cmath>

namespace elegoo
{
namespace common
{

std::string strip(const std::string& input,
    const std::string& chars) {
    size_t start = input.find_first_not_of(chars);
    if (start == std::string::npos) return "";
    size_t end = input.find_last_not_of(chars);
    return input.substr(start, end - start + 1);
}

std::string to_upper(const std::string &str)
{
    std::string upper_str = str;
    for (auto &c : upper_str)
    {
        c = std::toupper(c);
    }
    return upper_str;
}

std::vector<std::string> split(
    const std::string& input, const std::string& delimiter,
    size_t max_splits) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end;
    size_t splits = 0;

    while ((end = input.find(delimiter, start)) != std::string::npos) {
        if (splits >= max_splits) break;
        tokens.push_back(input.substr(start, end - start));
        start = end + delimiter.length();
        ++splits;
    }
    tokens.push_back(input.substr(start));
    return tokens;
}

std::vector<std::string> shlex_split(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_quote = false;
    char quote_char = '\0';
    bool escape_next = false;

    for (char c : input) {
        if (escape_next) {
            current_token += c;
            escape_next = false;
            continue;
        }

        if (c == '\\') {
            escape_next = true;
            continue;
        }

        if (c == '"' || c == '\'') {
            if (in_quote) {
                if (c == quote_char) {
                    in_quote = false;
                    quote_char = '\0';
                } else {
                    current_token += c;
                }
            } else {
                in_quote = true;
                quote_char = c;
            }
            continue;
        }

        if (isspace(c) && !in_quote) {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += c;
        }
    }

    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }

    return tokens;
}

std::vector<std::string> regex_split(const std::string& input) {
    std::vector<std::string> parts;
    if (input.empty()) {
        parts.emplace_back("");
        return parts;
    }
    
    parts.reserve(input.size() / 3); 
    
    const char* data = input.data();
    const size_t length = input.size();
    size_t start = 0;
    auto is_special_char = [](char c) {
        return (c >= 'A' && c <= 'Z') || c == '_' || c == '*' || c == '/';
    };

    if (is_special_char(data[0])) {
        parts.emplace_back("");
    }

    while (start < length) {
        const bool currentSpecial = is_special_char(data[start]);
        size_t end = start + 1;
    
        while (end < length && is_special_char(data[end]) == currentSpecial) {
            ++end;
        }

        parts.emplace_back(data + start, end - start);
        start = end;
    }

    if (is_special_char(data[length - 1])) {
        parts.emplace_back("");
    }
    
    return parts;
}

std::string join(const std::vector<std::string>& elements,
    const std::string& delimiter) {
    std::ostringstream result;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i != 0) result << delimiter;
        result << elements[i];
    }
    return result.str();
}

void fix_sigint() {
    signal(SIGINT, SIG_DFL);
}

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void clear_hupcl(int fd) {
    struct termios attrs;
    if (tcgetattr(fd, &attrs) == 0) {
        attrs.c_cflag &= ~HUPCL;
        tcsetattr(fd, TCSADRAIN, &attrs);
    }
}

int create_pty(const std::string& ptyname) {
    int mfd, sfd;
    if (openpty(&mfd, &sfd, nullptr, nullptr, nullptr) < 0) {
        throw std::runtime_error("Failed to open pty");
    }

    unlink(ptyname.c_str());
    std::string filename = ttyname(sfd);
    chmod(filename.c_str(), 0660);
    symlink(filename.c_str(), ptyname.c_str());
    set_nonblock(mfd);

    struct termios old;
    tcgetattr(mfd, &old);
    old.c_lflag &= ~ECHO;
    tcsetattr(mfd, TCSADRAIN, &old);

    return mfd;
}

void dump_file_stats(const std::string& build_dir, const std::string& filename) {
    std::string fname = build_dir + "/" + filename;
    struct stat statbuf;
    if (stat(fname.c_str(), &statbuf) == 0) {
        std::time_t mtime = statbuf.st_mtime;
        std::cout << "Build file " << fname << " (" << statbuf.st_size << "): " 
                    << std::ctime(&mtime);
    } else {
        std::cout << "No build file " << fname << std::endl;
    }
}

std::string getParentPath(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        return "";
    }
    return path.substr(0, pos);
}

void dump_mcu_build() {
    std::string src_dir = __FILE__;
    std::string build_dir = getParentPath(src_dir);
    dump_file_stats(build_dir, ".config");

    std::ifstream config_file(build_dir + "/.config");
    if (config_file) {
        std::string data((std::istreambuf_iterator<char>(config_file)),
                            std::istreambuf_iterator<char>());
        std::cout << "========= Last MCU build config =========\n" << data
                    << "=======================" << std::endl;
    }

    dump_file_stats(build_dir, "out/elegoo.dict");
    std::ifstream elegoo_file(build_dir + "/out/elegoo.dict");
    if (elegoo_file) {
        json data;
        elegoo_file >> data;

        std::cout << "Last MCU build version: " << data.value("version", "?") << std::endl;
        std::cout << "Last MCU build tools: " << data.value("build_versions", "?") << std::endl;

        std::cout << "Last MCU build config: ";
        for (json::iterator it = data.begin(); it != data.end(); ++it) {
            std::cout << "Key: " << it.key() << ", Value: " << it.value() << std::endl;
        }
        std::cout << std::endl;
    }
    dump_file_stats(build_dir, "out/elegoo.elf");
}

std::string get_cpu_info() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo) {
        return "?";
    }

    std::string line;
    int core_count = 0;
    std::string model_name = "?";
    while (std::getline(cpuinfo, line)) {
        if (line.find("processor") == 0) {
            core_count++;
        }
        if (line.find("model name") == 0) {
            model_name = line.substr(line.find(":") + 2);
        }
    }
    return std::to_string(core_count) + " core " + model_name;
}

std::string get_version_from_file(const std::string& elegoo_src) {
    std::ifstream version_file(elegoo_src + "/.version");
    if (version_file) {
        std::string version((std::istreambuf_iterator<char>(version_file)),
                                std::istreambuf_iterator<char>());
        return version;
    }
    return "?";
    }

    std::string get_git_version(bool from_file) {
    std::string version = "?";
    std::string elegoo_src = __FILE__;
    std::string gitdir = getParentPath(elegoo_src);
    std::string cmd = "git -C " + gitdir + " describe --always --tags --long --dirty";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return version;

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof buffer, pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    if (!result.empty()) {
        version = result.substr(0, result.find_last_not_of(" \n") + 1);
    }

    if (from_file) {
        version = get_version_from_file(gitdir);
    }

    return version;
}

void get_firmware_info(std::string path, std::unordered_map<std::string, std::string>& map)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "无法打开 version.json 文件。" << std::endl;
        return;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        std::cerr << "解析 JSON 出错: " << e.what() << std::endl;
        return ;
    }

    // 输出 machine.model
    if (j.contains("machine") && j["machine"].contains("model")) {
        map["machine_model"] = j["machine"]["model"];
    }

    // 输出所有版本信息
    if (j.contains("versions") && j["versions"].is_array()) {
        for (const auto& item : j["versions"]) {
            for (auto it = item.begin(); it != item.end(); ++it) {
                map[it.key()] = it.value();
            }
        }
    }
}

bool are_equal(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

int ffs(uint32_t mask) {
    if (mask == 0) {
        return -1;  // No bits set
    }
    int position = 0;
    while ((mask & 1) == 0) {
        mask >>= 1;
        position++;
    }
    return position;
}

int bit_length(int number) {
    if (number == 0) {
        return 0;
    }
    return static_cast<int>(std::ceil(std::log2(number + 1)));
}


} // namespace common
} // namespace elegoo