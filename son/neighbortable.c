// 文件名: son/neighbortable.c
//
// 描述: 这个文件实现用于邻居表的API

#include "neighbortable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int nbr_table_size;

// 这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat,
// 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
// 返回创建的邻居表.
nbr_entry_t* nt_create() {
  nbr_table_size = topology_getNbrNum();
  int* nbr_nodes = topology_getNbrArray();
  nbr_entry_t* res = malloc(sizeof(nbr_entry_t) * nbr_table_size);
  for (int i = 0; i < nbr_table_size; ++i) {
    res[i].conn = -1;
    res[i].nodeID = nbr_nodes[i];
    // TODO: fill nodeIP
  }
  return res;
}

// 这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt) {
  for (int i = 0; i < nbr_table_size; ++i) {
    close(nt[i].conn);
  }
  free(nt);
}

// 这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1,
// 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn) {
  for (int i = 0; i < nbr_table_size; ++i)
    if (nt[i].nodeID == nodeID) {
      if (nt[i].conn == -1) {
        nt[i].conn = conn;
        return 1;
      } else
        return -1;
    }
  return -1;
}

nbr_entry_t* get_nt_by_ID(nbr_entry_t* nt, int nodeID) {
  for (int i = 0; i < nbr_table_size; ++i)
    if (nt[i].nodeID == nodeID) return &nt[i];
  return NULL;
}