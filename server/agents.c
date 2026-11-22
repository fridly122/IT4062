/*
 * server/agents.c - Cài đặt danh sách máy (xem agents.h)
 */

#include <stdio.h>
#include <string.h>

#include "agents.h"
#include "log.h"

static AgentInfo agents[MAX_AGENTS];

int agent_find(const char *hostname)
{
    for (int i = 0; i < MAX_AGENTS; i++) {
        if (agents[i].used && strcmp(agents[i].hostname, hostname) == 0)
            return i;
    }
    return -1;
}

int agent_register(const char *hostname, const char *ip)
{
    int idx = agent_find(hostname);     /* Máy cũ kết nối lại? */

    if (idx < 0) {
        /* Máy mới: tìm ô trống */
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (!agents[i].used) {
                idx = i;
                break;
            }
        }
    }
    if (idx < 0) {
        /* Bảng đầy: thay máy offline lâu nhất. Luôn tìm được vì số máy
         * online <= MAX_CLIENTS (64) < MAX_AGENTS (128). */
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (!agents[i].online &&
                (idx < 0 || agents[i].last_heartbeat < agents[idx].last_heartbeat))
                idx = i;
        }
    }

    AgentInfo *a = &agents[idx];
    memset(a, 0, sizeof(*a));
    a->used = 1;
    snprintf(a->hostname, sizeof(a->hostname), "%s", hostname);
    snprintf(a->ip, sizeof(a->ip), "%s", ip);
    a->online = 1;
    a->last_heartbeat = time(NULL);     /* Tính thời gian timeout từ lúc HELLO */
    return idx;
}

AgentInfo *agent_get(int idx)
{
    return &agents[idx];
}

void agent_heartbeat(int idx, int cpu, int ram)
{
    AgentInfo *a = &agents[idx];
    a->cpu = cpu;
    a->ram = ram;
    a->has_data = 1;
    a->last_heartbeat = time(NULL);
}

void agent_set_offline(int idx, const char *reason)
{
    AgentInfo *a = &agents[idx];
    if (!a->online)
        return;
    a->online = 0;
    write_log("agent %s (%s) offline: %s", a->hostname, a->ip, reason);
}

void agent_format_list(char *out, int size)
{
    /* Đếm số máy */
    int n = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (agents[i].used)
            n++;

    /* snprintf trả số ký tự đã ghi; len là vị trí ghi tiếp theo */
    int len = snprintf(out, size, "%d LIST %d", RES_LIST, n);

    for (int i = 0; i < MAX_AGENTS; i++) {
        AgentInfo *a = &agents[i];
        if (!a->used)
            continue;

        /* Tạo 1 mục "host:status:cpu:ram"; chưa có số liệu thì dùng "-" */
        char item[MAX_HOSTNAME + 32];
        if (a->online && a->has_data)
            snprintf(item, sizeof(item), " %s:%s:%d:%d",
                     a->hostname, STATUS_ONLINE, a->cpu, a->ram);
        else
            snprintf(item, sizeof(item), " %s:%s:-:-",
                     a->hostname, a->online ? STATUS_ONLINE : STATUS_OFFLINE);

        /* Cả dòng (tính "\r\n") không được vượt MAX_LINE.
         * Không đủ chỗ thì dừng: client thấy số mục < n là biết bị cắt. */
        int item_len = (int)strlen(item);
        if (len + item_len + 2 > MAX_LINE || len + item_len >= size)
            break;
        memcpy(out + len, item, item_len + 1);   /* +1: copy cả '\0' */
        len += item_len;
    }
}
