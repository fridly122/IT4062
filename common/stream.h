/*
 * common/stream.h - Xử lý truyền dòng trên TCP
 *
 * TCP là luồng byte: 1 lần recv() có thể nhận nửa dòng, hoặc nhiều dòng dính nhau.
 * Vì vậy mỗi kết nối có 1 buffer riêng để tích luỹ dữ liệu, và chỉ tách ra
 * thành dòng khi gặp "\r\n" (xem docs/protocol.md, mục 1.1).
 *
 * Cách dùng với select() (server):
 *     conn_read(&c);                       // khi select báo fd có dữ liệu
 *     while ((n = conn_get_line(&c, line, MAX_LINE)) != STREAM_NO_LINE) {
 *         ... xử lý từng dòng ...
 *     }
 *
 * Cách dùng blocking (agent/admin):
 *     n = recv_line(&c, line, MAX_LINE);
 */

#ifndef STREAM_H
#define STREAM_H

#define CONN_BUF_SIZE   4096    /* Kích thước buffer nhận của mỗi kết nối */

/* Giá trị trả về đặc biệt (đều là số âm, vì số >= 0 là độ dài dòng) */
#define STREAM_ERR      -1  /* Lỗi (recv/send lỗi, buffer đầy mà chưa có "\r\n") */
#define STREAM_NO_LINE  -1  /* conn_get_line: buffer chưa có dòng hoàn chỉnh */
#define STREAM_TOO_LONG -2  /* Dòng dài quá max, đã bị bỏ khỏi buffer */
#define STREAM_CLOSED   -3  /* recv_line: bên kia đã đóng kết nối */

/* Một kết nối: socket + buffer chứa dữ liệu đã nhận nhưng chưa xử lý */
typedef struct {
    int  fd;                    /* Socket của kết nối */
    char buf[CONN_BUF_SIZE];    /* Dữ liệu đã nhận, chưa tách thành dòng */
    int  len;                   /* Số byte đang có trong buf */
} Conn;

/* Gắn socket fd vào Conn, buffer rỗng. */
void conn_init(Conn *c, int fd);

/* Gửi đủ len byte (gọi send() lặp lại). Trả 0 nếu ok, -1 nếu lỗi. */
int send_all(int fd, const char *buf, int len);

/* Gửi line và tự thêm "\r\n". line không được chứa "\r\n".
 * Trả 0 nếu ok, -1 nếu lỗi hoặc dòng dài quá MAX_LINE (tính cả "\r\n"). */
int send_line(int fd, const char *line);

/* Gọi recv() đúng 1 lần, nối dữ liệu vào cuối c->buf.
 * Trả số byte đọc được; 0 nếu bên kia đóng kết nối;
 * -1 nếu lỗi hoặc buffer đã đầy mà vẫn chưa có "\r\n".
 * Lưu ý: phải lấy hết các dòng bằng conn_get_line trước khi gọi lại hàm này. */
int conn_read(Conn *c);

/* Lấy 1 dòng hoàn chỉnh từ buffer (KHÔNG gọi recv).
 * max là kích thước mảng out; dòng tính cả "\r\n" dài hơn max bị coi là quá dài
 * (gọi với max = MAX_LINE thì đúng giới hạn của giao thức).
 * Trả về:
 *   >= 0             độ dài dòng, đã copy vào out (không có "\r\n", có '\0' cuối)
 *   STREAM_NO_LINE   chưa có dòng hoàn chỉnh
 *   STREAM_TOO_LONG  dòng quá dài: đã bỏ dòng đó khỏi buffer, out không đổi */
int conn_get_line(Conn *c, char *out, int max);

/* Bản blocking cho agent/admin: chờ đến khi có 1 dòng.
 * Trả về độ dài dòng (>= 0), STREAM_ERR, STREAM_TOO_LONG hoặc STREAM_CLOSED. */
int recv_line(Conn *c, char *out, int max);

#endif /* STREAM_H */
