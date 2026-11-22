# Giao thức ứng dụng — Hệ thống giám sát phòng máy

Tài liệu mô tả giao thức tầng ứng dụng giữa **agent**, **admin** và **server**.
Giao thức chạy trên TCP, dạng text (giống SMTP/FTP), dễ đọc và dễ thử bằng `nc`/`telnet`.

Các hằng số trong tài liệu này được định nghĩa trong [`common/protocol.h`](../common/protocol.h).
Hai file phải luôn khớp nhau.

---

## 1. Quy ước chung

### 1.1. Khung thông điệp

- Mỗi thông điệp là **1 dòng text ASCII**, kết thúc bằng `\r\n` (CR LF).
- Độ dài tối đa của 1 dòng là **1024 byte, tính cả `\r\n`** (`MAX_LINE`).
- Dạng của một lệnh (client → server):

  ```
  LỆNH tham_số1 tham_số2 ...\r\n
  ```

  - Các phần cách nhau bởi **đúng 1 dấu cách**.
  - Tên lệnh viết **HOA** và phân biệt hoa/thường (`hello` là lệnh không tồn tại).
  - Tham số **không chứa dấu cách**.
- Dòng rỗng (chỉ có `\r\n`) bị server bỏ qua, không phản hồi.

> Vì TCP là luồng byte (stream), một lần `recv()` có thể nhận được nửa dòng hoặc nhiều dòng.
> Bên nhận phải gom dữ liệu vào bộ đệm và chỉ xử lý khi gặp `\r\n`.

### 1.2. Phản hồi của server

Mỗi lệnh client gửi lên nhận **đúng 1 phản hồi**, trừ `HEARTBEAT` thành công thì server
**không phản hồi** (giảm lưu lượng vì agent gửi 5 giây/lần; `HEARTBEAT` lỗi vẫn có phản hồi). Dạng:

```
MÃ_SỐ TÊN_MÃ [dữ liệu]\r\n
```

- `MÃ_SỐ`: 3 chữ số, chương trình dựa vào đây để xử lý.
- `TÊN_MÃ`: tên ngắn để người đọc dễ hiểu (vd `OK`, `BAD_SYNTAX`).
- `[dữ liệu]`: tuỳ chọn, phụ thuộc lệnh.


### 1.3. Giới hạn tham số

| Tham số      | Quy định                                                    |
|--------------|-------------------------------------------------------------|
| `<hostname>` | 1–63 ký tự, chỉ gồm chữ, số, `-`, `_`, `.` (`MAX_HOSTNAME`) |
| `<ip>`       | Địa chỉ IPv4 dạng chấm, vd `192.168.1.10`                   |
| `cpu=<n>`    | Số nguyên 0–100 (phần trăm CPU đang dùng)                   |
| `ram=<n>`    | Số nguyên 0–100 (phần trăm RAM đang dùng)                   |
| `<username>` | 1–31 ký tự, không có dấu cách (`MAX_USERNAME`)              |
| `<password>` | 1–31 ký tự, không có dấu cách (`MAX_PASSWORD`)              |

Sai bất kỳ quy định nào → `201 BAD_SYNTAX`.

### 1.4. Vai trò của một kết nối

Khi mới kết nối, server chưa biết bên kia là ai. Vai trò được xác định bởi lệnh đầu tiên thành công:

- `HELLO AGENT ...` thành công → kết nối là **agent**.
- `LOGIN ...` thành công → kết nối là **admin**.

Một kết nối chỉ có 1 vai trò. Lệnh dành cho vai trò khác sẽ bị từ chối (mã 3xx).

### 1.5. Các hằng số thời gian

| Hằng số              | Giá trị | Ý nghĩa                                                      |
|----------------------|---------|--------------------------------------------------------------|
| `DEFAULT_PORT`       | 5500    | Cổng mặc định của server                                     |
| `HEARTBEAT_INTERVAL` | 5 giây  | Chu kỳ agent gửi `HEARTBEAT`                                 |
| `AGENT_TIMEOUT`      | 15 giây | Quá thời gian này không nhận heartbeat → server coi máy `offline` |

---

## 2. Bảng mã phản hồi

