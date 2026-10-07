CC = gcc

# Base warning and language configuration
BASE_CFLAGS = -Wall -Wextra -std=c11 -Iinclude -I"C:/Program Files/MySQL/MySQL Server 8.0/include"

# SEC-L05 Compiler Hardening Flags
HARDENING_CFLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wformat -Wformat-security

# SEC-L05 Linker Hardening Flags (DEP / ASLR / NX compat for MinGW-w64)
HARDENING_LDFLAGS = -Wl,--dynamicbase -Wl,--nxcompat

# Full production flags
CFLAGS = $(BASE_CFLAGS) $(HARDENING_CFLAGS)
LDFLAGS = -L"C:/Program Files/MySQL/MySQL Server 8.0/lib" -lmysql $(HARDENING_LDFLAGS)

SRC_DIR = src
INC_DIR = include
TESTS_DIR = tests
BUILD_DIR = build

TARGET = $(BUILD_DIR)/atm_simulation.exe
TEST_TARGET = $(BUILD_DIR)/atm_simulation_tests.exe

# Production sources and objects
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/auth.c \
       $(SRC_DIR)/customer.c \
       $(SRC_DIR)/account.c \
       $(SRC_DIR)/card.c \
       $(SRC_DIR)/transaction.c \
       $(SRC_DIR)/withdrawal.c \
       $(SRC_DIR)/deposit.c \
       $(SRC_DIR)/transfer.c \
       $(SRC_DIR)/beneficiary.c \
       $(SRC_DIR)/atm.c \
       $(SRC_DIR)/admin.c \
       $(SRC_DIR)/database.c \
       $(SRC_DIR)/security.c \
       $(SRC_DIR)/ui.c \
       $(SRC_DIR)/receipt.c \
       $(SRC_DIR)/validation.c \
       $(SRC_DIR)/utils.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Shared library objects (excluding main.o)
LIB_OBJS = $(filter-out $(BUILD_DIR)/main.o, $(OBJS))

# Test source and object
TEST_SRCS = $(TESTS_DIR)/test_main.c
TEST_OBJS = $(BUILD_DIR)/test_main.o

# Flags for testing build
TEST_CFLAGS = $(BASE_CFLAGS) $(HARDENING_CFLAGS) -DATM_TESTING

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_main.o: $(TESTS_DIR)/test_main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET) $(TEST_TARGET)
