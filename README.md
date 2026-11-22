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
./bin/agent  <server_ip> <port> [hostname]  # vd: ./bin/agent 127.0.0.1 5500
./bin/admin  <server_ip> <port>      # vd: ./bin/admin 127.0.0.1 5500
```

- `agent`: `hostname` tuỳ chọn, mặc định lấy tên máy (`gethostname`). Chỉ cần truyền khi chạy
  nhiều agent trên cùng 1 máy, vì server không cho 2 agent online trùng hostname.
- `admin`: tài khoản tạm thời là `admin` / `123456`.

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

## Demo

Kịch bản demo luồng end-to-end: 1 server, 2 agent, 1 admin, mỗi chương trình 1 terminal.
Chạy trên 1 máy nên 2 agent phải đặt hostname khác nhau (`pc01`, `pc02`).
Nếu chạy trên nhiều máy thật, thay `127.0.0.1` bằng IP của máy chạy server.

1. **Terminal 1 — server:**
   ```bash
   make
   ./bin/server 5500
   ```
2. **Terminal 2 — agent 1:**
   ```bash
   ./bin/agent 127.0.0.1 5500 pc01
   ```
   Agent in `<- 100 OK`, `dang ky thanh cong`, rồi cứ 5 giây in 1 dòng `-> HEARTBEAT pc01 cpu=.. ram=..`.
   Server log `agent pc01 (127.0.0.1) online`.
3. **Terminal 3 — agent 2:**
   ```bash
   ./bin/agent 127.0.0.1 5500 pc02
   ```
4. **Terminal 4 — admin:**
   ```bash
   ./bin/admin 127.0.0.1 5500
   ```
   - Chọn `2` khi chưa đăng nhập → `Ban can dang nhap truoc.`
   - Chọn `1`, nhập `admin` / `sai` → `Sai username hoac password.` (server log `login failed`)
   - Chọn `1`, nhập `admin` / `123456` → `Dang nhap thanh cong.`
   - Chọn `2` → bảng có cả `pc01` và `pc02` đều `online`, kèm % CPU và RAM:
     ```
     STT  | Hostname             | Trang thai | CPU   | RAM
     -----+----------------------+------------+-------+------
     1    | pc01                 | online     | 79%   | 42%
     2    | pc02                 | online     | 77%   | 50%
     ```
5. **Tắt 1 agent:** ở Terminal 3 (pc02), có 2 cách:
   - `Ctrl+C`: agent gửi `QUIT` rồi thoát → server chuyển `pc02` sang offline **ngay lập tức**
     (log `agent pc02 ... offline: quit`).
   - `Ctrl+Z` (tạm dừng tiến trình, kết nối vẫn mở nhưng ngừng gửi heartbeat) → minh hoạ
     **timeout**: sau hơn 15 giây server log `agent pc02 ... offline: heartbeat timeout` và
     chủ động đóng kết nối. (Gõ `fg` để chạy tiếp, agent sẽ báo mất kết nối và thoát.)
6. **Chờ hơn 15 giây**, ở admin chọn `2` lại → `pc02` hiện `offline`, CPU/RAM là `-`;
   `pc01` vẫn `online` với số liệu mới.
7. Chạy lại `./bin/agent 127.0.0.1 5500 pc02` → `pc02` trở lại `online`.
   Thử chạy thêm 1 agent `pc01` nữa → bị từ chối `304 HOSTNAME_IN_USE`.
8. Ở admin chọn `3` (đăng xuất), rồi `0` (thoát, nhận `101 BYE`).
9. Xem toàn bộ sự kiện trong `server.log`.

## Tiến độ

- [x] Khởi tạo project
- [x] Thiết kế giao thức (xem [docs/protocol.md](docs/protocol.md))
- [x] Xử lý truyền dòng (`common/stream.c`, kiểm tra bằng `make test`)
- [x] Server đa client (`select()`, log vào `server.log`)
- [x] Agent heartbeat + Admin xem danh sách (xem mục [Demo](#demo))
