# -----------------------------------------------------
# Compiler & Flags
# -----------------------------------------------------
CC      := gcc
NASM    := nasm
CFLAGS  := -Wall -Wextra -Wpedantic -D_GNU_SOURCE -I.
NASMFLAGS := -f elf64  
LDFLAGS := -ldefer

# Sanitizer and performance flags
SANITIZE_FLAGS := -fsanitize=address -fsanitize=undefined -fsanitize=leak
PERF_FLAGS := -O3 -march=native -flto

# Base CFLAGS variants
BASE_DEBUG_CFLAGS := -g -O0 -DDEBUG
BASE_RELEASE_CFLAGS := -O3 -DNDEBUG

# Build mode (debug by default, use --release flag for release)
BUILD_MODE := debug

# Parse command line arguments
ifneq (,$(findstring --release,$(MAKECMDGOALS)))
    BUILD_MODE := release
endif

ifeq ($(BUILD_MODE),debug)
    CFLAGS += $(BASE_DEBUG_CFLAGS)
    NASMFLAGS += -g -F dwarf   # Add debug info for NASM
else
    CFLAGS += $(BASE_RELEASE_CFLAGS)
endif

# -----------------------------------------------------
# Project structure
# -----------------------------------------------------

# Output directory
OUT_DIR := out
BIN_DIR := $(OUT_DIR)/bin
OBJ_DIR := $(OUT_DIR)/obj
ASAN_OBJ_DIR := $(OUT_DIR)/obj-asan
PERF_OBJ_DIR := $(OUT_DIR)/obj-perf

# Main executable
TARGET  := $(BIN_DIR)/main

# Find all .c files (excluding tests ONLY, NOT utils)
SRC_C := $(shell find . -type f -name "*.c" -not -path "./test/*")

# Find all .s (assembly) files (excluding tests ONLY, NOT utils)
SRC_ASM := $(shell find . -type f -name "*.s" -not -path "./test/*")

# Helper function to convert source to object path
src_to_obj = $(subst ./,$(1)/,$(2:.c=.o))
src_to_asm_obj = $(subst ./,$(1)/,$(2:.s=.o))

# Object files for regular build
OBJ_C := $(call src_to_obj,$(OBJ_DIR),$(SRC_C))
OBJ_ASM := $(call src_to_asm_obj,$(OBJ_DIR),$(SRC_ASM))
OBJ := $(OBJ_C) $(OBJ_ASM)

# Object files for ASAN build
OBJ_C_ASAN := $(call src_to_obj,$(ASAN_OBJ_DIR),$(SRC_C))
OBJ_ASM_ASAN := $(call src_to_asm_obj,$(ASAN_OBJ_DIR),$(SRC_ASM))
OBJ_ASAN := $(OBJ_C_ASAN) $(OBJ_ASM_ASAN)


# Object files for PERF build
OBJ_C_PERF := $(call src_to_obj,$(PERF_OBJ_DIR),$(SRC_C))
OBJ_ASM_PERF := $(call src_to_asm_obj,$(PERF_OBJ_DIR),$(SRC_ASM))
OBJ_PERF := $(OBJ_C_PERF) $(OBJ_ASM_PERF)

# -----------------------------------------------------
# Rules
# -----------------------------------------------------

# Default target
all: build

# Build with debug mode by default, or --release flag
build: $(TARGET)

# Release build
release: BUILD_MODE := release
release: CFLAGS := $(filter-out $(BASE_DEBUG_CFLAGS), $(CFLAGS))
release: CFLAGS += $(BASE_RELEASE_CFLAGS)
release: NASMFLAGS := $(filter-out -g -F dwarf, $(NASMFLAGS))
release: $(TARGET)

# Debug build
debug: BUILD_MODE := debug
debug: CFLAGS := $(filter-out $(BASE_RELEASE_CFLAGS), $(CFLAGS))
debug: CFLAGS += $(BASE_DEBUG_CFLAGS)
debug: NASMFLAGS += -g -F dwarf
debug: $(TARGET)

# Run the main executable
run: build
	@echo "Running $(TARGET)..."
	@./$(TARGET)

