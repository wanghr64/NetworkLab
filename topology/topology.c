// 文件名: topology/topology.c
//
// 描述: 这个文件实现一些用于解析拓扑文件的辅助函数

#include "topology.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_API 0

static char ifa_name[] = {"enp0s8"};

// 这个函数返回指定主机的节点ID.
// 节点ID是节点IP地址最后8位表示的整数.
// 例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
// 如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) {
  char* num = hostname;
  while (*num != '_') ++num;
  ++num;
  return atoi(num);
}

// 这个函数返回指定的IP地址的节点ID.
// 如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr) {
  uint8_t arr_addr[4];
  memcpy(arr_addr, &addr->s_addr, sizeof(uint32_t));
  return arr_addr[3];
}

#if TEST_API

int topology_getMyNodeID() { return 1; }

#else

// 这个函数返回本机的节点ID
// 如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID() {
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    return -1;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) continue;

    if (ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & 8) &&
        !strcmp(ifa_name, ifa->ifa_name)) {
      int res = topology_getNodeIDfromip(
          &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr);
      freeifaddrs(ifaddr);
      return res;
    }
  }

  freeifaddrs(ifaddr);
  return -1;
}

#endif

// 这个函数解析保存在文件topology.dat中的拓扑信息.
// 返回邻居数.
int topology_getNbrNum() {
  int res = 0;
  FILE* f = fopen("./topology/topology.dat", "r");
  char buf[128];
  while (fgets(buf, sizeof(buf), f) != NULL) {
    int i = 0;
    while (buf[i] && buf[i] != '\n') ++i;
    buf[i] = 0;
    char* parts[3];
    parts[0] = strtok(buf, " ");
    parts[1] = strtok(NULL, " ");
    parts[2] = strtok(NULL, " ");
    if (topology_getNodeIDfromname(parts[0]) == topology_getMyNodeID() ||
        topology_getNodeIDfromname(parts[1]) == topology_getMyNodeID())
      ++res;
  }
  return res;
}

// 这个函数解析保存在文件topology.dat中的拓扑信息.
// 返回重叠网络中的总节点数.
int topology_getNodeNum() {
  char map[256];
  memset(map, 0, 256);
  FILE* f = fopen("./topology/topology.dat", "r");
  char buf[128];
  while (fgets(buf, sizeof(buf), f) != NULL) {
    char* parts[3];
    parts[0] = strtok(buf, " ");
    parts[1] = strtok(NULL, " ");
    parts[2] = strtok(NULL, " ");
    map[topology_getNodeIDfromname(parts[0])] =
        map[topology_getNodeIDfromname(parts[1])] = 1;
  }
  int res = 0;
  for (int i = 0; i < 256; ++i)
    if (map[i]) ++res;
  return res;
}

// 这个函数解析保存在文件topology.dat中的拓扑信息.
// 返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.
int* topology_getNodeArray() {
  char map[256];
  memset(map, 0, 256);
  FILE* f = fopen("./topology/topology.dat", "r");
  char buf[128];
  while (fgets(buf, sizeof(buf), f) != NULL) {
    char* parts[3];
    parts[0] = strtok(buf, " ");
    parts[1] = strtok(NULL, " ");
    parts[2] = strtok(NULL, " ");
    map[topology_getNodeIDfromname(parts[0])] =
        map[topology_getNodeIDfromname(parts[1])] = 1;
  }
  int len = 0;
  for (int i = 0; i < 256; ++i)
    if (map[i]) ++len;

  int* res = malloc(sizeof(int) * len);
  int cur_ptr = 0;
  for (int i = 0; i < 256; ++i)
    if (map[i]) {
      res[cur_ptr] = i;
      ++cur_ptr;
    }
  return res;
}

// 这个函数解析保存在文件topology.dat中的拓扑信息.
// 返回一个动态分配的数组, 它包含所有邻居的节点ID.
int* topology_getNbrArray() {
  int* res = malloc(topology_getNbrNum());
  int cut_ptr = 0;
  FILE* f = fopen("./topology/topology.dat", "r");
  char buf[128];
  while (fgets(buf, sizeof(buf), f) != NULL) {
    char* parts[3];
    parts[0] = strtok(buf, " ");
    parts[1] = strtok(NULL, " ");
    parts[2] = strtok(NULL, " ");
    if (topology_getNodeIDfromname(parts[0]) == topology_getMyNodeID()) {
      res[cut_ptr] = topology_getNodeIDfromname(parts[1]);
      ++cut_ptr;
    } else if (topology_getNodeIDfromname(parts[1]) == topology_getMyNodeID()) {
      res[cut_ptr] = topology_getNodeIDfromname(parts[0]);
      ++cut_ptr;
    }
  }
  return res;
}

// 这个函数解析保存在文件topology.dat中的拓扑信息.
// 返回指定两个节点之间的直接链路代价.
// 如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID) {
  FILE* f = fopen("./topology/topology.dat", "r");
  char buf[128];
  while (fgets(buf, sizeof(buf), f) != NULL) {
    char* parts[3];
    parts[0] = strtok(buf, " ");
    parts[1] = strtok(NULL, " ");
    parts[2] = strtok(NULL, " ");
    if ((topology_getNodeIDfromname(parts[0]) == fromNodeID &&
         topology_getNodeIDfromname(parts[1]) == toNodeID) ||
        (topology_getNodeIDfromname(parts[1]) == fromNodeID &&
         topology_getNodeIDfromname(parts[0]) == toNodeID))
      return atoi(parts[2]);
  }
  return INFINITE_COST;
}
