/*
 * server/client.h - Thông tin 1 client đang kết nối tới server
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>     /* INET_ADDRSTRLEN */

#include "stream.h"

#define MAX_CLIENTS 64      /* Số kết nối tối đa server phục vụ cùng lúc */

typedef struct {
    Conn conn;                      /* Socket + buffer nhận (common/stream.h) */
    int  active;                    /* 1 = ô đang dùng, 0 = ô trống */
    char ip[INET_ADDRSTRLEN];       /* IP của client, vd "127.0.0.1" */
    int  port;                      /* Port phía client */
    /* TODO commit sau: loại client (agent/admin), hostname, trạng thái đăng nhập... */
} Client;

#endif /* CLIENT_H */
