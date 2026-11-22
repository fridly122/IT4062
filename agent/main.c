/*
 * agent/main.c - Chương trình agent chạy trên từng máy trong phòng lab
 *
 * Cách dùng: ./bin/agent <server_ip> <port> [hostname]
 *   hostname: tuỳ chọn, mặc định lấy bằng gethostname().
 *             Dùng khi chạy nhiều agent trên cùng 1 máy để demo.
 *
 * Hoạt động:
 *   1. Kết nối server, gửi "HELLO AGENT <hostname> <ip>", chờ "100 OK".
 *   2. Cứ HEARTBEAT_INTERVAL giây gửi "HEARTBEAT <hostname> cpu=<n> ram=<n>".
 *   3. Server đóng kết nối -> in thông báo và thoát. Ctrl+C -> gửi QUIT rồi thoát.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "protocol.h"
#include "stream.h"

/* Đổi thành 0 khi nhấn Ctrl+C */
static volatile sig_atomic_t running = 1;

static void on_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <server_ip> <port> [hostname]\n", prog);
    fprintf(stderr, "Vi du: %s 127.0.0.1 %d\n", prog, DEFAULT_PORT);
}

/* Kết nối TCP tới ip:port. Trả socket, hoặc -1 nếu lỗi. */
static int connect_server(const char *ip, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {   /* Chuỗi IP -> dạng số */
        fprintf(stderr, "[agent] IP khong hop le: %s\n", ip);
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

/* Lấy hostname của máy, sửa cho hợp lệ theo giao thức:
 * tối đa 63 ký tự, ký tự lạ (ngoài chữ, số, - _ .) đổi thành '-'. */
static void get_hostname(char *out, int size)
{
    if (gethostname(out, size) != 0 || out[0] == '\0')
        snprintf(out, size, "unknown");
    out[size - 1] = '\0';   /* gethostname có thể không thêm '\0' nếu tên quá dài */

    for (char *p = out; *p; p++) {
        char ch = *p;
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                 (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.';
        if (!ok)
            *p = '-';
    }
}

/* Lấy IP của chính máy này trên kết nối tới server.
 * getsockname() cho biết địa chỉ phía "mình" của socket đã kết nối. */
static void get_local_ip(int fd, char *out, int size)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) < 0 ||
        inet_ntop(AF_INET, &addr.sin_addr, out, size) == NULL)
        snprintf(out, size, "0.0.0.0");
}

/* Gửi 1 heartbeat. Trả 0 nếu ok, -1 nếu gửi lỗi. */
static int send_heartbeat(int fd, const char *hostname)
{
    /* TODO commit sau: lấy % CPU và % RAM thật của máy thay vì số ngẫu nhiên */
    int cpu = rand() % 101;     /* 0..100 */
    int ram = rand() % 101;

    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%s %s cpu=%d ram=%d", CMD_HEARTBEAT, hostname, cpu, ram);
    if (send_line(fd, line) < 0)
        return -1;
    printf("[agent] -> %s\n", line);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    char *end;
    long port = strtol(argv[2], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "[agent] port khong hop le: %s\n", argv[2]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);   /* Server tắt đột ngột: send() trả lỗi thay vì chết */

    /* In từng dòng ngay lập tức, kể cả khi chuyển output vào file (./bin/agent ... > a.log) */
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Ctrl+C ngắt được select() (không có SA_RESTART) để thoát gọn */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    srand((unsigned)(time(NULL) ^ getpid()));   /* Mỗi agent 1 dãy số ngẫu nhiên khác */

    char hostname[MAX_HOSTNAME];
    if (argc == 4)
        snprintf(hostname, sizeof(hostname), "%s", argv[3]);
    else
        get_hostname(hostname, sizeof(hostname));

    printf("[agent] connecting to %s:%ld...\n", server_ip, port);
    int fd = connect_server(server_ip, (int)port);
    if (fd < 0)
        return 1;

    Conn conn;
    conn_init(&conn, fd);

    /* ---- Bước 1: đăng ký với server ---- */
    char ip[INET_ADDRSTRLEN];
    get_local_ip(fd, ip, sizeof(ip));

    char line[MAX_LINE];
    snprintf(line, sizeof(line), "%s %s %s %s", CMD_HELLO, ROLE_AGENT, hostname, ip);
    if (send_line(fd, line) < 0) {
        fprintf(stderr, "[agent] gui HELLO that bai\n");
        close(fd);
        return 1;
    }
    printf("[agent] -> %s\n", line);

    int n = recv_line(&conn, line, sizeof(line));
    if (n < 0) {
        fprintf(stderr, "[agent] mat ket noi khi cho phan hoi HELLO\n");
        close(fd);
        return 1;
    }
    printf("[agent] <- %s\n", line);
    if (atoi(line) != RES_OK) {     /* atoi đọc số ở đầu chuỗi, vd "304 ..." -> 304 */
        fprintf(stderr, "[agent] server tu choi dang ky: %s\n", line);
        close(fd);
        return 1;
    }
    printf("[agent] dang ky thanh cong: %s (%s)\n", hostname, ip);

    /* ---- Bước 2: gửi heartbeat định kỳ ---- */
    if (send_heartbeat(fd, hostname) < 0) {
        fprintf(stderr, "[agent] mat ket noi toi server\n");
        close(fd);
        return 1;
    }
    time_t next_hb = time(NULL) + HEARTBEAT_INTERVAL;   /* Lúc gửi heartbeat tiếp */

    while (running) {
        /* Vừa chờ đến lúc gửi heartbeat, vừa theo dõi socket: nếu server
         * đóng kết nối, select() báo "đọc được" và recv() trả 0. */
        time_t now = time(NULL);
        struct timeval tv;
        tv.tv_sec = next_hb > now ? next_hb - now : 0;
        tv.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        int ready = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR)     /* Ctrl+C: vòng lặp kiểm tra lại running */
                continue;
            perror("select");
            break;
        }

        if (ready > 0) {
            int r = conn_read(&conn);
            if (r <= 0) {
                printf("[agent] mat ket noi toi server, thoat.\n");
                close(fd);
                return 1;
            }
            /* Thành công thì HEARTBEAT không có phản hồi; có dòng gửi về là lỗi */
            while ((n = conn_get_line(&conn, line, sizeof(line))) != STREAM_NO_LINE) {
                if (n >= 0)
                    printf("[agent] <- %s\n", line);
            }
        }

        if (time(NULL) >= next_hb) {
            if (send_heartbeat(fd, hostname) < 0) {
                printf("[agent] mat ket noi toi server, thoat.\n");
                close(fd);
                return 1;
            }
            next_hb += HEARTBEAT_INTERVAL;
        }
    }

    /* ---- Ctrl+C: chào server rồi thoát ---- */
    printf("\n[agent] dang thoat...\n");
    if (send_line(fd, CMD_QUIT) == 0 && recv_line(&conn, line, sizeof(line)) >= 0)
        printf("[agent] <- %s\n", line);
    close(fd);
    return 0;
}
