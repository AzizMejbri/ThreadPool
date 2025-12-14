# -----------------------------------------------------
# Compiler & Flags
# -----------------------------------------------------
CC      := gcc
NASM    := nasm
CFLAGS  := -Wall -Wextra -Wpedantic -D_GNU_SOURCE -I.
NASMFLAGS := -f elf64  
LDFLAGS := -ldefer

# Build mode (debug by default, use --release flag for release)
BUILD_MODE := debug

# Parse command line arguments
ifneq (,$(findstring --release,$(MAKECMDGOALS)))
    BUILD_MODE := release
endif

ifeq ($(BUILD_MODE),debug)
    CFLAGS += -g -O0 -DDEBUG
    NASMFLAGS += -g -F dwarf   # Add debug info for NASM
else
    CFLAGS += -O3 -DNDEBUG
endif

# -----------------------------------------------------
# Project structure
# -----------------------------------------------------

# Output directory
OUT_DIR := out
BIN_DIR := $(OUT_DIR)/bin

# Main executable
TARGET  := $(BIN_DIR)/main

# Find all .c files (excluding tests ONLY, NOT utils)
SRC_C := $(shell find . -type f -name "*.c" -not -path "./test/*")

# Find all .s (assembly) files (excluding tests ONLY, NOT utils)
SRC_ASM := $(shell find . -type f -name "*.s" -not -path "./test/*")

# Object files for C sources
OBJ_C := $(patsubst ./%, $(OUT_DIR)/%, $(SRC_C:.c=.o))

# Object files for assembly sources
OBJ_ASM := $(patsubst ./%, $(OUT_DIR)/%, $(SRC_ASM:.s=.o))

# Combine all object files
OBJ := $(OBJ_C) $(OBJ_ASM)

# -----------------------------------------------------
# Rules
# -----------------------------------------------------

# Default target
all: build

# Build with debug mode by default, or --release flag
build: $(TARGET)

# Release build
release: BUILD_MODE := release
release: CFLAGS := $(filter-out -g -O0 -DDEBUG, $(CFLAGS))
release: CFLAGS += -O3 -DNDEBUG
release: NASMFLAGS := $(filter-out -g -F dwarf, $(NASMFLAGS))
release: $(TARGET)

# Debug build
debug: BUILD_MODE := debug
debug: CFLAGS := $(filter-out -O3 -DNDEBUG, $(CFLAGS))
debug: CFLAGS += -g -O0 -DDEBUG
debug: NASMFLAGS += -g -F dwarf
debug: $(TARGET)

# Run the main executable
run: build
	@echo "Running $(TARGET)..."
	@./$(TARGET)

# Test targets
test: test-quic

# Test QUIC library (if exists)
test-quic:
	@if [ -f "test/quic/quic.c" ]; then \
		echo "Running QUIC tests..."; \
		$(MAKE) test--quic; \
	else \
		echo "No QUIC tests found at test/quic/quic.c"; \
		echo "Create test files with: mkdir -p test/quic && touch test/quic/quic.c"; \
	fi

# Test with specific module
test-%:
	$(eval MODULE := $(patsubst test-%,%,$@))
	@echo "Building and running test for module: $(MODULE)"
	@if [ -f "test/$(MODULE)/$(MODULE).c" ]; then \
		$(CC) $(CFLAGS) test/$(MODULE)/$(MODULE).c $(filter-out $(OUT_DIR)/components/main.o, $(OBJ)) -o $(BIN_DIR)/test_$(MODULE) $(LDFLAGS); \
		echo "Running test for $(MODULE)..."; \
		./$(BIN_DIR)/test_$(MODULE); \
	else \
		echo "Error: Test file not found at test/$(MODULE)/$(MODULE).c"; \
		echo "Available tests:"; \
		find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
			module_name=$$(basename $$(dirname $$test_file)); \
			if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
				echo "  make test--$$module_name"; \
			fi \
		done || echo "  No tests found. Create test/module/module.c files."; \
		exit 1; \
	fi

# Test all (run all tests found)
test-all:
	@echo "Running all tests..."
	@found=0; \
	find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
		module_name=$$(basename $$(dirname $$test_file)); \
		if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
			found=1; \
			echo "\n=== Testing $$module_name ==="; \
			$(MAKE) test--$$module_name || exit 1; \
		fi \
	done; \
	if [ $$found -eq 0 ]; then \
		echo "No tests found. Create test/module/module.c files."; \
	fi

