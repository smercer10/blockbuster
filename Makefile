MAKEFLAGS += --no-print-directory

BUILD_DIR = build

all:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

test: all
	cd $(BUILD_DIR) && ctest

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean
