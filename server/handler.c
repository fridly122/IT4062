/*
 * server/handler.c - Xử lý lệnh của client (xem docs/protocol.md)
 *
 * Thứ tự kiểm tra mỗi lệnh (protocol.md mục 2):
 *   lệnh có tồn tại? (200) -> đúng cú pháp? (201) -> đúng quyền/trạng thái? (3xx) -> thực hiện
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agents.h"
#include "handler.h"
#include "log.h"
#include "protocol.h"

/* TODO commit sau: đọc danh sách tài khoản từ file thay vì viết cứng */
#define ADMIN_USER  "admin"
#define ADMIN_PASS  "123456"

#define MAX_TOKENS  8       /* Số từ tối đa trong 1 lệnh (đủ cho mọi lệnh hiện có) */

void send_response(Client *cl, int code, const char *name)
{
    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%d %s", code, name);
    /* Gửi lỗi (client vừa ngắt) thì bỏ qua: select() sẽ phát hiện ở vòng sau */
    send_line(cl->conn.fd, line);
}

/* ==================== Hàm kiểm tra tham số ==================== */

/* Hostname: 1..63 ký tự, chỉ gồm chữ, số, '-', '_', '.' */
static int valid_hostname(const char *s)
{
    int n = (int)strlen(s);
    if (n < 1 || n >= MAX_HOSTNAME)
        return 0;
    for (int i = 0; i < n; i++) {
        char ch = s[i];
        if (!isalnum((unsigned char)ch) && ch != '-' && ch != '_' && ch != '.')
            return 0;
    }
    return 1;
}

/* IPv4 dạng chấm: inet_pton trả 1 nếu chuyển đổi được */
static int valid_ip(const char *s)
{
    struct in_addr addr;
    return inet_pton(AF_INET, s, &addr) == 1;
}

/* Đọc "<prefix><số 0..100>", vd "cpu=23". Trả 0 nếu ok, -1 nếu sai. */
static int parse_percent(const char *s, const char *prefix, int *value)
{
    size_t plen = strlen(prefix);
    if (strncmp(s, prefix, plen) != 0)      /* Phải bắt đầu bằng prefix */
        return -1;

    const char *num = s + plen;
    if (*num == '\0')
        return -1;
    for (const char *p = num; *p; p++)      /* Chỉ được chứa chữ số */
        if (!isdigit((unsigned char)*p))
            return -1;

    int v = atoi(num);
    if (strlen(num) > 3 || v > 100)
        return -1;
    *value = v;
    return 0;
}

/* Tách line thành các từ, cách nhau bởi ĐÚNG 1 dấu cách.
 * buf: bản sao của line (sẽ bị sửa: dấu cách được thay bằng '\0').
 * Trả số từ; trả -1 nếu có từ rỗng (2 dấu cách liền nhau, cách ở đầu/cuối)
 * hoặc nhiều hơn MAX_TOKENS từ. */
static int split_tokens(char *buf, char *argv[])
{
    int argc = 0;
    char *p = buf;
    for (;;) {
        if (argc == MAX_TOKENS)
            return -1;
        argv[argc++] = p;
        char *space = strchr(p, ' ');
        if (space == NULL)
            break;
        *space = '\0';      /* Cắt chuỗi tại dấu cách */
        p = space + 1;
    }
    for (int i = 0; i < argc; i++)
        if (argv[i][0] == '\0')
            return -1;
    return argc;
}

/* ==================== Xử lý từng lệnh ==================== */

/* HELLO AGENT <hostname> <ip> */
static void cmd_hello(Client *cl, int argc, char *argv[])
{
    if (argc != 4 || strcmp(argv[1], ROLE_AGENT) != 0 ||
        !valid_hostname(argv[2]) || !valid_ip(argv[3])) {
        send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
        return;
    }
    if (cl->type != CLIENT_UNKNOWN) {           /* Đã HELLO hoặc LOGIN rồi */
        send_response(cl, RES_ALREADY_LOGGED_IN, "ALREADY_LOGGED_IN");
        return;
    }

    /* Hostname đang được 1 agent khác (đang online) dùng -> từ chối */
    int idx = agent_find(argv[2]);
    if (idx >= 0 && agent_get(idx)->online) {
        send_response(cl, RES_HOSTNAME_IN_USE, "HOSTNAME_IN_USE");
        return;
    }

    idx = agent_register(argv[2], argv[3]);
    cl->type = CLIENT_AGENT;
    cl->agent_idx = idx;
    snprintf(cl->hostname, sizeof(cl->hostname), "%s", argv[2]);

    write_log("agent %s (%s) online, from %s:%d", argv[2], argv[3], cl->ip, cl->port);
    send_response(cl, RES_OK, "OK");
}

/* HEARTBEAT <hostname> cpu=<n> ram=<n> — thành công thì KHÔNG phản hồi */
static void cmd_heartbeat(Client *cl, int argc, char *argv[])
{
    int cpu, ram;
    if (argc != 4 || parse_percent(argv[2], "cpu=", &cpu) < 0 ||
        parse_percent(argv[3], "ram=", &ram) < 0) {
        send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
        return;
    }
    /* Chỉ nhận từ agent đã HELLO, và hostname phải đúng hostname đã đăng ký */
    if (cl->type != CLIENT_AGENT || strcmp(argv[1], cl->hostname) != 0) {
        send_response(cl, RES_NOT_REGISTERED, "NOT_REGISTERED");
        return;
    }
    agent_heartbeat(cl->agent_idx, cpu, ram);
}

