// 文件名: son/son.c

// 描述: 这个文件实现SON进程
// SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程,
// 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程.
// 然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后,
// SON进程持续接收来自SIP进程的sendpkt_arg_t结构,
// 并将接收到的报文发送到重叠网络中.

#include "son.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "neighbortable.h"

// 你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 10

/**************************************************************/
// 声明全局变量
/**************************************************************/

// 将邻居表声明为一个全局变量
nbr_entry_t* nt;
// 将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn;

/**************************************************************/
// 实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止.
void* waitNbrs(void* arg) {
  int bigger_nbr_num = 0;
  for (int i = 0; i < nbr_table_size; ++i)
    if (nt[i].nodeID > topology_getMyNodeID()) bigger_nbr_num++;

  int listenfd, connfd;
  socklen_t clilen;
  struct sockaddr_in cliaddr, servaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(CONNECTION_PORT);

  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  listen(listenfd, 1024);

  int connected_sum = 0;
  printf("Need to listen to %d nodes.\n", bigger_nbr_num);
  while (connected_sum < bigger_nbr_num) {
    clilen = sizeof(cliaddr);
    connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
    nt_addconn(nt, topology_getNodeIDfromip(&cliaddr.sin_addr), connfd);
    ++connected_sum;
  }

  return NULL;
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
  struct sockaddr_in servaddr;

  for (int i = 0; i < nbr_table_size; ++i) {
    if (nt[i].nodeID < topology_getMyNodeID()) {
      nt[i].conn = socket(AF_INET, SOCK_STREAM, 0);

      bzero(&servaddr, sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      servaddr.sin_port = htons(CONNECTION_PORT);
      servaddr.sin_addr.s_addr = nt[i].nodeIP;
      if (connect(nt[i].conn, (struct sockaddr*)&servaddr, sizeof(servaddr)) <
          0) {
        printf("Failed to connect to %s.\n", inet_ntoa(servaddr.sin_addr));
        return -1;
      }
    }
  }
  return 1;
}

// 每个listen_to_neighbor线程持续接收来自一个邻居的报文.
// 它将接收到的报文转发给SIP进程.
// 所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
void* listen_to_neighbor(void* arg) {
  int idx = *((int*)arg);
  sip_pkt_t pkt;
  memset(&pkt, 0, sizeof(sip_pkt_t));
  while (1) {
    recvpkt(&pkt, nt[idx].conn);
    forwardpktToSIP(&pkt, sip_conn);
  }

  return 0;
}

// 这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接.
// 在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构,
// 并将报文发送到重叠网络中的下一跳. 如果下一跳的节点ID为BROADCAST_NODEID,
// 报文应发送到所有邻居节点.
void waitSIP() {
  int listenfd;
  socklen_t clilen;
  struct sockaddr_in cliaddr, servaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  servaddr.sin_port = htons(SON_PORT);
  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  listen(listenfd, 1024);

  clilen = sizeof(cliaddr);
  sip_conn = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);

  sip_pkt_t pkt;
  memset(&pkt, 0, sizeof(sip_pkt_t));
  int nextNode;
  while (1) {
    // 接收来自SIP进程的sendpkt_arg_t结构
    if (getpktToSend(&pkt, &nextNode, sip_conn) == -1) {
      printf("getpktToSend Error.\n");
      exit(-1);
    }
    // 如果下一跳的节点ID为BROADCAST_NODEID，报文应发送到所有邻居节点.
    if (nextNode == BROADCAST_NODEID) {
    } else {
      nbr_entry_t* next_nt = get_nt_by_ID(nt, nextNode);
      sendpkt(&pkt, next_nt->conn);
    }
  }
}

// 这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
// 它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
  close(sip_conn);
  nt_destroy(nt);
}

int main() {
  // 启动重叠网络初始化工作
  printf("Overlay network: Node %d initializing...\n", topology_getMyNodeID());

  // 创建一个邻居表
  nt = nt_create();
  // 将sip_conn初始化为-1, 即还未与SIP进程连接
  sip_conn = -1;

  // 注册一个信号句柄, 用于终止进程
  signal(SIGINT, son_stop);

  // 打印所有邻居
  int nbrNum = topology_getNbrNum();
  int i;
  for (i = 0; i < nbrNum; i++) {
    struct in_addr addr;
    addr.s_addr = nt[i].nodeIP;
    printf("Overlay network: neighbor %d:%d, IP: %s\n", i + 1, nt[i].nodeID,
           inet_ntoa(addr));
  }

  // 启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
  pthread_t waitNbrs_thread;
  pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void*)0);

  // 等待其他节点启动
  sleep(SON_START_DELAY);

  // 连接到节点ID比自己小的所有邻居
  connectNbrs();

  // 等待waitNbrs线程返回
  pthread_join(waitNbrs_thread, NULL);

  // 此时, 所有与邻居之间的连接都建立好了

  // 创建线程监听所有邻居
  for (i = 0; i < nbrNum; i++) {
    int* idx = (int*)malloc(sizeof(int));
    *idx = i;
    pthread_t nbr_listen_thread;
    pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void*)idx);
  }
  printf("Overlay network: node initialized...\n");
  printf("Overlay network: waiting for connection from SIP process...\n");

  // 等待来自SIP进程的连接
  waitSIP();
}
