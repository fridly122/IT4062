/*
 * server/agents.h - Danh sách các máy (agent) server đã biết
 *
 * Tách riêng khỏi Client vì: khi agent mất kết nối, ô Client bị giải phóng,
 * nhưng máy vẫn phải còn trong danh sách với trạng thái offline
 * (docs/protocol.md, lệnh HEARTBEAT và LIST_AGENTS).
 */

#ifndef AGENTS_H
#define AGENTS_H

#include <netinet/in.h>     /* INET_ADDRSTRLEN */
#include <time.h>

#include "protocol.h"

#define MAX_AGENTS 128      /* Số máy tối đa được nhớ (cả online và offline) */

typedef struct {
    int    used;                    /* 1 = ô đang chứa 1 máy */
    char   hostname[MAX_HOSTNAME];
    char   ip[INET_ADDRSTRLEN];
    int    online;                  /* 1 = đang kết nối và gửi heartbeat đều */
    int    has_data;                /* 1 = đã nhận ít nhất 1 heartbeat */
    int    cpu;                     /* % CPU trong heartbeat gần nhất */
    int    ram;                     /* % RAM trong heartbeat gần nhất */
    time_t last_heartbeat;          /* Thời điểm HELLO/HEARTBEAT gần nhất */
} AgentInfo;

/* Tìm máy theo hostname. Trả chỉ số trong bảng, hoặc -1 nếu chưa có. */
int agent_find(const char *hostname);

/* Đăng ký máy (lệnh HELLO): thêm mới hoặc cập nhật máy cũ, đánh dấu online.
 * Gọi khi đã chắc máy này đang KHÔNG online. Trả chỉ số trong bảng. */
int agent_register(const char *hostname, const char *ip);

/* Lấy con trỏ tới máy thứ idx */
AgentInfo *agent_get(int idx);

/* Ghi nhận 1 heartbeat */
void agent_heartbeat(int idx, int cpu, int ram);

/* Chuyển máy sang offline và ghi log lý do */
void agent_set_offline(int idx, const char *reason);

/* Tạo phản hồi LIST_AGENTS vào out (size byte), vd:
 * "120 LIST 2 pc01:online:23:61 pc02:offline:-:-" */
void agent_format_list(char *out, int size);

#endif /* AGENTS_H */
