/*
 * admin/main.c - Client dòng lệnh cho quản trị viên
 *
 * Cách dùng: ./bin/admin <server_ip> <port>
 *
 * Menu:
 *   1. Đăng nhập          -> LOGIN <username> <password>
 *   2. Xem danh sách máy  -> LIST_AGENTS
 *   3. Đăng xuất          -> LOGOUT
 *   0. Thoát              -> QUIT
 */

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocol.h"
#include "stream.h"

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <server_ip> <port>\n", prog);
    fprintf(stderr, "Vi du: %s 127.0.0.1 %d\n", prog, DEFAULT_PORT);
}

/* Kết nối TCP tới ip:port. Trả socket, hoặc -1 nếu lỗi. */
static int connect_server(const char *ip, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "[admin] IP khong hop le: %s\n", ip);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* In prompt rồi đọc 1 dòng từ bàn phím (bỏ '\n' cuối).
 * Trả 0 nếu ok, -1 nếu hết input (Ctrl+D). */
static int read_input(const char *prompt, char *buf, int size)
{
    printf("%s", prompt);
    fflush(stdout);                     /* In prompt ngay (chưa có '\n') */
    if (fgets(buf, size, stdin) == NULL)
        return -1;
    buf[strcspn(buf, "\r\n")] = '\0';   /* Cắt bỏ ký tự xuống dòng */
    return 0;
}

/* Gửi 1 lệnh và chờ 1 dòng phản hồi.
 * Mất kết nối thì in thông báo và thoát chương trình. Trả mã phản hồi. */
static int request(Conn *c, const char *cmd, char *resp, int size)
{
    if (send_line(c->fd, cmd) < 0 || recv_line(c, resp, size) < 0) {
        printf("[admin] mat ket noi toi server, thoat.\n");
        close(c->fd);
        exit(1);
    }
    return atoi(resp);      /* "110 LOGIN_OK admin" -> 110 */
}

/* Chuỗi chỉ gồm ký tự in được, không có dấu cách (để làm 1 tham số) */
static int valid_word(const char *s)
{
    if (s[0] == '\0')
        return 0;
    for (; *s; s++)
        if (*s <= ' ' || *s > '~')
            return 0;
    return 1;
}

/* 1. Đăng nhập */
static void do_login(Conn *c)
{
    char user[MAX_LINE], pass[MAX_LINE];
    if (read_input("Username: ", user, sizeof(user)) < 0 ||
        read_input("Password: ", pass, sizeof(pass)) < 0)
        return;

    if (!valid_word(user) || !valid_word(pass) ||
        strlen(user) >= MAX_USERNAME || strlen(pass) >= MAX_PASSWORD) {
        printf("=> Username/password phai 1-%d ky tu, khong co dau cach.\n", MAX_USERNAME - 1);
        return;
    }

    char cmd[MAX_LINE], resp[MAX_LINE];
    snprintf(cmd, sizeof(cmd), "%s %s %s", CMD_LOGIN, user, pass);
    int code = request(c, cmd, resp, sizeof(resp));

    if (code == RES_LOGIN_OK)
        printf("=> Dang nhap thanh cong.\n");
    else if (code == RES_WRONG_PASS)
        printf("=> Sai username hoac password.\n");
    else if (code == RES_ALREADY_LOGGED_IN)
        printf("=> Ban da dang nhap roi.\n");
    else
        printf("=> Loi: %s\n", resp);
}