# Create component structure
new-component:
	@if [ -z "$(NAME)" ]; then \
		echo "Usage: make new-component NAME=component_name"; \
		exit 1; \
	fi
	@mkdir -p components/$(NAME)
	@printf '#include "%s.h"\n#include <stdio.h>\n\nvoid %s_init(void) {\n    printf("%s initialized\\n");\n}\n\nvoid %s_cleanup(void) {\n    printf("%s cleaned up\\n");\n}\n' "$(NAME)" "$(NAME)" "$(NAME)" "$(NAME)" "$(NAME)" > components/$(NAME)/$(NAME).c
	@printf '#ifndef %s_H\n#define %s_H\n\nvoid %s_init(void);\nvoid %s_cleanup(void);\n\n#endif\n' "$(shell echo $(NAME) | tr '[:lower:]' '[:upper:]')" "$(shell echo $(NAME) | tr '[:lower:]' '[:upper:]')" "$(NAME)" "$(NAME)" > components/$(NAME)/$(NAME).h
	@echo "Created component: components/$(NAME)/"

# Create assembly component
new-asm:
	@if [ -z "$(NAME)" ]; then \
		echo "Usage: make new-asm NAME=component_name"; \
		exit 1; \
	fi
	@mkdir -p components/$(NAME)
	@printf 'section .text\n\nglobal %s_function\n\nextern printf\n\n%s_function:\n    push rbp\n    mov rbp, rsp\n    \n    ; Your assembly code here\n    \n    pop rbp\n    ret\n' "$(NAME)" "$(NAME)" > components/$(NAME)/$(NAME).s
	@printf '#ifndef %s_H\n#define %s_H\n\nvoid %s_function(void);\n\n#endif\n' "$(shell echo $(NAME) | tr '[:lower:]' '[:upper:]')" "$(shell echo $(NAME) | tr '[:lower:]' '[:upper:]')" "$(NAME)" > components/$(NAME)/$(NAME).h
	@echo "Created assembly component: components/$(NAME)/$(NAME).s"

# Create test for component
new-test:
	@if [ -z "$(NAME)" ]; then \
		echo "Usage: make new-test NAME=component_name"; \
		exit 1; \
	fi
	@mkdir -p test/$(NAME)
	@printf '#include <stdio.h>\n#include <assert.h>\n\n// Include component headers here\n// #include "components/%s/%s.h"\n\nint main(void) {\n    printf("Testing %s...\\n");\n    \n    // Test cases\n    printf("✓ Test 1 passed\\n");\n    printf("✓ Test 2 passed\\n");\n    \n    printf("All %s tests passed!\\n");\n    return 0;\n}\n' "$(NAME)" "$(NAME)" "$(NAME)" "$(NAME)" > test/$(NAME)/$(NAME).c
	@echo "Created test: test/$(NAME)/$(NAME).c"

# Main executable
$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	@echo "$(BUILD_MODE) build complete → $@"

# Generic compilation rule for C files
$(OUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Generic assembly rule for NASM files
$(OUT_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OUT_DIR)
	@echo "Cleaned build directory"

# Show file lists (for debugging)
list-files:
	@echo "=== C Source Files ==="
	@echo "$(SRC_C)" | tr ' ' '\n'
	@echo ""
	@echo "=== Assembly Source Files ==="
	@echo "$(SRC_ASM)" | tr ' ' '\n'
	@echo ""
	@echo "=== Object Files ==="
	@echo "$(OBJ)" | tr ' ' '\n'

# Show help
help:
	@echo "Available commands:"
	@echo "  make build          - Build in debug mode (default)"
	@echo "  make build --release - Build in release mode"
	@echo "  make debug          - Build in debug mode"
	@echo "  make release        - Build in release mode"
	@echo "  make run            - Build and run main executable"
	@echo "  make test           - Run all tests"
	@echo "  make test--<name>   - Build and run specific test"
	@echo "  make test-all       - Build and run all available tests"
	@echo "  make new-component NAME=name - Create new C component"
	@echo "  make new-asm NAME=name       - Create new assembly component"
	@echo "  make new-test NAME=name      - Create new test"
	@echo "  make clean          - Remove build artifacts"
	@echo "  make list-files     - Show source files being compiled"
	@echo "  make help           - Show this help"
	@echo ""
	@echo "Available tests:"
	@found=0; \
	find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
		module_name=$$(basename $$(dirname $$test_file)); \
		if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
			found=1; \
			echo "  make test--$$module_name"; \
		fi \
	done; \
	if [ $$found -eq 0 ]; then \
		echo "  No tests found. Create test/module/module.c files."; \
	fi
	@echo ""
	@echo "Assembly files found:"
	@if [ -n "$(SRC_ASM)" ]; then \
		echo "$(SRC_ASM)" | tr ' ' '\n' | while read asm_file; do \
			echo "  $$asm_file"; \
		done; \
	else \
		echo "  No assembly files found."; \
	fi

# -----------------------------------------------------
# Phony targets
# -----------------------------------------------------
.PHONY: all build release debug run test test-quic test-all clean help new-component new-test new-asm list-files

# Handle --release flag
.PHONY: --release
--release:
	@# This target exists only to parse the --release flag
