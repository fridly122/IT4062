/*
 * server/client.h - Thông tin 1 client đang kết nối tới server
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>     /* INET_ADDRSTRLEN */

#include "protocol.h"
#include "stream.h"

#define MAX_CLIENTS 64      /* Số kết nối tối đa server phục vụ cùng lúc */

/* Loại client, xác định bởi lệnh thành công đầu tiên (protocol.md mục 1.4) */
typedef enum {
    CLIENT_UNKNOWN = 0,     /* Mới kết nối, chưa HELLO/LOGIN */
    CLIENT_AGENT,           /* Đã HELLO AGENT thành công */
    CLIENT_ADMIN            /* Đã LOGIN thành công (ít nhất 1 lần) */
} ClientType;

typedef struct {
    Conn conn;                      /* Socket + buffer nhận (common/stream.h) */
    int  active;                    /* 1 = ô đang dùng, 0 = ô trống */
    char ip[INET_ADDRSTRLEN];       /* IP của client, vd "127.0.0.1" */
    int  port;                      /* Port phía client */

    ClientType type;                /* Agent / admin / chưa rõ */

    /* Chỉ dùng khi type == CLIENT_AGENT */
    char hostname[MAX_HOSTNAME];    /* Hostname đã đăng ký bằng HELLO */
    int  agent_idx;                 /* Vị trí của máy trong bảng agents (agents.h),
                                       nơi lưu cpu, ram, last_heartbeat */

    /* Chỉ dùng khi type == CLIENT_ADMIN */
    int  logged_in;                 /* 1 = đang đăng nhập */
    char username[MAX_USERNAME];
} Client;

#endif /* CLIENT_H */