| Nhóm | Ý nghĩa                         |
|------|---------------------------------|
| 1xx  | Thành công                      |
| 2xx  | Lỗi cú pháp / lệnh không tồn tại |
| 3xx  | Lỗi xác thực / quyền / trạng thái |
| 5xx  | Lỗi phía server                 |

| Mã  | Tên                 | Hằng số trong code      | Ý nghĩa                                                         |
|-----|---------------------|-------------------------|-----------------------------------------------------------------|
| 100 | `OK`                | `RES_OK`                | Lệnh thực hiện thành công                                       |
| 101 | `BYE`               | `RES_BYE`               | Đồng ý đóng kết nối (trả lời `QUIT`)                            |
| 110 | `LOGIN_OK`          | `RES_LOGIN_OK`          | Đăng nhập thành công                                            |
| 120 | `LIST`              | `RES_LIST`              | Bắt đầu danh sách máy, kèm số dòng dữ liệu theo sau             |
| 200 | `UNKNOWN_CMD`       | `RES_UNKNOWN_CMD`       | Lệnh không tồn tại                                              |
| 201 | `BAD_SYNTAX`        | `RES_BAD_SYNTAX`        | Sai số lượng hoặc định dạng tham số                             |
| 202 | `LINE_TOO_LONG`     | `RES_LINE_TOO_LONG`     | Dòng dài quá `MAX_LINE` byte, cả dòng bị bỏ                     |
| 300 | `NOT_LOGGED_IN`     | `RES_NOT_LOGGED_IN`     | Lệnh cần đăng nhập admin nhưng kết nối chưa đăng nhập           |
| 301 | `WRONG_PASS`        | `RES_WRONG_PASS`        | Sai tên đăng nhập hoặc mật khẩu                                 |
| 302 | `ALREADY_LOGGED_IN` | `RES_ALREADY_LOGGED_IN` | Kết nối đã có vai trò (đã `HELLO` hoặc đã `LOGIN`)               |
| 303 | `NOT_REGISTERED`    | `RES_NOT_REGISTERED`    | Gửi `HEARTBEAT` khi chưa `HELLO`, hoặc hostname không khớp       |
| 304 | `HOSTNAME_IN_USE`   | `RES_HOSTNAME_IN_USE`   | `HELLO` với hostname đang được 1 agent online khác sử dụng       |
| 500 | `SERVER_ERR`        | `RES_SERVER_ERR`        | Lỗi nội bộ của server                                           |
| 501 | `SERVER_FULL`       | `RES_SERVER_FULL`       | Server đã đủ số kết nối tối đa, server đóng kết nối ngay sau đó  |

> `301 WRONG_PASS` dùng chung cho cả sai username lẫn sai password, để người ngoài
> không dò được username nào tồn tại.

**Quy tắc kiểm tra theo thứ tự:** server kiểm tra lệnh có tồn tại không (200) → đúng cú pháp
không (201) → có quyền / đúng trạng thái không (3xx) → thực hiện.

---

## 3. Các lệnh

### 3.1. `HELLO` — agent đăng ký với server

- **Ai gửi:** agent, ngay sau khi kết nối.
- **Cú pháp:** `HELLO AGENT <hostname> <ip>`
- **Ý nghĩa:** báo cho server biết kết nối này là agent của máy `<hostname>` có địa chỉ `<ip>`.
  Server thêm máy vào danh sách với trạng thái `online`. Nếu hostname đã có trong danh sách
  ở trạng thái `offline` (máy kết nối lại) thì server cập nhật thông tin và đánh dấu lại `online`.
- **Phản hồi:**
  - `100 OK` — đăng ký thành công
  - `201 BAD_SYNTAX` — thiếu/thừa tham số, từ thứ hai không phải `AGENT`, hostname hoặc ip sai định dạng
  - `302 ALREADY_LOGGED_IN` — kết nối này đã `HELLO` hoặc đã `LOGIN` trước đó
  - `304 HOSTNAME_IN_USE` — hostname đang `online` trên 1 kết nối khác
- **Ví dụ:**
  ```
  C: HELLO AGENT lab1-pc01 192.168.1.11
  S: 100 OK
  ```

### 3.2. `HEARTBEAT` — agent báo còn sống

