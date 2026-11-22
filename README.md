# Hệ thống giám sát phòng máy

Bài tập lớn môn **Lập trình mạng (IT4062)** — lớp Sáng T3, Đại học Bách khoa Hà Nội.

## Thành viên

| Họ và tên          | MSSV     |
|--------------------|----------|
| Nguyễn Anh Nguyên  | 20235796 |
| Trần Văn Tùng      | 20235868 |

## Mô tả

Hệ thống gồm 3 chương trình viết bằng C, giao tiếp với nhau qua TCP:

- **agent**: chạy trên từng máy trong phòng lab, thu thập và gửi thông tin máy
  (tên máy, trạng thái, ...) định kỳ về server.
- **server**: chương trình trung tâm, nhận kết nối TCP đồng thời từ nhiều agent
  và admin, lưu trạng thái các máy và chuyển tiếp lệnh điều khiển.
- **admin**: client dòng lệnh cho quản trị viên, dùng để xem danh sách / trạng thái
  các máy và gửi lệnh điều khiển tới máy.

## Kiến trúc

```
  +---------+
  | agent 1 |----+
  +---------+    |
                 |   TCP    +----------+   TCP    +---------+
  +---------+    +--------->|          |<---------|  admin  |
  | agent 2 |-------------->|  server  |          +---------+
  +---------+    +--------->|          |
                 |          +----------+
  +---------+    |
  | agent N |----+
  +---------+
```

## Cấu trúc thư mục

```
.
├── common/     # Code dùng chung: giao thức, truyền dòng, log
├── server/     # Mã nguồn server
│   └── main.c
├── agent/      # Mã nguồn agent
│   └── main.c
├── admin/      # Mã nguồn admin
│   └── main.c
├── tests/      # Chương trình kiểm thử (make test)
├── docs/       # Tài liệu (thiết kế giao thức, báo cáo)
├── Makefile
├── README.md
└── .gitignore
```

## Build

Yêu cầu: `gcc` (hoặc clang trên macOS) và `make`. Chạy được trên macOS và Linux.

```bash
make            # build cả 3 chương trình vào thư mục bin/
make server     # chỉ build server (tương tự: make agent, make admin)
make test       # build và chạy test (tests/)
make clean      # xoá thư mục bin/
```

## Chạy

```bash
./bin/server <port>                  # vd: ./bin/server 5500
./bin/agent  <server_ip> <port>      # vd: ./bin/agent 127.0.0.1 5500
./bin/admin  <server_ip> <port>      # vd: ./bin/admin 127.0.0.1 5500
```

Server ghi log ra màn hình và vào file `server.log` (trong thư mục đang chạy lệnh).
Nhấn `Ctrl+C` để tắt server.

## Kiểm thử thủ công

Dùng `nc` (netcat) đóng vai client. **Chú ý:** giao thức yêu cầu mỗi dòng kết thúc bằng `\r\n`,
còn `nc` mặc định chỉ gửi `\n`, nên phải thêm cờ để `nc` gửi CRLF:

- macOS: `nc -c localhost 5500`
- Linux (netcat-openbsd): `nc -C localhost 5500`

Thiếu cờ này thì server không thấy dòng hoàn chỉnh và sẽ không phản hồi.

**Các bước:**

1. Terminal 1 — chạy server:
   ```bash
   make
   ./bin/server 5500
   ```
2. Terminal 2, 3, 4 — mỗi terminal mở 1 client:
   ```bash
   nc -c localhost 5500
   ```
   Server in `connect 127.0.0.1:<port> (slot N)` cho mỗi client.
3. Ở các client, gõ lệnh linh tinh, vd `abc`, `hello world`.
   Mỗi lệnh nhận `200 UNKNOWN_CMD`; server log `127.0.0.1:<port> -> abc`.
4. Gõ `QUIT now` → `201 BAD_SYNTAX` (QUIT không có tham số), kết nối vẫn giữ.
5. Gõ `QUIT` → nhận `101 BYE`, server đóng kết nối, log `disconnect ... (quit)`.
6. Ở 1 client khác, nhấn `Ctrl+C` để tắt `nc` đột ngột.
   Server log `disconnect ... (closed by peer)` và **không crash**.
7. Ở client còn lại, gõ tiếp vài lệnh → vẫn nhận phản hồi bình thường.
8. Ở terminal server, nhấn `Ctrl+C` → server log `shutting down` và thoát.
9. Xem lại log: `cat server.log`.

**Kết quả mong đợi:** server phục vụ đồng thời nhiều client, client này thoát (dù đúng cách
hay đột ngột) không ảnh hưởng client khác. Khi đã có đủ `MAX_CLIENTS` = 64 kết nối,
client tiếp theo nhận `501 SERVER_FULL` và bị đóng ngay.

## Tiến độ

- [x] Khởi tạo project
- [x] Thiết kế giao thức (xem [docs/protocol.md](docs/protocol.md))
- [x] Xử lý truyền dòng (`common/stream.c`, kiểm tra bằng `make test`)
- [x] Server đa client (`select()`, log vào `server.log`)
- [ ] Agent heartbeat + Admin xem danh sách
