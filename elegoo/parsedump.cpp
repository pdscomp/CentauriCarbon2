/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 10:16:11
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-28 21:18:27
 * @Description  : Script to parse a serial port data dump
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <memory>
#include <unistd.h>  // 用于 os::read
#include "msgproto.h" // 假设 msgproto.h 是包含 MessageParser 的头文件

std::vector<uint8_t> read_dictionary(const std::string& filename) 
{
    std::ifstream dfile(filename, std::ios::binary);
    if (!dfile) 
    {
        throw std::runtime_error("Could not open dictionary file: " + filename);
    }

    std::vector<uint8_t> dictionary((std::istreambuf_iterator<char>(dfile)), std::istreambuf_iterator<char>());
    return dictionary;
}

int main(int argc, char* argv[]) {
    if (argc < 3) 
    {
        std::cerr << "Usage: " << argv[0] << " <dict_filename> <data_filename>" << std::endl;
        return 1;
    }

    std::string dict_filename = argv[1];
    std::string data_filename = argv[2];

    std::vector<uint8_t> dictionary = read_dictionary(dict_filename);

    std::shared_ptr<MessageParser> mp = std::make_shared<MessageParser>();
    mp->process_identify(dictionary, false); // 传递字典数据

    std::ifstream f(data_filename, std::ios::binary);
    if (!f) 
    {
        std::cerr << "Could not open data file: " << data_filename << std::endl;
        return 1;
    }

    std::vector<uint8_t> data;

    while (true) 
    {
        std::vector<uint8_t> newdata(4096);
        f.read(reinterpret_cast<char*>(newdata.data()), newdata.size());
        std::streamsize bytes_read = f.gcount(); // 获取实际读取的字节数
        
        if (bytes_read <= 0) 
        {
            break;  // 读取完成
        }
        
        data.insert(data.end(), newdata.begin(), newdata.begin() + bytes_read);  // 添加新数据

        while (true) 
        {
            ssize_t length = mp->check_packet(data);  // 检查数据包

            if (length == 0) 
            {
                break;  // 没有完整数据包
            }

            if (length < 0) 
            {
                std::cerr << "Invalid data" << std::endl;
                data = std::vector<uint8_t>(data.end() + length, data.end());
                continue;  // 继续检查
            }

            std::vector<std::string> msgs = mp->dump(std::vector<uint8_t>(data.begin(), data.begin() + length));
            for (size_t i = 1; i < msgs.size(); ++i) 
            {
                std::cout << msgs[i] << std::endl;
            }
            
            data.erase(data.begin(), data.begin() + length); // 更新 data，丢弃已处理的数据
        }
    }

    return 0;
}