- **Ai gửi:** agent, định kỳ mỗi `HEARTBEAT_INTERVAL` = 5 giây sau khi `HELLO` thành công.
- **Cú pháp:** `HEARTBEAT <hostname> cpu=<n> ram=<n>`
- **Ý nghĩa:** cập nhật % CPU, % RAM và thời điểm nhận heartbeat cuối cùng của máy.
  Máy chuyển sang `offline` (vẫn giữ trong danh sách) khi:
  - kết nối bị đóng (agent gửi `QUIT`, tắt đột ngột, mất mạng được TCP phát hiện), hoặc
  - quá `AGENT_TIMEOUT` = 15 giây (tức lỡ 3 heartbeat liên tiếp) không nhận được heartbeat.
    Khi đó server coi agent đã treo và **chủ động đóng kết nối**.
  Server kiểm tra timeout ít nhất 1 lần/giây (timeout 1 giây của `select()`).
- **Phản hồi:**
  - Thành công: **không có phản hồi**
  - `201 BAD_SYNTAX` — sai số tham số, thiếu tiền tố `cpu=`/`ram=` hoặc sai thứ tự, giá trị
    không phải số nguyên 0–100
  - `303 NOT_REGISTERED` — kết nối chưa `HELLO`, hoặc `<hostname>` khác hostname đã đăng ký
- **Ví dụ:**
  ```
  C: HEARTBEAT lab1-pc01 cpu=23 ram=61
     (server không trả lời)
  C: HEARTBEAT lab1-pc01 cpu=150 ram=61
  S: 201 BAD_SYNTAX
  ```

### 3.3. `LOGIN` — admin đăng nhập

- **Ai gửi:** admin, ngay sau khi kết nối.
- **Cú pháp:** `LOGIN <username> <password>`
- **Ý nghĩa:** xác thực quản trị viên. Hiện tại server dùng 1 tài khoản viết cứng `admin` / `123456` (sẽ đổi sang đọc từ file).
  Mật khẩu gửi dạng rõ (plain text) — đây là giới hạn đã biết của bài tập, chưa mã hoá.
- **Phản hồi:**
  - `110 LOGIN_OK <username>`
  - `201 BAD_SYNTAX` — thiếu/thừa tham số, quá độ dài cho phép
  - `301 WRONG_PASS` — sai username hoặc password
  - `302 ALREADY_LOGGED_IN` — kết nối đã đăng nhập, hoặc là kết nối agent
- **Ví dụ:**
  ```
  C: LOGIN admin secret123
  S: 110 LOGIN_OK admin
  ```
  ```
  C: LOGIN admin wrongpass
  S: 301 WRONG_PASS
  ```

### 3.4. `LOGOUT` — admin đăng xuất

- **Ai gửi:** admin đã đăng nhập.
- **Cú pháp:** `LOGOUT` (không có tham số)
- **Ý nghĩa:** huỷ đăng nhập nhưng **giữ kết nối**. Sau đó có thể `LOGIN` lại.
- **Phản hồi:**
  - `100 OK`
  - `201 BAD_SYNTAX` — có tham số thừa
  - `300 NOT_LOGGED_IN` — chưa đăng nhập (kể cả khi là kết nối agent)
- **Ví dụ:**
  ```
  C: LOGOUT
  S: 100 OK
  ```

### 3.5. `LIST_AGENTS` — admin xem danh sách máy

- **Ai gửi:** admin, **chỉ khi đã đăng nhập**.
- **Cú pháp:** `LIST_AGENTS` (không có tham số)
- **Ý nghĩa:** lấy danh sách tất cả các máy server biết (cả `online` và `offline`).
- **Phản hồi:**
  - `120 LIST <n> <mục_1> <mục_2> ... <mục_n>` — **tất cả trên 1 dòng**, mỗi mục 1 máy:
    ```
    <hostname>:<status>:<cpu>:<ram>
    ```
    - `<n>`: tổng số máy server biết; có thể bằng 0 (không có mục nào)
    - `<status>`: `online` hoặc `offline`
    - `<cpu>`, `<ram>`: % của heartbeat gần nhất. Dùng `-` khi máy `offline`,
      hoặc khi máy `online` nhưng chưa gửi heartbeat nào.
  - `201 BAD_SYNTAX` — có tham số thừa
  - `300 NOT_LOGGED_IN` — chưa đăng nhập
