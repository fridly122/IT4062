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
make clean      # xoá thư mục bin/
```

## Chạy

```bash
./bin/server <port>                  # vd: ./bin/server 5500
./bin/agent  <server_ip> <port>      # vd: ./bin/agent 127.0.0.1 5500
./bin/admin  <server_ip> <port>      # vd: ./bin/admin 127.0.0.1 5500
```

## Tiến độ

- [x] Khởi tạo project
- [ ] Thiết kế giao thức
- [ ] Xử lý truyền dòng
- [ ] Server đa client
- [ ] Agent heartbeat + Admin xem danh sách
