/*
 * server/handler.c - Xử lý lệnh của client (xem docs/protocol.md)
 *
 * Hiện tại chỉ có QUIT. Commit sau thêm HELLO, HEARTBEAT, LOGIN, ...
 * bằng cách thêm nhánh "else if" trong handle_line.
 */

#include <stdio.h>
#include <string.h>

#include "handler.h"
#include "log.h"
#include "protocol.h"

void send_response(Client *cl, int code, const char *name)
{
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%d %s", code, name);
    /* Gửi lỗi (client vừa ngắt) thì bỏ qua: select() sẽ phát hiện ở vòng sau */
    send_line(cl->conn.fd, line);
}

int handle_line(Client *cl, const char *line)
{
    /* Dòng rỗng: bỏ qua, không phản hồi (protocol.md mục 1.1) */
    if (line[0] == '\0')
        return HANDLE_KEEP;

    write_log("%s:%d -> %s", cl->ip, cl->port, line);

    /* Tách tên lệnh: phần trước dấu cách đầu tiên.
     * args trỏ tới phần tham số phía sau (NULL nếu không có). */
    char cmd[MAX_LINE];
    const char *space = strchr(line, ' ');
    const char *args = NULL;
    if (space != NULL) {
        size_t n = (size_t)(space - line);
        memcpy(cmd, line, n);
        cmd[n] = '\0';
        args = space + 1;
    } else {
        strcpy(cmd, line);   /* An toàn: line luôn ngắn hơn MAX_LINE */
    }

    if (strcmp(cmd, CMD_QUIT) == 0) {
        if (args != NULL) {                     /* QUIT không có tham số */
            send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
            return HANDLE_KEEP;
        }
        send_response(cl, RES_BYE, "BYE");
        return HANDLE_CLOSE;
    }

    /* TODO commit sau: HELLO, HEARTBEAT, LOGIN, LOGOUT, LIST_AGENTS */

    send_response(cl, RES_UNKNOWN_CMD, "UNKNOWN_CMD");
    return HANDLE_KEEP;
}
