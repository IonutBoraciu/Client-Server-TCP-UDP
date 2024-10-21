#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1024

struct chat_packet {
  uint16_t len;
  char message[MSG_MAXSIZE + 1];
};
struct udpMessage {
  char topic[50];
  unsigned int type;
  char message[1500];
};
struct completeMessage {
  char topic[50];
  char mesaj[1500];
  int val;
  float val_short_poz;
  float big_float;
  unsigned int type;
  char ip[50];
  int port;
};
#endif