/* 2. Xem danh sách máy */
static void do_list(Conn *c)
{
    char resp[MAX_LINE];
    int code = request(c, CMD_LIST_AGENTS, resp, sizeof(resp));

    if (code == RES_NOT_LOGGED_IN) {
        printf("=> Ban can dang nhap truoc.\n");
        return;
    }
    if (code != RES_LIST) {
        printf("=> Loi: %s\n", resp);
        return;
    }

    /* resp = "120 LIST <n> host:status:cpu:ram host:status:cpu:ram ..."
     * strtok cắt chuỗi theo dấu cách, mỗi lần gọi trả 1 từ. */
    strtok(resp, " ");                  /* "120"  */
    strtok(NULL, " ");                  /* "LIST" */
    char *count = strtok(NULL, " ");    /* "<n>"  */
    int n = count ? atoi(count) : 0;

    if (n == 0) {
        printf("=> Chua co may nao ket noi.\n");
        return;
    }

    printf("\n%-4s | %-20s | %-10s | %-5s | %-5s\n", "STT", "Hostname", "Trang thai", "CPU", "RAM");
    printf("-----+----------------------+------------+-------+------\n");

    int shown = 0;
    char *item;
    while ((item = strtok(NULL, " ")) != NULL) {
        /* Tách "host:status:cpu:ram" theo dấu ':' bằng strchr */
        char *host = item;
        char *status = strchr(host, ':');
        char *cpu = status ? strchr(status + 1, ':') : NULL;
        char *ram = cpu ? strchr(cpu + 1, ':') : NULL;
        if (ram == NULL)
            continue;                   /* Mục sai định dạng: bỏ qua */
        *status++ = '\0';               /* Thay ':' bằng '\0' để cắt chuỗi */
        *cpu++ = '\0';
        *ram++ = '\0';

        char cpu_s[8], ram_s[8];
        if (strcmp(cpu, "-") == 0) {
            strcpy(cpu_s, "-");
            strcpy(ram_s, "-");
        } else {
            snprintf(cpu_s, sizeof(cpu_s), "%s%%", cpu);    /* %% in ra dấu % */
            snprintf(ram_s, sizeof(ram_s), "%s%%", ram);
        }

        shown++;
        printf("%-4d | %-20s | %-10s | %-5s | %-5s\n", shown, host, status, cpu_s, ram_s);
    }

    printf("\nTong: %d may", n);
    if (shown < n)      /* Dòng phản hồi dài quá MAX_LINE nên server phải cắt bớt */
        printf(" (chi hien thi duoc %d, danh sach bi cat do qua dai)", shown);
    printf("\n");
}

/* 3. Đăng xuất */
static void do_logout(Conn *c)
{
    char resp[MAX_LINE];
    int code = request(c, CMD_LOGOUT, resp, sizeof(resp));
    if (code == RES_OK)
        printf("=> Da dang xuat.\n");
    else if (code == RES_NOT_LOGGED_IN)
        printf("=> Ban chua dang nhap.\n");
    else
        printf("=> Loi: %s\n", resp);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    char *end;
    long port = strtol(argv[2], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "[admin] port khong hop le: %s\n", argv[2]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);   /* Server tắt đột ngột: send() trả lỗi thay vì chết */

    printf("[admin] connecting to %s:%ld...\n", server_ip, port);
    int fd = connect_server(server_ip, (int)port);
    if (fd < 0)
        return 1;
    printf("[admin] da ket noi.\n");

    Conn conn;
    conn_init(&conn, fd);

    char choice[MAX_LINE];
    for (;;) {
        printf("\n===== QUAN LY PHONG MAY =====\n");
        printf("1. Dang nhap\n");
        printf("2. Xem danh sach may\n");
        printf("3. Dang xuat\n");
        printf("0. Thoat\n");
        if (read_input("Chon: ", choice, sizeof(choice)) < 0)
            break;                      /* Ctrl+D: coi như chọn Thoát */

        if (strcmp(choice, "1") == 0)
            do_login(&conn);
        else if (strcmp(choice, "2") == 0)
            do_list(&conn);
        else if (strcmp(choice, "3") == 0)
            do_logout(&conn);
        else if (strcmp(choice, "0") == 0)
            break;
        else
            printf("=> Lua chon khong hop le.\n");
    }

    /* Thoát: gửi QUIT, chờ "101 BYE" rồi đóng */
    char resp[MAX_LINE];
    request(&conn, CMD_QUIT, resp, sizeof(resp));
    printf("[admin] <- %s\n", resp);
    close(fd);
    return 0;
}
