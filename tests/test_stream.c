/*
 * tests/test_stream.c - Kiểm tra module truyền dòng (common/stream.c)
 *
 * Không cần server thật: socketpair() tạo sẵn 2 socket nối với nhau,
 * ghi vào sv[0] thì đọc được ở sv[1].
 *
 * Chạy: make test
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocol.h"
#include "stream.h"

static int failed = 0;  /* Số test bị FAIL */

/* In kết quả 1 test */
static void report(const char *name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        failed++;
}

/* Tạo cặp socket: sv[0] để gửi, sv[1] để nhận (gắn vào Conn) */
static int make_pair(int sv[2], Conn *c)
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return -1;
    }
    conn_init(c, sv[1]);
    return 0;
}

static void close_pair(int sv[2])
{
    close(sv[0]);
    close(sv[1]);
}

/* 1. Gửi 1 dòng trọn vẹn bằng send_line (tự thêm "\r\n") */
static void test_full_line(void)
{
    int sv[2];
    Conn c;
    char line[MAX_LINE];

    if (make_pair(sv, &c) < 0) { report("1 dong tron ven", 0); return; }

    int ok = send_line(sv[0], "HELLO AGENT pc01 10.0.0.1") == 0;
    int n = recv_line(&c, line, sizeof(line));
    ok = ok && n == 25 && strcmp(line, "HELLO AGENT pc01 10.0.0.1") == 0
            && c.len == 0;  /* Buffer phải rỗng sau khi lấy dòng */

    report("1 dong tron ven", ok);
    close_pair(sv);
}

/* 2. Một dòng bị cắt làm 2 lần send */
static void test_split_line(void)
{
    int sv[2];
    Conn c;
    char line[MAX_LINE];

    if (make_pair(sv, &c) < 0) { report("1 dong bi cat lam 2 lan send", 0); return; }

    /* Lần 1: mới có nửa đầu -> chưa được có dòng */
    send_all(sv[0], "HEART", 5);
    int ok = conn_read(&c) == 5
          && conn_get_line(&c, line, sizeof(line)) == STREAM_NO_LINE;

    /* Lần 2: phần còn lại -> ghép thành 1 dòng hoàn chỉnh */
    send_all(sv[0], "BEAT pc01 cpu=10 ram=20\r\n", 25);
    int n = recv_line(&c, line, sizeof(line));
    ok = ok && n >= 0 && strcmp(line, "HEARTBEAT pc01 cpu=10 ram=20") == 0;

    report("1 dong bi cat lam 2 lan send", ok);
    close_pair(sv);
}

/* 3. Hai dòng dính nhau trong 1 lần send */
static void test_two_lines(void)
{
    int sv[2];
    Conn c;
    char line1[MAX_LINE], line2[MAX_LINE];

    if (make_pair(sv, &c) < 0) { report("2 dong dinh nhau trong 1 lan send", 0); return; }

    const char *data = "LOGIN admin 123\r\nLIST_AGENTS\r\n";
    send_all(sv[0], data, (int)strlen(data));

    /* 1 lần recv nhận cả 2 dòng; sau đó tách ra mà không cần recv thêm */
    int ok = conn_read(&c) == (int)strlen(data);
    ok = ok && conn_get_line(&c, line1, sizeof(line1)) >= 0
            && conn_get_line(&c, line2, sizeof(line2)) >= 0
            && conn_get_line(&c, line2, sizeof(line2)) == STREAM_NO_LINE
            && strcmp(line1, "LOGIN admin 123") == 0
            && strcmp(line2, "LIST_AGENTS") == 0;

    report("2 dong dinh nhau trong 1 lan send", ok);
    close_pair(sv);
}

/* 4. Bị cắt đúng giữa '\r' và '\n' */
static void test_split_crlf(void)
{
    int sv[2];
    Conn c;
    char line[MAX_LINE];

    if (make_pair(sv, &c) < 0) { report("cat giua \\r va \\n", 0); return; }

    send_all(sv[0], "QUIT\r", 5);
    int ok = conn_read(&c) == 5
          && conn_get_line(&c, line, sizeof(line)) == STREAM_NO_LINE;

    send_all(sv[0], "\n", 1);
    ok = ok && recv_line(&c, line, sizeof(line)) == 4 && strcmp(line, "QUIT") == 0;

    report("cat giua \\r va \\n", ok);
    close_pair(sv);
}

/* 5. Dòng quá MAX_LINE bị bỏ, dòng tiếp theo vẫn đọc bình thường */
static void test_too_long(void)
{
    int sv[2];
    Conn c;
    char line[MAX_LINE];
    char big[MAX_LINE + 10];

    if (make_pair(sv, &c) < 0) { report("dong qua dai bi bo", 0); return; }

    /* MAX_LINE - 1 ký tự 'A' + "\r\n" = MAX_LINE + 1 byte: vượt giới hạn 1 byte */
    memset(big, 'A', MAX_LINE - 1);
    send_all(sv[0], big, MAX_LINE - 1);
    send_all(sv[0], "\r\nQUIT\r\n", 8);

    int ok = recv_line(&c, line, sizeof(line)) == STREAM_TOO_LONG
          && recv_line(&c, line, sizeof(line)) == 4
          && strcmp(line, "QUIT") == 0;

    /* send_line cũng phải từ chối gửi dòng quá dài */
    big[MAX_LINE - 1] = '\0';
    ok = ok && send_line(sv[0], big) == -1;

    report("dong qua dai bi bo", ok);
    close_pair(sv);
}

/* 6. Bên kia đóng kết nối */
static void test_closed(void)
{
    int sv[2];
    Conn c;
    char line[MAX_LINE];

    if (make_pair(sv, &c) < 0) { report("ben kia dong ket noi", 0); return; }

    close(sv[0]);   /* Đóng đầu gửi */
    int ok = recv_line(&c, line, sizeof(line)) == STREAM_CLOSED;

    report("ben kia dong ket noi", ok);
    close(sv[1]);
}

int main(void)
{
    /* Gửi vào socket đã đóng không được làm chết chương trình (xem stream.c) */
    signal(SIGPIPE, SIG_IGN);

    test_full_line();
    test_split_line();
    test_two_lines();
    test_split_crlf();
    test_too_long();
    test_closed();

    if (failed == 0)
        printf("Tat ca test PASS\n");
    else
        printf("%d test FAIL\n", failed);

    /* Trả mã khác 0 khi có lỗi để "make test" báo thất bại */
    return failed == 0 ? 0 : 1;
}
