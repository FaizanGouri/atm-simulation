CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -I"C:/Program Files/MySQL/MySQL Server 8.0/include"
LDFLAGS = -L"C:/Program Files/MySQL/MySQL Server 8.0/lib" -lmysql

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TARGET = $(BUILD_DIR)/atm_simulation.exe

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/auth.c \
       $(SRC_DIR)/customer.c \
       $(SRC_DIR)/account.c \
       $(SRC_DIR)/card.c \
       $(SRC_DIR)/transaction.c \
       $(SRC_DIR)/withdrawal.c \
       $(SRC_DIR)/deposit.c \
       $(SRC_DIR)/transfer.c \
       $(SRC_DIR)/atm.c \
       $(SRC_DIR)/admin.c \
       $(SRC_DIR)/database.c \
       $(SRC_DIR)/security.c \
       $(SRC_DIR)/ui.c \
       $(SRC_DIR)/receipt.c \
       $(SRC_DIR)/validation.c \
       $(SRC_DIR)/utils.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)
