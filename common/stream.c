/*
 * common/stream.c - Cài đặt xử lý truyền dòng (xem stream.h)
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "protocol.h"
#include "stream.h"

/* Linux có cờ MSG_NOSIGNAL: send() vào socket đã đóng sẽ trả lỗi thay vì
 * gửi tín hiệu SIGPIPE làm chết chương trình. macOS không có cờ này,
 * nên ở main() của từng chương trình nên gọi signal(SIGPIPE, SIG_IGN). */
#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

void conn_init(Conn *c, int fd)
{
    c->fd = fd;
    c->len = 0;
}

int send_all(int fd, const char *buf, int len)
{
    int sent = 0;   /* Số byte đã gửi được */

    /* send() có thể chỉ gửi được một phần, nên phải lặp đến khi hết */
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, SEND_FLAGS);
        if (n < 0) {
            if (errno == EINTR)     /* Bị tín hiệu ngắt giữa chừng: thử lại */
                continue;
            return -1;
        }
        sent += (int)n;
    }
    return 0;
}

int send_line(int fd, const char *line)
{
    char tmp[MAX_LINE];
    int n = (int)strlen(line);

    /* Dòng + "\r\n" phải vừa giới hạn MAX_LINE của giao thức */
    if (n + 2 > MAX_LINE)
        return -1;

    /* Ghép line và "\r\n" vào 1 mảng để gửi trong 1 lần */
    memcpy(tmp, line, n);
    tmp[n] = '\r';
    tmp[n + 1] = '\n';
    return send_all(fd, tmp, n + 2);
}

/* Tìm "\r\n" trong c->buf. Trả vị trí của '\r', hoặc -1 nếu không có.
 * (Tự viết vòng lặp vì memmem không có trên mọi hệ thống.) */
static int find_crlf(const Conn *c)
{
    for (int i = 0; i + 1 < c->len; i++) {
        if (c->buf[i] == '\r' && c->buf[i + 1] == '\n')
            return i;
    }
    return -1;
}

int conn_read(Conn *c)
{
    int space = CONN_BUF_SIZE - c->len;     /* Chỗ trống còn lại trong buffer */

    /* Buffer đầy mà không có "\r\n": bên kia gửi dòng quá dài / sai giao thức.
     * Không thể recv thêm (recv với độ dài 0 sẽ trả 0, dễ nhầm là đóng kết nối). */
    if (space <= 0)
        return -1;

    ssize_t n;
    do {
        /* Ghi tiếp vào sau phần dữ liệu đang có: c->buf + c->len */
        n = recv(c->fd, c->buf + c->len, space, 0);
    } while (n < 0 && errno == EINTR);      /* Bị ngắt bởi tín hiệu: gọi lại */

    if (n < 0)
        return -1;      /* Lỗi */
    if (n == 0)
        return 0;       /* Bên kia đóng kết nối */

    c->len += (int)n;

    /* Vừa đọc làm đầy buffer mà vẫn chưa có dòng hoàn chỉnh -> lỗi */
    if (c->len == CONN_BUF_SIZE && find_crlf(c) < 0)
        return -1;

    return (int)n;
}

int conn_get_line(Conn *c, char *out, int max)
{
    int pos = find_crlf(c);         /* Vị trí '\r' của dòng đầu tiên */
    if (pos < 0)
        return STREAM_NO_LINE;

    int consumed = pos + 2;         /* Số byte của dòng, tính cả "\r\n" */
    int result;

    if (consumed > max) {
        result = STREAM_TOO_LONG;   /* Quá dài: không copy, chỉ bỏ đi */
    } else {
        memcpy(out, c->buf, pos);   /* Copy nội dung dòng (không có "\r\n") */
        out[pos] = '\0';            /* Kết thúc chuỗi C */
        result = pos;
    }

    /*
     * Dồn phần dữ liệu còn lại lên đầu buffer.
     *
     * Ví dụ buffer có 2 dòng dính nhau (len = 10, dòng đầu "HI1" -> consumed = 5):
     *
     *   trước:  [ H I 1 \r \n | B Y E \r \n ]
     *             0       4     5       9
     *                           ^ c->buf + consumed
     *
     *   sau:    [ B Y E \r \n ]   len = 10 - 5 = 5
     *
     * Phải dùng memmove chứ không dùng memcpy, vì vùng nguồn và vùng đích
     * có thể chồng lên nhau (memcpy khi chồng nhau cho kết quả không xác định).
     */
    c->len -= consumed;
    memmove(c->buf, c->buf + consumed, c->len);

    return result;
}

int recv_line(Conn *c, char *out, int max)
{
    for (;;) {
        /* Buffer đã có sẵn dòng (hoặc dòng quá dài) thì trả về ngay */
        int n = conn_get_line(c, out, max);
        if (n != STREAM_NO_LINE)
            return n;

        /* Chưa có dòng hoàn chỉnh: chờ nhận thêm dữ liệu */
        int r = conn_read(c);
        if (r == 0)
            return STREAM_CLOSED;
        if (r < 0)
            return STREAM_ERR;
    }
}
