# Makefile cho Hệ thống giám sát phòng máy (IT4062)
# Dùng được trên cả macOS và Linux.
#
#   make          -> build cả 3 chương trình (server, agent, admin)
#   make server   -> chỉ build server (tương tự cho agent, admin)
#   make test     -> build và chạy chương trình test
#   make clean    -> xoá thư mục bin/

# ---- Trình biên dịch và cờ biên dịch ----
CC      = gcc
CFLAGS  = -Wall -Wextra -g -Icommon

# ---- Thư mục chứa file chạy ----
BIN_DIR = bin

# ---- Mã nguồn ----
# wildcard: tự tìm mọi file khớp mẫu. Thêm file .c mới vào common/
# là nó tự được link vào cả 3 chương trình, không cần sửa Makefile.
COMMON_SRC = $(wildcard common/*.c)
COMMON_HDR = $(wildcard common/*.h)

SERVER_SRC = $(wildcard server/*.c)
AGENT_SRC  = $(wildcard agent/*.c)
ADMIN_SRC  = $(wildcard admin/*.c)

# Code test nằm riêng trong tests/, chỉ dùng cho "make test",
# không bao giờ được link vào server/agent/admin.
TEST_SRC   = tests/test_stream.c

# ---- Target ----
# .PHONY: các target này là "tên lệnh", không phải tên file.
# Bắt buộc với server/agent/admin vì đã có thư mục trùng tên,
# nếu không make sẽ tưởng "server" đã tồn tại và không build nữa.
.PHONY: all server agent admin test clean

# Target đầu tiên là mặc định khi gõ "make"
all: server agent admin

server: $(BIN_DIR)/server
agent:  $(BIN_DIR)/agent
admin:  $(BIN_DIR)/admin

# Quy tắc build từng binary:
#   - Phía trước dấu "|": nếu các file này thay đổi thì build lại.
#   - Sau dấu "|" (order-only): chỉ cần thư mục bin/ tồn tại trước.
#   - $@ là tên target (vd: bin/server).
$(BIN_DIR)/server: $(SERVER_SRC) $(COMMON_SRC) $(COMMON_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(COMMON_SRC)

$(BIN_DIR)/agent: $(AGENT_SRC) $(COMMON_SRC) $(COMMON_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(AGENT_SRC) $(COMMON_SRC)

$(BIN_DIR)/admin: $(ADMIN_SRC) $(COMMON_SRC) $(COMMON_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(ADMIN_SRC) $(COMMON_SRC)

# Build chương trình test rồi chạy luôn.
# Nếu có test FAIL, chương trình trả mã khác 0 và make báo lỗi.
test: $(BIN_DIR)/test_stream
	./$(BIN_DIR)/test_stream

$(BIN_DIR)/test_stream: $(TEST_SRC) $(COMMON_SRC) $(COMMON_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(COMMON_SRC)

# Tạo thư mục bin/ nếu chưa có (-p: không báo lỗi nếu đã tồn tại)
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Xoá toàn bộ file build
clean:
	rm -rf $(BIN_DIR)
