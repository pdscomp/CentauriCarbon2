/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-10-31 21:52:06
 * @LastEditors  : Jack
 * @LastEditTime : 2025-06-11 18:33:58
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#ifndef _VIDEI_LIST_H_
#define _VIDEI_LIST_H_

extern "C" {
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

#define LINK_LIST_LENGTH (4)
#define SIZE_OF_FRAME (100*1024)

typedef enum {
  FRAME_START = 0,  // 数据帧的开始标志
  FRAME_CENTER,     // 数据帧的中间标志
  FRAME_END,        // 数据帧的结束标志
} FRAME_FLAG_E;

struct NodeContentDef {
  int seqNUm;
  int length;  // 帧长度
  int timeStamp;
  unsigned char *buf;  // 存储图像帧数据
  int appendPtr;       // 偏移量
};

struct Node {
  struct NodeContentDef nodeContent;
  bool frameError;
  struct Node *next;
};

class VideoList {
 public:
  VideoList();
  ~VideoList();

  int Init(void);
  int AppendWriteNode(unsigned char *pBuf, int length, FRAME_FLAG_E FrameFlag);
  int ReadOneFrame(unsigned char *pBuf, int *pLen);
  void SetNodeErrorFrame(void);
  void Reset(void);

  int Append_write_node(char *pBuf, int length, int timeStamp, int start,
                        int end);

 private:
  struct Node *nodeWdPtr;    // 用于写
  struct Node *nodeHDMIPtr;  // 用于读
  pthread_rwlock_t rwlock;
  pthread_rwlockattr_t attr;
  struct Node videoLink[LINK_LIST_LENGTH];
};

#endif