- **Ví dụ:**
  ```
  C: LIST_AGENTS
  S: 120 LIST 2 lab1-pc01:online:23:61 lab1-pc02:offline:-:-
  ```

> **Giới hạn:** cả dòng (tính `\r\n`) không được vượt `MAX_LINE` = 1024 byte. Mỗi mục dài tối đa
> ~80 byte (hostname 63 ký tự), thường ~25 byte (vd `lab1-pc01:online:23:61`), nên 1 dòng chứa
> được khoảng 12 (trường hợp xấu nhất) đến ~40 máy. Nếu không đủ chỗ, server **cắt bớt** các mục
> cuối nhưng vẫn giữ `<n>` là tổng số máy; client thấy số mục nhận được < `<n>` thì biết danh sách
> bị cắt. Phòng lab lớn hơn cần đổi sang dạng nhiều dòng (dự kiến ở giai đoạn sau).

### 3.6. `QUIT` — đóng kết nối

- **Ai gửi:** agent hoặc admin, bất cứ lúc nào.
- **Cú pháp:** `QUIT` (không có tham số)
- **Ý nghĩa:** server trả lời rồi đóng kết nối. Nếu là agent thì máy chuyển `offline`;
  nếu là admin đang đăng nhập thì tự động đăng xuất.
- **Phản hồi:**
  - `101 BYE`
  - `201 BAD_SYNTAX` — có tham số thừa (kết nối **không** bị đóng)
- **Ví dụ:**
  ```
  C: QUIT
  S: 101 BYE
  ```

### 3.7. Tóm tắt

| Lệnh          | Ai gửi       | Cú pháp                               | Phản hồi thành công |
|---------------|--------------|---------------------------------------|---------------------|
| `HELLO`       | agent        | `HELLO AGENT <hostname> <ip>`         | `100 OK`            |
| `HEARTBEAT`   | agent        | `HEARTBEAT <hostname> cpu=<n> ram=<n>`| *(không phản hồi)*  |
| `LOGIN`       | admin        | `LOGIN <username> <password>`         | `110 LOGIN_OK`      |
| `LOGOUT`      | admin        | `LOGOUT`                              | `100 OK`            |
| `LIST_AGENTS` | admin        | `LIST_AGENTS`                         | `120 LIST <n>`      |
| `QUIT`        | agent, admin | `QUIT`                                | `101 BYE`           |

Lệnh không có trong bảng → `200 UNKNOWN_CMD`. Dòng quá dài → `202 LINE_TOO_LONG`.

---

## 4. Sơ đồ trình tự

Kịch bản: agent `lab1-pc01` kết nối và gửi heartbeat; admin đăng nhập và xem danh sách máy.

```
   agent (lab1-pc01)               server                          admin
        |                            |                               |
        |------ TCP connect -------->|                               |
        |  HELLO AGENT lab1-pc01     |                               |
        |        192.168.1.11        |                               |
        |--------------------------->|  thêm lab1-pc01, online       |
        |<--------- 100 OK ----------|                               |
        |                            |                               |
        |  HEARTBEAT lab1-pc01       |                               |
        |        cpu=23 ram=61       |                               |
        |--------------------------->|  cập nhật cpu, ram,           |
        |   (không có phản hồi)      |  last_heartbeat               |
        |                            |<-------- TCP connect ---------|
        |                            |                               |
        |                            |<---- LOGIN admin secret123 ---|
        |                            |---- 110 LOGIN_OK admin ------>|
        |   ... 5 giây sau ...       |                               |
        |  HEARTBEAT lab1-pc01       |                               |
        |        cpu=30 ram=62       |                               |
        |--------------------------->|                               |
        |                            |<-------- LIST_AGENTS ---------|
        |                            |--- 120 LIST 1                 |
        |                            |    lab1-pc01:online:30:62 --->|
        |                            |                               |
        |                            |<----------- QUIT -------------|
        |                            |----------- 101 BYE ---------->|
        |                            |  đóng kết nối admin           |
        |   (tiếp tục heartbeat      |                               |
        |    mỗi 5 giây ...)         |                               |
```

Nếu agent mất kết nối hoặc ngừng gửi heartbeat quá 15 giây, lần `LIST_AGENTS` tiếp theo sẽ
thấy `lab1-pc01:offline:-:-`.
