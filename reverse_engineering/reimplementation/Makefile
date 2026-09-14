# Clean-room G6 HE behavioural model - host build and test.
#
# The library builds for any hosted C11 target. It is intentionally
# hardware-independent: no vendor SDK, no nRF54 HAL, no device access.
#
# There is no target-flash artefact here and no signing step. See README.md.

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -g
CPPFLAGS += -Iinclude

BUILD_DIR := build
LIB_SRCS := \
	src/g6he_crc.c \
	src/g6he_sha512.c \
	src/g6he_sha256.c \
	src/g6he_mcuboot.c \
	src/g6he_identity.c \
	src/g6he_hall.c \
	src/g6he_defaults.c \
	src/g6he_protocol.c \
	src/g6he_event.c \
	src/g6he_optomagnetic.c \
	src/g6he_sensor.c \
	src/g6he_power.c \
	src/g6he_settings.c \
	src/g6he_hid.c \
	src/g6he_wireless.c

TEST_SRCS := \
	tests/test_main.c \
	tests/test_crc.c \
	tests/test_sha512.c \
	tests/test_sha256.c \
	tests/test_version.c \
	tests/test_hall.c \
	tests/test_defaults.c \
	tests/test_identity.c \
	tests/test_mcuboot.c \
	tests/test_protocol.c \
	tests/test_integration.c \
	tests/test_target_arch.c

LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS := $(TEST_SRCS:tests/%.c=$(BUILD_DIR)/tests_%.o)

.PHONY: all test target-smoke clean

all: test target-smoke

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests_%.o: tests/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/target_main.o: zephyr/src/main.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/g6he_tests: $(LIB_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Host smoke build of the Zephyr target entry point (hardware disabled). The
# artifact is deliberately named as an unsigned engineering build. It is not
# signed and there is no flash target anywhere in this tree.
UNSIGNED_NAME := ENGINEERING-UNSIGNED-NOT-FOR-FLASHING

target-smoke: $(LIB_OBJS) $(BUILD_DIR)/target_main.o
	$(CC) $(CFLAGS) $^ -o $(BUILD_DIR)/$(UNSIGNED_NAME)
	@echo "built build/$(UNSIGNED_NAME): unsigned engineering artifact, NOT flashable"

# The integration tests read authorized +84 and +87 artifacts read-only when
# available. The +84 fallback matches this repository's historical test fixture;
# provide +87 explicitly when authorized. No vendor image is copied by this build.
REAL_IMAGE ?= $(CURDIR)/../../firmware/G6HE_v1.0.0+84_202609101503.signed.bin
REAL_IMAGE_87 ?=

test: $(BUILD_DIR)/g6he_tests
	G6HE_REAL_IMAGE="$(REAL_IMAGE)" \
	G6HE_REAL_IMAGE_87="$(REAL_IMAGE_87)" \
	$(BUILD_DIR)/g6he_tests

clean:
	rm -rf $(BUILD_DIR)