# Generic test compilation function
# Usage: $(call compile_test,module_name,obj_dir,extra_cflags,objects)
define compile_test
	@echo "Building test for module: $(1)"
	@if [ -f "test/$(1)/$(1).c" ]; then \
		$(CC) $(CFLAGS) $(3) test/$(1)/$(1).c $(filter-out $(2)/components/main.o, $(4)) -o $(BIN_DIR)/test_$(1) $(LDFLAGS); \
		echo "Running test for $(1)..."; \
		./$(BIN_DIR)/test_$(1); \
	else \
		echo "Error: Test file not found at test/$(1)/$(1).c"; \
		echo "Available tests:"; \
		find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
			module_name=$$(basename $$(dirname $$test_file)); \
			if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
				echo "  make test-$$module_name"; \
			fi \
		done || echo "  No tests found. Create test/module/module.c files."; \
		exit 1; \
	fi
endef

# ================== SPECIALIZED TEST TARGETS ==================
# These must come BEFORE the generic test-% rule!

# ASAN test (using ASAN objects)
asan-test-%: $(OBJ_ASAN)
	$(eval MODULE := $(patsubst asan-test-%,%,$@))
	$(call compile_test,$(MODULE),$(ASAN_OBJ_DIR),$(SANITIZE_FLAGS),$(OBJ_ASAN))


# Performance test (using PERF objects)
perf-test-%: $(OBJ_PERF)
	$(eval MODULE := $(patsubst perf-test-%,%,$@))
	$(call compile_test,$(MODULE),$(PERF_OBJ_DIR),$(PERF_FLAGS),$(OBJ_PERF))

# Valgrind test (using regular objects with debug symbols)
valgrind-test-%: $(OBJ)
	$(eval MODULE := $(patsubst valgrind-test-%,%,$@))
	@echo "Building valgrind test for module: $(MODULE)"
	@if [ -f "test/$(MODULE)/$(MODULE).c" ]; then \
		$(CC) $(CFLAGS) test/$(MODULE)/$(MODULE).c $(filter-out $(OBJ_DIR)/components/main.o, $(OBJ)) -o $(BIN_DIR)/test_$(MODULE)_valgrind $(LDFLAGS); \
		echo "Running valgrind for $(MODULE)..."; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./$(BIN_DIR)/test_$(MODULE)_valgrind; \
		if [ $$? -eq 0 ]; then \
			echo "✓ Valgrind test passed for $(MODULE)"; \
		fi \
	else \
		echo "Error: Test file not found at test/$(MODULE)/$(MODULE).c"; \
		exit 1; \
	fi

# ================== STANDARD TEST TARGET ==================
# Standard test (using regular objects)
test-%:
	$(eval MODULE := $(patsubst test-%,%,$@))
	$(call compile_test,$(MODULE),$(OBJ_DIR),,$(OBJ))

# ================== COMPREHENSIVE TEST TARGETS ==================

# Comprehensive test for a module (runs all test types)
all-test-%: test-% asan-test-% perf-test-% valgrind-test-%
	@echo ""
	@echo "✓ All tests completed for module: $(patsubst all-test-%,%,$@)"

# Test all modules with all test types
test-all:
	@echo "Running all tests for all modules..."
	@found=0; \
	find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
		module_name=$$(basename $$(dirname $$test_file)); \
		if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
			found=1; \
			echo "\n=== Testing $$module_name ==="; \
			$(MAKE) all-test-$$module_name || exit 1; \
		fi \
	done; \
	if [ $$found -eq 0 ]; then \
		echo "No tests found. Create test/module/module.c files."; \
	fi

# Quick test (just standard test for all modules)
quick-test:
	@echo "Running quick tests (standard only) for all modules..."
	@found=0; \
	find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
		module_name=$$(basename $$(dirname $$test_file)); \
		if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
			found=1; \
			echo "\n=== Quick testing $$module_name ==="; \
			$(MAKE) test-$$module_name || exit 1; \
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
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Generic assembly rule for NASM files
$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

