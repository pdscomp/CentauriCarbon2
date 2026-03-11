/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-05 12:28:58
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-05 14:56:18
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "kfifo.h"

namespace Pbus {
// #define min(a ,b)		((a)<=(b)?(a):(b))

// 内核中的写法
#define min(x, y)       \
  ({                    \
    typeof(x) _x = (x); \
    typeof(y) _y = (y); \
    (&_x == &_y);       \
    _x < _y ? _x : _y;  \
  })

/*
    in - out为缓冲区中的数据长度
    size - in + out 为缓冲区中空闲空间
    in == out时缓冲区为空
    size == (in - out)时缓冲区满了
 */

/*判断n是否为2的幂*/
static bool is_power_of_2(int n) { return (n != 0 && ((n & (n - 1)) == 0)); }

/*将数字a向上取整为2的次幂*/
static int roundup_power_of_2(int a) {
  if (a == 0) {
    return 0;
  }

  int i;
  unsigned int position = 0;
  for (i = a; i != 0; i >>= 1) position++;

  return (int)(1 << position);
}

Kfifo::Kfifo(int size) { Init(size); }

Kfifo::~Kfifo() { Free(); }

int Kfifo::Init(int size) {
  if (size <= 0) {
    return -1;
  }

  fifo_ = (pKfifo *)malloc(sizeof(pKfifo));
  if (nullptr == fifo_) {
    return -1;
  }

  if (!is_power_of_2(size)) size = roundup_power_of_2(size);

  fifo_->buffer = (char *)(malloc(size * sizeof(char)));
  if (nullptr != fifo_->buffer) {
    fifo_->in = 0;
    fifo_->out = 0;
    fifo_->size = size;
    Empty();

    // 初始化一个读写锁
    if (0 != pthread_rwlock_init(&rw_lock_, nullptr)) {
      if (nullptr != fifo_->buffer) {
        free(fifo_->buffer);
        fifo_->buffer = nullptr;
      }
      if (nullptr != fifo_) {
        free(fifo_);
        fifo_ = nullptr;
      }
      return -1;
    }
  } else {
    if (nullptr != fifo_) {
      free(fifo_);
      fifo_ = nullptr;
    }
    return -1;
  }

  return 0;
}

int Kfifo::PutData(const char *data, int len) {
  if ((nullptr == fifo_) || (nullptr == data) || (len <= 0)) {
    return -1;
  }

  pthread_rwlock_wrlock(&rw_lock_);  // 请求写锁

  /*当前缓冲区空闲空间*/
  len = min(len, fifo_->size - fifo_->in + fifo_->out);

  /*当前in位置到buffer末尾的长度*/
  int l = min(len, fifo_->size - (fifo_->in & (fifo_->size - 1)));

  /*首先复制数据到[in，buffer的末尾]*/
  memcpy(fifo_->buffer + (fifo_->in & (fifo_->size - 1)), data, l);

  /*复制剩余的数据（如果有）到[buffer的起始位置,...]*/
  memcpy(fifo_->buffer, data + l, len - l);

  fifo_->in += len;  // 直接加，不作模运算。当溢出时，从buffer的开始位置重新开始

  pthread_rwlock_unlock(&rw_lock_);  // 释放写锁

  return len;
}

int Kfifo::GetData(char *data, int len) {
  if ((nullptr == fifo_) || (nullptr == data) || (len <= 0)) {
    return -1;
  }

  pthread_rwlock_rdlock(&rw_lock_);  // 请求读锁

  /*缓冲区中的数据长度: 注意都是无符号数*/
  len = min(len, fifo_->in - fifo_->out);

  // 首先从[out,buffer end]读取数据
  int l = min(len, fifo_->size - (fifo_->out & (fifo_->size - 1)));

  memcpy(data, fifo_->buffer + (fifo_->out & (fifo_->size - 1)), l);

  // 从[buffer start,...]读取数据
  memcpy(data + l, fifo_->buffer, len - l);

  fifo_->out += len;  // 直接加，不错模运算。溢出后，从buffer的起始位置重新开始

  pthread_rwlock_unlock(&rw_lock_);  // 释放读锁

  return len;
}

void Kfifo::Free() {
  if (nullptr != fifo_) {
    // 释放申请的队列空间
    if (nullptr != fifo_->buffer) {
      free(fifo_->buffer);
      fifo_->buffer = nullptr;
    }

    if (nullptr != fifo_) {
      free(fifo_);
      fifo_ = nullptr;
    }

    // 销毁读写锁
    pthread_rwlock_destroy(&rw_lock_);
  }
}

void Kfifo::Empty() {
  if (nullptr != fifo_) {
    memset(fifo_->buffer, 0, fifo_->size);
    fifo_->in = 0;
    fifo_->out = 0;
  }
}

}  // namespace Pbus
