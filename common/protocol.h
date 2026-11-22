/*
 * common/protocol.h - Hằng số của giao thức ứng dụng
 *
 * Dùng chung cho server, agent, admin.
 * Mô tả chi tiết: docs/protocol.md — hai file PHẢI luôn khớp nhau.
 */

/* Include guard: tránh lỗi định nghĩa lặp khi file bị #include nhiều lần */
#ifndef PROTOCOL_H
#define PROTOCOL_H

/* ==================== Giới hạn và cấu hình ==================== */

#define MAX_LINE            1024    /* Độ dài tối đa 1 dòng, tính cả "\r\n" */
#define MAX_HOSTNAME        64      /* Hostname tối đa 63 ký tự + '\0' */
#define MAX_USERNAME        32      /* Username tối đa 31 ký tự + '\0' */
#define MAX_PASSWORD        32      /* Password tối đa 31 ký tự + '\0' */

#define DEFAULT_PORT        5500    /* Cổng mặc định của server */
#define HEARTBEAT_INTERVAL  5       /* Agent gửi HEARTBEAT mỗi 5 giây */
#define AGENT_TIMEOUT       15      /* Quá 15 giây không có heartbeat -> OFFLINE */

#define LINE_END            "\r\n"  /* Ký tự kết thúc mỗi dòng */

/* ==================== Tên lệnh (client -> server) ==================== */

#define CMD_HELLO           "HELLO"         /* HELLO AGENT <hostname> <ip> */
#define CMD_HEARTBEAT       "HEARTBEAT"     /* HEARTBEAT <hostname> cpu=<n> ram=<n> */
#define CMD_LOGIN           "LOGIN"         /* LOGIN <username> <password> */
#define CMD_LOGOUT          "LOGOUT"        /* LOGOUT */
#define CMD_LIST_AGENTS     "LIST_AGENTS"   /* LIST_AGENTS */
#define CMD_QUIT            "QUIT"          /* QUIT */

#define ROLE_AGENT          "AGENT"         /* Từ thứ hai của lệnh HELLO */

/* Trạng thái máy trong danh sách trả về của LIST_AGENTS */
#define STATUS_ONLINE       "ONLINE"
#define STATUS_OFFLINE      "OFFLINE"

/* ==================== Mã phản hồi (server -> client) ==================== */
/* Dạng phản hồi: "<MÃ_SỐ> <TÊN_MÃ> [dữ liệu]\r\n" */

/* 1xx - Thành công */
#define RES_OK                  100     /* OK */
#define RES_BYE                 101     /* BYE - trả lời QUIT */
#define RES_LOGIN_OK            110     /* LOGIN_OK <username> */
#define RES_LIST                120     /* LIST <n> - theo sau là n dòng "AGENT ..." */

/* 2xx - Lỗi cú pháp / lệnh không tồn tại */
#define RES_UNKNOWN_CMD         200     /* UNKNOWN_CMD */
#define RES_BAD_SYNTAX          201     /* BAD_SYNTAX */
#define RES_LINE_TOO_LONG       202     /* LINE_TOO_LONG */

/* 3xx - Lỗi xác thực / quyền / trạng thái */
#define RES_NOT_LOGGED_IN       300     /* NOT_LOGGED_IN */
#define RES_WRONG_PASS          301     /* WRONG_PASS */
#define RES_ALREADY_LOGGED_IN   302     /* ALREADY_LOGGED_IN */
#define RES_NOT_REGISTERED      303     /* NOT_REGISTERED */

/* 5xx - Lỗi server */
#define RES_SERVER_ERR          500     /* SERVER_ERR */
#define RES_SERVER_FULL         501     /* SERVER_FULL */

#endif /* PROTOCOL_H */
