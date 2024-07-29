MAKEFLAGS += --no-print-directory

BUILD_DIR := build

build:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

.PHONY: build test clean