# ASAN compilation rules
$(ASAN_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(filter-out $(BASE_DEBUG_CFLAGS) $(BASE_RELEASE_CFLAGS), $(CFLAGS)) $(BASE_DEBUG_CFLAGS) $(SANITIZE_FLAGS) -c $< -o $@

$(ASAN_OBJ_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@


# PERF compilation rules
$(PERF_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(filter-out $(BASE_DEBUG_CFLAGS) $(BASE_RELEASE_CFLAGS), $(CFLAGS)) $(PERF_FLAGS) -DNDEBUG -c $< -o $@

$(PERF_OBJ_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(NASM) $(filter-out -g -F dwarf, $(NASMFLAGS)) $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OUT_DIR)
	@echo "Cleaned build directory"

# Clean specific build type
clean-asan:
	rm -rf $(ASAN_OBJ_DIR) $(BIN_DIR)/test_*_asan
	@echo "Cleaned ASAN build"

clean-perf:
	rm -rf $(PERF_OBJ_DIR) $(BIN_DIR)/test_*_perf
	@echo "Cleaned PERF build"

clean-all: clean clean-asan clean-perf

# Show file lists (for debugging)
list-files:
	@echo "=== C Source Files ==="
	@echo "$(SRC_C)" | tr ' ' '\n'
	@echo ""
	@echo "=== Assembly Source Files ==="
	@echo "$(SRC_ASM)" | tr ' ' '\n'
	@echo ""
	@echo "=== Object Files (Regular) ==="
	@echo "$(OBJ)" | tr ' ' '\n'
	@echo ""
	@echo "=== Object Files (ASAN) ==="
	@echo "$(OBJ_ASAN)" | tr ' ' '\n'
	@echo ""
	@echo "=== Object Files (PERF) ==="
	@echo "$(OBJ_PERF)" | tr ' ' '\n'

# Show help
help:
	@echo "Available commands:"
	@echo "  make build          - Build in debug mode (default)"
	@echo "  make build --release - Build in release mode"
	@echo "  make debug          - Build in debug mode"
	@echo "  make release        - Build in release mode"
	@echo "  make run            - Build and run main executable"
	@echo ""
	@echo "Testing commands for modules:"
	@echo "  make test-<name>          - Build and run standard test"
	@echo "  make asan-test-<name>     - Build and run test with AddressSanitizer"
	@echo "  make perf-test-<name>     - Build and run performance-optimized test"
	@echo "  make valgrind-test-<name> - Build and run test with Valgrind"
	@echo "  make all-test-<name>      - Run all test types for a module"
	@echo "  make test-all             - Run all tests for all modules"
	@echo "  make quick-test           - Run only standard tests for all modules"
	@echo ""
	@echo "  Example: make test-arena        # Run standard arena tests"
	@echo "           make asan-test-arena   # Run arena tests with ASAN"
	@echo "           make all-test-arena    # Run all arena test types"
	@echo ""
	@echo "Utility commands:"
	@echo "  make new-component NAME=name - Create new C component"
	@echo "  make new-asm NAME=name       - Create new assembly component"
	@echo "  make new-test NAME=name      - Create new test"
	@echo "  make clean                   - Remove build artifacts"
	@echo "  make clean-asan              - Remove ASAN build artifacts"
	@echo "  make clean-perf              - Remove PERF build artifacts"
	@echo "  make clean-all               - Remove all build artifacts"
	@echo "  make list-files              - Show source files being compiled"
	@echo "  make help                    - Show this help"
	@echo ""
	@echo "Available tests:"
	@found=0; \
	find test -name "*.c" -type f 2>/dev/null | while read test_file; do \
		module_name=$$(basename $$(dirname $$test_file)); \
		if [ "$$(basename $$test_file .c)" = "$$module_name" ]; then \
			found=1; \
			echo "  test-$$module_name"; \
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
	@echo ""
	@echo "Build directories:"
	@echo "  Regular objects: $(OBJ_DIR)"
	@echo "  ASAN objects:    $(ASAN_OBJ_DIR)"
	@echo "  PERF objects:    $(PERF_OBJ_DIR)"
	@echo "  Binaries:        $(BIN_DIR)"

# -----------------------------------------------------
# Phony targets
# -----------------------------------------------------
.PHONY: all build release debug run test-% asan-test-% perf-test-% valgrind-test-% all-test-% test-all quick-test clean clean-asan clean-perf clean-all help new-component new-test new-asm list-files

# Handle --release flag
.PHONY: --release
--release:
	@# This target exists only to parse the --release flag
