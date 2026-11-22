/*
 * common/log.c - Cài đặt ghi log (xem log.h)
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "log.h"

static FILE *log_file = NULL;   /* File log đang mở (NULL = chưa mở) */

void log_open(const char *filename)
{
    log_file = fopen(filename, "a");    /* "a": ghi nối vào cuối file */
    if (log_file == NULL)
        perror("log_open");
}

void log_close(void)
{
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

/* Ghi 1 dòng ra stream f: "[thời gian] nội dung\n" */
static void write_to(FILE *f, const char *timestr, const char *fmt, va_list ap)
{
    fprintf(f, "[%s] ", timestr);
    vfprintf(f, fmt, ap);   /* vfprintf: giống fprintf nhưng nhận va_list */
    fputc('\n', f);
    fflush(f);              /* Ghi ngay, không đợi buffer đầy */
}

void write_log(const char *fmt, ...)
{
    /* Lấy thời gian hiện tại, định dạng "2026-11-22 20:00:00" */
    char timestr[32];
    time_t now = time(NULL);
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));

    /* va_list chứa các tham số "..." của hàm.
     * Mỗi va_list chỉ dùng được 1 lần, nên cần va_copy để ghi 2 nơi. */
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    write_to(stdout, timestr, fmt, ap);
    if (log_file != NULL)
        write_to(log_file, timestr, fmt, ap2);

    va_end(ap2);
    va_end(ap);
}
