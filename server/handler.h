/*
 * server/handler.h - Xử lý từng dòng lệnh client gửi lên
 */

#ifndef HANDLER_H
#define HANDLER_H

#include "client.h"

/* Giá trị trả về của handle_line */
#define HANDLE_KEEP   0     /* Giữ kết nối */
#define HANDLE_CLOSE  1     /* Server cần đóng kết nối này (vd sau QUIT) */

/* Xử lý 1 dòng lệnh (đã bỏ "\r\n") của client cl và gửi phản hồi.
 * Trả HANDLE_KEEP hoặc HANDLE_CLOSE. Việc đóng socket do main.c làm. */
int handle_line(Client *cl, const char *line);

/* Gửi phản hồi dạng "<code> <name>" (vd "200 UNKNOWN_CMD"). */
void send_response(Client *cl, int code, const char *name);

#endif /* HANDLER_H */
