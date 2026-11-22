/*
 * server/main.c - Server trung tâm, phục vụ nhiều client cùng lúc bằng select()
 *
 * Cách dùng: ./bin/server <port>
 *
 * Ý tưởng: chỉ có 1 tiến trình, 1 luồng. select() chờ đến khi có ít nhất
 * 1 socket "sẵn sàng" (có kết nối mới hoặc có dữ liệu), rồi server xử lý
 * lần lượt các socket đó, xong quay lại chờ tiếp.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "client.h"
#include "handler.h"
#include "log.h"
#include "protocol.h"
#include "stream.h"

#define BACKLOG   16            /* Số kết nối chờ accept tối đa trong hàng đợi */
#define LOG_FILE  "server.log"

static Client clients[MAX_CLIENTS];     /* Ô active = 0 là ô trống */

/* Đổi thành 0 khi nhấn Ctrl+C để thoát vòng lặp chính gọn gàng.
 * volatile sig_atomic_t: kiểu an toàn để sửa trong signal handler. */
static volatile sig_atomic_t running = 1;

static void on_sigint(int sig)
{
    (void)sig;      /* Không dùng tham số: tránh warning */
    running = 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <port>\n", prog);
    fprintf(stderr, "Vi du: %s %d\n", prog, DEFAULT_PORT);
}

/* Tạo socket lắng nghe: socket -> setsockopt -> bind -> listen.
 * Trả fd, hoặc -1 nếu lỗi. */
static int create_listen_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    /* SO_REUSEADDR: cho phép chạy lại server ngay trên cùng port,
     * không phải đợi port được giải phóng (trạng thái TIME_WAIT). */
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   /* Nhận kết nối trên mọi card mạng */
    addr.sin_port = htons((unsigned short)port);  /* htons: đổi sang thứ tự byte mạng */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

/* Đóng kết nối và trả ô về trạng thái trống */
static void close_client(Client *cl, const char *reason)
{
    write_log("disconnect %s:%d (%s)", cl->ip, cl->port, reason);
    close(cl->conn.fd);
    cl->active = 0;
}

/* Có kết nối mới trên socket lắng nghe: accept và lưu vào ô trống */
static void accept_client(int listen_fd)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(listen_fd, (struct sockaddr *)&addr, &addrlen);
    if (fd < 0) {
        perror("accept");
        return;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));  /* IP dạng số -> chuỗi */
    int port = ntohs(addr.sin_port);

    /* Tìm ô trống */
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        /* Hết chỗ: báo cho client rồi đóng ngay */
        char line[MAX_LINE];
        snprintf(line, sizeof(line), "%d SERVER_FULL", RES_SERVER_FULL);
        send_line(fd, line);
        close(fd);
        write_log("reject %s:%d (server full)", ip, port);
        return;
    }

    Client *cl = &clients[slot];
    memset(cl, 0, sizeof(*cl));
    conn_init(&cl->conn, fd);
    cl->active = 1;
    strcpy(cl->ip, ip);
    cl->port = port;
    write_log("connect %s:%d (slot %d)", ip, port, slot);
}

/* Client có dữ liệu: đọc 1 lần, rồi xử lý hết các dòng hoàn chỉnh trong buffer */
static void handle_client_data(Client *cl)
{
    int n = conn_read(&cl->conn);
    if (n == 0) {
        close_client(cl, "closed by peer");
        return;
    }
    if (n < 0) {
        close_client(cl, "read error or line too long");
        return;
    }

    /* 1 lần recv có thể chứa 0, 1 hoặc nhiều dòng -> lặp đến khi hết dòng */
    char line[MAX_LINE];
    int len;
    while ((len = conn_get_line(&cl->conn, line, sizeof(line))) != STREAM_NO_LINE) {
        if (len == STREAM_TOO_LONG) {
            send_response(cl, RES_LINE_TOO_LONG, "LINE_TOO_LONG");
            continue;
        }
        if (handle_line(cl, line) == HANDLE_CLOSE) {
            close_client(cl, "quit");
            return;     /* Bỏ phần dữ liệu còn lại của client này */
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    char *end;
    long port = strtol(argv[1], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "[server] port khong hop le: %s\n", argv[1]);
        return 1;
    }

    /* Client ngắt đột ngột rồi server send() vào socket đó sẽ sinh SIGPIPE,
     * mặc định làm chết chương trình. Bỏ qua để send() chỉ trả lỗi. */
    signal(SIGPIPE, SIG_IGN);

    /* Ctrl+C: dùng sigaction không có SA_RESTART để select() bị ngắt
     * (trả -1, errno = EINTR) và vòng lặp kiểm tra lại biến running. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int listen_fd = create_listen_socket((int)port);
    if (listen_fd < 0)
        return 1;

    log_open(LOG_FILE);
    write_log("[server] starting on port %ld...", port);

    while (running) {
        /*
         * fd_set: một "tập hợp" các socket (file descriptor) cần theo dõi.
         *   FD_ZERO(&set)     : làm rỗng tập
         *   FD_SET(fd, &set)  : thêm fd vào tập
         *   FD_ISSET(fd, &set): sau select(), hỏi fd có sẵn sàng không
         *
         * select() SỬA TRỰC TIẾP tập truyền vào (chỉ giữ lại các fd sẵn sàng),
         * nên mỗi vòng lặp phải tạo lại tập từ đầu.
         */
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(listen_fd, &readfds);    /* Theo dõi kết nối mới */
        int max_fd = listen_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                int fd = clients[i].conn.fd;
                FD_SET(fd, &readfds);   /* Theo dõi dữ liệu từ client */
                if (fd > max_fd)
                    max_fd = fd;        /* Ghi nhớ fd lớn nhất */
            }
        }

        /*
         * Tham số đầu của select() là (max_fd + 1): select chỉ kiểm tra các fd
         * từ 0 đến max_fd, nên phải biết fd lớn nhất đang theo dõi.
         * 3 tập: đọc / ghi / lỗi. Ở đây chỉ cần tập đọc.
         * Timeout NULL: chờ mãi đến khi có fd sẵn sàng.
         */
        int ready = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR)     /* Bị tín hiệu ngắt (vd Ctrl+C) */
                continue;           /* Quay lại kiểm tra running */
            perror("select");
            break;
        }

        /* Socket lắng nghe sẵn sàng = có client mới đang chờ accept */
        if (FD_ISSET(listen_fd, &readfds))
            accept_client(listen_fd);

        /* Kiểm tra từng client xem có dữ liệu không */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && FD_ISSET(clients[i].conn.fd, &readfds))
                handle_client_data(&clients[i]);
        }
    }

    /* Dọn dẹp khi thoát */
    write_log("[server] shutting down");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active)
            close_client(&clients[i], "server shutdown");
    }
    close(listen_fd);
    log_close();
    return 0;
}
