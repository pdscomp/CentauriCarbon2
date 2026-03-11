/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-10-25 17:52:23
 * @LastEditors  : Jack
 * @LastEditTime : 2025-04-02 15:13:02
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include "control.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include "logs.h"

znp::Control* g_control = nullptr;

void cleanup() {
    if (g_control) {
        delete g_control;
        g_control = nullptr;
        std::cout << "Resources released." << std::endl;
    }
}

void signal_handler(int signum) {
    std::cout << "Received signal: " << signum << ". Initiating cleanup..." << std::endl;
    cleanup();
    std::exit(EXIT_FAILURE);
}


int main(int argc, char const *argv[]) {
  std::atexit(cleanup);

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  init_logs();
  g_control  = new znp::Control(SOCKET_FILE);
  if (!g_control) {
    printf("Control Init failed!\n");
    return -1;
  }
  if (CommonTools::isFileExists(CAMERA_JPEG_DEVICE)) {
    g_control->Start();
  }
  else {
    printf("No Camera!\n");
  }

  g_control->Loop();


  return 0;
}
