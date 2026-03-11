#pragma once

#include <cstring>
#include <thread>

namespace Pbus {

typedef struct {
  char *buffer;
  int in;    // 输入指针
  int out;   // 输出指针
  int size;  // 缓冲区大小，必须为2的次幂
} pKfifo;

class Kfifo {
 public:
  explicit Kfifo(int size = 1024);
  ~Kfifo();

 private:
  int Init(int size);

 public:
  int PutData(const char *data, int len);
  int GetData(char *data, int len);
  void Free();
  void Empty();

 private:
  pKfifo *fifo_;
  pthread_rwlock_t rw_lock_;
};

}  // namespace Pbus
