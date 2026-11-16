# Build, test and check targets. The CI workflows only call these targets, so
# every check that runs on GitHub also runs on a laptop with `make <target>`.
#
#   make build          compile the solver from src/            -> build/solver
#   make amalgamate     write the single contest file           -> build/solver.cpp
#   make test           run the Rust test suite against the build
#   make equivalence    prove src/ is the submitted program
#   make contest-check  hash, size and encoding of the submitted file
#   make format-check   clang-format and rustfmt, no changes written
#   make lint           clippy with warnings as errors
#   make sanitize       AddressSanitizer + UBSan build, then the solver tests
#   make ci             everything above except sanitize

CXX           ?= g++
CARGO         ?= cargo
CLANG_FORMAT  ?= clang-format
OPT           ?= -O2
# Warnings stay visible but only fail the build when WERROR=1. Two sign-compare
# warnings come from the submitted code and cannot be fixed without changing
# it, so that one warning class is switched off.
WARN          := -Wall -Wextra -Wno-sign-compare
ifeq ($(WERROR),1)
WARN += -Werror
endif
ifeq ($(STATIC),1)
LDFLAGS += -static
endif

BUILD         := build
EXE           := $(if $(filter Windows_NT,$(OS)),.exe,)
SOLVER        := $(BUILD)/solver$(EXE)
TOOLS_DIR     := tools
TOOLS_BIN     := $(TOOLS_DIR)/target/release
SOURCES       := $(wildcard src/*.hpp) src/main.cpp
CONTEST_FILE  := contest/SOICT2026_Round2_S4T2A8_SISR_v3_pkasg.cpp
SOURCE_LIMIT  := 500000

.PHONY: all build tools amalgamate test equivalence contest-check format format-check lint \
        sanitize ci clean

all: build

build: $(SOLVER)

$(SOLVER): $(SOURCES)
	@mkdir -p $(BUILD)
	$(CXX) -std=c++17 $(OPT) $(WARN) -Isrc src/main.cpp -o $@ $(LDFLAGS)

tools:
	$(CARGO) build --release --manifest-path $(TOOLS_DIR)/Cargo.toml

amalgamate: tools
	$(TOOLS_BIN)/amalgamate --entry src/main.cpp --output $(BUILD)/solver.cpp

test: build
	SOLVER=$(abspath $(SOLVER)) CXX=$(CXX) $(CARGO) test --manifest-path $(TOOLS_DIR)/Cargo.toml

equivalence: tools amalgamate
	$(TOOLS_BIN)/check_equivalence --cxx $(CXX) --reference $(CONTEST_FILE) --source src/main.cpp $(if $(ASM),--asm)
	$(TOOLS_BIN)/check_equivalence --cxx $(CXX) --reference $(CONTEST_FILE) --source $(BUILD)/solver.cpp $(if $(ASM),--asm)

# The contest takes one file of at most 500,000 characters. The submitted file
# must stay byte for byte as it was uploaded, and it must be plain LF text.
contest-check: amalgamate
	cd contest && sha256sum -c SHA256SUMS
	@chars=$$(wc -m < $(CONTEST_FILE)); test $$chars -le $(SOURCE_LIMIT) \
	  || { echo "contest file has $$chars characters, limit $(SOURCE_LIMIT)"; exit 1; }
	@if grep -q "$$(printf '\r')" $(CONTEST_FILE); then echo "contest file contains CR"; exit 1; fi
	@echo "contest file: $$(wc -m < $(CONTEST_FILE)) characters, LF only, hash matches"
	@chars=$$(wc -m < $(BUILD)/solver.cpp); test $$chars -le $(SOURCE_LIMIT) \
	  || { echo "amalgamated file has $$chars characters, limit $(SOURCE_LIMIT)"; exit 1; }
	@echo "amalgamated file: $$(wc -m < $(BUILD)/solver.cpp) characters"

format:
	"$(CLANG_FORMAT)" -i --style=file $(SOURCES)
	$(CARGO) fmt --manifest-path $(TOOLS_DIR)/Cargo.toml

format-check:
	"$(CLANG_FORMAT)" --dry-run --Werror --style=file $(SOURCES)
	$(CARGO) fmt --manifest-path $(TOOLS_DIR)/Cargo.toml --check

lint:
	$(CARGO) clippy --manifest-path $(TOOLS_DIR)/Cargo.toml --all-targets -- -D warnings

# Sanitizer runs are slower, so the tests skip their wall-clock assertions.
# Undefined behaviour is fatal: the build adds -fno-sanitize-recover.
SAN_SOLVER := $(BUILD)/solver-asan$(EXE)

sanitize: $(SOURCES)
	@mkdir -p $(BUILD)
	$(CXX) -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
	  -fno-sanitize-recover=undefined -Isrc src/main.cpp -o $(SAN_SOLVER)
	SKIP_TIMING=1 SOLVER=$(abspath $(SAN_SOLVER)) CXX=$(CXX) \
	  $(CARGO) test --manifest-path $(TOOLS_DIR)/Cargo.toml --test solver

ci: format-check lint build test equivalence contest-check

clean:
	$(RM) -r $(BUILD) $(TOOLS_DIR)/target