/* LOGIN <username> <password> */
static void cmd_login(Client *cl, int argc, char *argv[])
{
    if (argc != 3 || strlen(argv[1]) >= MAX_USERNAME ||
        strlen(argv[2]) >= MAX_PASSWORD) {
        send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
        return;
    }
    if (cl->type == CLIENT_AGENT || cl->logged_in) {
        send_response(cl, RES_ALREADY_LOGGED_IN, "ALREADY_LOGGED_IN");
        return;
    }
    if (strcmp(argv[1], ADMIN_USER) != 0 || strcmp(argv[2], ADMIN_PASS) != 0) {
        write_log("login failed: user '%s' from %s:%d", argv[1], cl->ip, cl->port);
        send_response(cl, RES_WRONG_PASS, "WRONG_PASS");
        return;
    }

    cl->type = CLIENT_ADMIN;
    cl->logged_in = 1;
    snprintf(cl->username, sizeof(cl->username), "%s", argv[1]);
    write_log("admin '%s' logged in from %s:%d", cl->username, cl->ip, cl->port);

    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%d LOGIN_OK %s", RES_LOGIN_OK, cl->username);
    send_line(cl->conn.fd, line);
}

/* LOGOUT */
static void cmd_logout(Client *cl, int argc)
{
    if (argc != 1) {
        send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
        return;
    }
    if (!cl->logged_in) {
        send_response(cl, RES_NOT_LOGGED_IN, "NOT_LOGGED_IN");
        return;
    }
    cl->logged_in = 0;
    write_log("admin '%s' logged out", cl->username);
    send_response(cl, RES_OK, "OK");
}

/* LIST_AGENTS */
static void cmd_list_agents(Client *cl, int argc)
{
    if (argc != 1) {
        send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
        return;
    }
    if (!cl->logged_in) {
        send_response(cl, RES_NOT_LOGGED_IN, "NOT_LOGGED_IN");
        return;
    }
    char line[MAX_LINE];
    agent_format_list(line, sizeof(line));
    send_line(cl->conn.fd, line);
}

/* ==================== Hàm chính ==================== */

int handle_line(Client *cl, const char *line)
{
    /* Dòng rỗng: bỏ qua, không phản hồi (protocol.md mục 1.1) */
    if (line[0] == '\0')
        return HANDLE_KEEP;

    /* Tách thành các từ: argv[0] là tên lệnh */
    char buf[MAX_LINE];
    char *argv[MAX_TOKENS];
    snprintf(buf, sizeof(buf), "%s", line);
    int argc = split_tokens(buf, argv);

    /* Ghi log lệnh nhận được. Không log HEARTBEAT (5 giây/lần, quá nhiều)
     * và không bao giờ ghi mật khẩu của LOGIN vào log. */
    if (strcmp(argv[0], CMD_LOGIN) == 0)
        write_log("%s:%d -> LOGIN ...", cl->ip, cl->port);
    else if (strcmp(argv[0], CMD_HEARTBEAT) != 0)
        write_log("%s:%d -> %s", cl->ip, cl->port, line);

    /* Kiểm tra lệnh có tồn tại không trước, rồi mới đến cú pháp */
    const char *cmd = argv[0];
    int known = strcmp(cmd, CMD_HELLO) == 0 || strcmp(cmd, CMD_HEARTBEAT) == 0 ||
                strcmp(cmd, CMD_LOGIN) == 0 || strcmp(cmd, CMD_LOGOUT) == 0 ||
                strcmp(cmd, CMD_LIST_AGENTS) == 0 || strcmp(cmd, CMD_QUIT) == 0;
    if (!known) {
        send_response(cl, RES_UNKNOWN_CMD, "UNKNOWN_CMD");
        return HANDLE_KEEP;
    }
    if (argc < 0) {     /* Có từ rỗng hoặc quá nhiều từ */
        send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
        return HANDLE_KEEP;
    }

    if (strcmp(cmd, CMD_HELLO) == 0)
        cmd_hello(cl, argc, argv);
    else if (strcmp(cmd, CMD_HEARTBEAT) == 0)
        cmd_heartbeat(cl, argc, argv);
    else if (strcmp(cmd, CMD_LOGIN) == 0)
        cmd_login(cl, argc, argv);
    else if (strcmp(cmd, CMD_LOGOUT) == 0)
        cmd_logout(cl, argc);
    else if (strcmp(cmd, CMD_LIST_AGENTS) == 0)
        cmd_list_agents(cl, argc);
    else if (strcmp(cmd, CMD_QUIT) == 0) {
        if (argc != 1) {
            send_response(cl, RES_BAD_SYNTAX, "BAD_SYNTAX");
            return HANDLE_KEEP;
        }
        send_response(cl, RES_BYE, "BYE");
        return HANDLE_CLOSE;
    }
    return HANDLE_KEEP;
}
