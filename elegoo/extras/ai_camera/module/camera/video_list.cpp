/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-05 14:06:54
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-05 14:21:12
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "video_list.h"

VideoList::VideoList() {
  nodeWdPtr = NULL;    // 用于写
  nodeHDMIPtr = NULL;  // 用于读
  memset(videoLink, 0, sizeof(videoLink));
}

VideoList::~VideoList() {
  for (int i = 0; i < LINK_LIST_LENGTH; i++) {
    if (videoLink[i].nodeContent.buf) {
      free(videoLink[i].nodeContent.buf);
      videoLink[i].nodeContent.buf = NULL;
    }
  }
}

/*
 * 功  能： 初始化 VideoList
 * 参  数： 无
 * 返回值：
 *      成功      ---》0
 *      失败      ---》-1
 */
int VideoList::Init(void) {
  int i;
  struct Node *head, *next;

  head = &videoLink[0];
  head->next = head;

  // 申请堆内存
  for (i = 0; i < LINK_LIST_LENGTH; i++) {
    videoLink[i].nodeContent.buf = (unsigned char *)malloc(SIZE_OF_FRAME);
    if (NULL == videoLink[i].nodeContent.buf) {
      printf("%s malloc failed!\n", __FUNCTION__);
      return -1;
    }
  }

  // 形成 单向循环链
  for (i = 1; i < LINK_LIST_LENGTH; i++) {
    next = &videoLink[i];
    next->next = head->next;
    head->next = next;
  }

  nodeWdPtr = head;
  nodeHDMIPtr = head;

  if (pthread_rwlockattr_init(&attr) != 0) return -1;

  if (pthread_rwlockattr_setkind_np(
          &attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) != 0)
    return -1;

  if (pthread_rwlock_init(&rwlock, &attr) != 0) return -1;

  printf("%s successful\n", __FUNCTION__);

  return 0;
}

/*
 * 功  能： 读取一帧数据
 * 参  数：
 *      pBuf    ---》保存读取一帧数据的缓冲区首地址
 *      pLen    ---》保存读取一帧数据的长度的地址
 * 返回值：
 *      成功      ---》0
 *      失败      ---》-1
 */
int VideoList::ReadOneFrame(unsigned char *pBuf, int *pLen) {
  // Param Check
  if ((!pBuf) || (!pLen)) {
    printf("Input Param Error!\n");
    return -1;
  }

  int err = 0;

  if (pthread_rwlock_tryrdlock(&rwlock) != 0) {
    return -1;
  }

  if (nodeHDMIPtr != nodeWdPtr) {
    memcpy(pBuf, nodeHDMIPtr->nodeContent.buf, nodeHDMIPtr->nodeContent.length);
    memcpy(pLen, &nodeHDMIPtr->nodeContent.length, sizeof(int));

    nodeHDMIPtr = nodeHDMIPtr->next;
  } else {
    err = -1;
  }

  pthread_rwlock_unlock(&rwlock);

  return err;
}

/*
 * 功  能： 追加写入（一帧数据有可能是分开多次传输，那么需要组帧）
 * 参  数：
 *      pBuf        ---》写入数据缓冲区首地址
 *      length      ---》写入数据的长度
 *      FrameFlag   ---》数据帧标志
 *                      FRAME_START         写入一帧数据的开始部分
 *                      FRAME_CENTER        写入一帧数据的中间部分
 *                      FRAME_END           写入一帧数据的结束部分
 * 返回值：
 *      成功          ---》0
 *      失败          ---》-1
 */
int VideoList::AppendWriteNode(unsigned char *pBuf, int length,
                               FRAME_FLAG_E FrameFlag) {
  // Param Check
  if ((!pBuf) || (length <= 0) || (FrameFlag < 0)) {
    printf("Input Param Error!\n");
    return -1;
  }

  if ((nodeWdPtr->nodeContent.length + length) > SIZE_OF_FRAME) {
    printf("Current One Frame Video Data[%d]:Out Of Rang[%d]!!!\n",
           nodeWdPtr->nodeContent.length + length, SIZE_OF_FRAME);
    return -1;
  }

  pthread_rwlock_wrlock(&rwlock);

  if ((FrameFlag == FRAME_START) || (FrameFlag == FRAME_CENTER)) {
    // 写入数据
    nodeWdPtr->nodeContent.length += length;
    memcpy(nodeWdPtr->nodeContent.buf + nodeWdPtr->nodeContent.appendPtr, pBuf,
           length);
    nodeWdPtr->nodeContent.appendPtr += length;

  } else if (FrameFlag == FRAME_END) {
    // 写入最后一点数据
    nodeWdPtr->nodeContent.length += length;
    memcpy(nodeWdPtr->nodeContent.buf + nodeWdPtr->nodeContent.appendPtr, pBuf,
           length);
    nodeWdPtr->nodeContent.appendPtr += length;

    // 指向下一个节点，并且将其清零
    nodeWdPtr = nodeWdPtr->next;
    memset(nodeWdPtr->nodeContent.buf, 0, nodeWdPtr->nodeContent.length);
    nodeWdPtr->nodeContent.length = 0;
    nodeWdPtr->nodeContent.appendPtr = 0;
    nodeWdPtr->frameError = false;
  }

  pthread_rwlock_unlock(&rwlock);

  return 0;
}

int VideoList::Append_write_node(char *pBuf, int length, int timeStamp,
                                 int start, int end) {
  pthread_rwlock_wrlock(&rwlock);

  if (start == 1) {
    nodeWdPtr->nodeContent.timeStamp = timeStamp;
  }

  if ((nodeWdPtr->nodeContent.length + length) < SIZE_OF_FRAME) {
    nodeWdPtr->nodeContent.length += length;
    memcpy(nodeWdPtr->nodeContent.buf + nodeWdPtr->nodeContent.appendPtr, pBuf,
           length);
    nodeWdPtr->nodeContent.appendPtr += length;

    if (end == 1) {
      nodeWdPtr->nodeContent.length -= 10;

      nodeWdPtr = nodeWdPtr->next;
      memset(nodeWdPtr->nodeContent.buf, 0, nodeWdPtr->nodeContent.length);
      nodeWdPtr->nodeContent.length = 0;
      nodeWdPtr->nodeContent.appendPtr = 0;
      nodeWdPtr->frameError = false;
    }
  } else {
    printf("Current One Frame Video Data[%d]:Out Of Rang[%d]!!!\n",
           nodeWdPtr->nodeContent.length + length, SIZE_OF_FRAME);
  }

  pthread_rwlock_unlock(&rwlock);

  return 0;
}

void VideoList::SetNodeErrorFrame(void) {
  pthread_rwlock_wrlock(&rwlock);

  nodeWdPtr->frameError = true;
  memset(nodeWdPtr->nodeContent.buf, 0, nodeWdPtr->nodeContent.length);
  nodeWdPtr = nodeWdPtr->next;
  nodeWdPtr->nodeContent.length = 0;
  nodeWdPtr->nodeContent.appendPtr = 0;
  nodeWdPtr->frameError = false;

  pthread_rwlock_unlock(&rwlock);
}

void VideoList::Reset(void) {
  pthread_rwlock_wrlock(&rwlock);

  nodeWdPtr = nodeHDMIPtr = &videoLink[0];
  memset(nodeWdPtr->nodeContent.buf, 0, nodeWdPtr->nodeContent.length);
  nodeWdPtr->nodeContent.length = 0;
  nodeWdPtr->nodeContent.appendPtr = 0;
  nodeWdPtr->frameError = false;

  pthread_rwlock_unlock(&rwlock);
}
