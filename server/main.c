/*
 * server/main.c - Chương trình server trung tâm
 *
 * Cách dùng: ./bin/server <port>
 *
 * Commit 1: chỉ kiểm tra tham số dòng lệnh, chưa có logic mạng.
 */

#include <stdio.h>
#include <stdlib.h>

/* In hướng dẫn sử dụng ra stderr */
static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <port>\n", prog);
    fprintf(stderr, "Vi du: %s 5500\n", prog);
}

int main(int argc, char *argv[])
{
    /* argc = số tham số, tính cả tên chương trình (argv[0]) */
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Chuyển chuỗi port sang số nguyên và kiểm tra hợp lệ (1..65535) */
    char *end;
    long port = strtol(argv[1], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "[server] port khong hop le: %s\n", argv[1]);
        return 1;
    }

    printf("[server] starting on port %ld...\n", port);

    /* TODO: tạo socket, bind, listen, xử lý nhiều client */
    return 0;
}
