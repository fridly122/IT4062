/*
 * agent/main.c - Chương trình agent chạy trên từng máy trong phòng lab
 *
 * Cách dùng: ./bin/agent <server_ip> <port>
 *
 * Commit 1: chỉ kiểm tra tham số dòng lệnh, chưa có logic mạng.
 */

#include <stdio.h>
#include <stdlib.h>

/* In hướng dẫn sử dụng ra stderr */
static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <server_ip> <port>\n", prog);
    fprintf(stderr, "Vi du: %s 127.0.0.1 5500\n", prog);
}

int main(int argc, char *argv[])
{
    /* Cần đúng 2 tham số: IP và port (argc = 3 vì có cả argv[0]) */
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];

    /* Chuyển chuỗi port sang số nguyên và kiểm tra hợp lệ (1..65535) */
    char *end;
    long port = strtol(argv[2], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "[agent] port khong hop le: %s\n", argv[2]);
        return 1;
    }

    printf("[agent] connecting to %s:%ld...\n", server_ip, port);

    /* TODO: kết nối tới server, gửi thông tin máy, heartbeat định kỳ */
    return 0;
}
