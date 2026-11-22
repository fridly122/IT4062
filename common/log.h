/*
 * common/log.h - Ghi log kèm thời gian
 *
 * Mỗi dòng log có dạng: [2026-11-22 20:00:00] nội dung
 * và được ghi đồng thời vào file log và stdout.
 */

#ifndef LOG_H
#define LOG_H

/* Mở file log (ghi nối tiếp vào cuối). Nếu mở lỗi thì chỉ in ra stdout. */
void log_open(const char *filename);

/* Đóng file log. */
void log_close(void);

/* Ghi 1 dòng log, cách dùng giống printf (tự thêm thời gian và '\n').
 * __attribute__((format...)): nhờ gcc/clang kiểm tra fmt giống printf,
 * vd viết %d mà truyền chuỗi sẽ có warning. */
void write_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* LOG_H */
