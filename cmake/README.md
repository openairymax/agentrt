<!-- SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 -->
<!-- Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved. -->

# CMake — AgentRT Build-System Modules

The `agentrt/cmake/` directory hosts the CMake build-system modules of the
AgentRT management repository: 7 `.cmake` modules, 1 CMake package-config
template (`.cmake.in`), 1 test-wrapper script template (`.sh.in`) and 1
Windows pre-include header (`.h`). They provide unified compiler
configuration, platform detection, dependency discovery, sanitizer runtime
checks and colored build-time logging for the repository and its 7 leaf
repositories (atoms / commons / cupolas / daemons / gateway / heapstore /
protocols).

Requires **CMake ≥ 3.20** (matching `cmake_minimum_required` in the
repository root `CMakeLists.txt`).

## Design Goals

- **Unified management**: avoid scattering `add_compile_options` /
  `find_package` calls across individual CMakeLists.txt files
- **Cross-platform**: supports the MSVC / GCC / Clang compilers and the
  Linux / macOS / Windows platforms
- **Security first**: built-in ASan / LSan / UBSan / TSan / stack protector /
  FORTIFY_SOURCE runtime checks
- **Observability**: build output uses unified ANSI-colored formatting,
  aligned with the runtime logging system (log_write)
- **Reusable**: all logic is wrapped in CMake functions, included via
  `include()` as needed

## Current Wiring Status

As actually included/configured by the root `CMakeLists.txt`:

| Module | Used by the root build today? |
|--------|-------------------------------|
| `airy_print.cmake` | **Yes** (included directly by the root CMakeLists) |
| `airy_linkgate.cmake` | **Yes** (included directly by the root CMakeLists, see below) |
| `ctest_wrapper.sh.in` | Conditional: configured into `run_tests.sh` only when `BUILD_TESTS=ON` **and** `ENABLE_SANITIZERS=ON` and the compiler is not MSVC (`ENABLE_SANITIZERS` defaults to **OFF**) |
| `compilerflags.cmake` | No (reusable module library, `include()` as needed) |
| `platform.cmake` | No (same) |
| `dependencies.cmake` | No (same) |
| `sanitizers.cmake` | No (same) |
| `utils.cmake` | No (same) |
| `AirymaxRTConfig.cmake.in` | No (package-config template for downstream `find_package(AirymaxRT)`) |

## File Inventory

```
cmake/
├── README.md / README_zh.md    # this documentation (English / Simplified Chinese)
├── airy_print.cmake            # AgentRT unified build printing (used by the root build)
├── airy_linkgate.cmake         # build-time link-whitelist gate (used by the root build)
├── compilerflags.cmake         # compiler flags (safe options/warnings/optimization/LTO/coverage)
├── platform.cmake              # platform detection and POSIX feature macros
├── dependencies.cmake          # unified dependency discovery (required: Threads; optional: SQLite3/cJSON/...)
├── sanitizers.cmake            # sanitizer configuration (ASan/LSan/UBSan/TSan/stack protector/FORTIFY)
├── utils.cmake                 # build-time printing (superset module library of airy_print)
├── AirymaxRTConfig.cmake.in    # CMake package-config template (for find_package(AirymaxRT))
├── ctest_wrapper.sh.in         # ctest wrapper script (sets sanitizer environment variables)
└── windows_preinclude.h        # Windows MSVC pre-include header (POSIX compatibility layer)
```

## Modules

### 1. airy_print.cmake — Unified Build Printing

The printing module actually used by the root build. Build-time output uses
the unified format `[YYYY-MM-DD HH:MM:SS] [LEVEL] message`, ANSI-colored and
aligned with the runtime logging system (log_write). Colors are disabled
automatically when output is piped/redirected, and can be forced on/off via
the `AIRY_BUILD_COLOR=1/0` environment variable.

| Function | Color | Purpose |
|----------|-------|---------|
| `airy_print_ok(msg)` | green | success/confirmation |
| `airy_print_info(msg)` | blue | informational output |
| `airy_print_warn(msg)` | yellow | warning |
| `airy_print_error(msg)` | red | error (does not abort the build) |
| `airy_print_fatal(msg)` | magenta | fatal error (aborts the build) |
| `airy_print_debug(msg)` | gray | debug information |
| `airy_print_section(msg)` | cyan bold | section heading |
| `airy_print_status(msg)` | blue | drop-in replacement for `message(STATUS)` |

### 2. airy_linkgate.cmake — Build-Time Link-Whitelist Gate

Turns module link relationships into build-time assertions, working with
`tools/airy_depgraph`:

- The whitelist file is `link-whitelist.txt` at the repository root (single
  source of truth), declaring target → allowed libraries;
- `airy_linkgate_collect(TARGET_NAME WHITELIST_FILE)`: called after a target
  is defined; collects the target's actual links (`LINK_LIBRARIES`,
  filtering generator expressions and linker options) into
  `${CMAKE_BINARY_DIR}/linkgate/<target>.links.txt`;
- `airy_linkgate_install_checks()`: called by the root CMakeLists once the
  `airy_depgraph` target exists; creates a gate target per registered target
  that runs `airy_depgraph --links <whitelist> --actual <actual links>`.
  Any unauthorized link (e.g. a gateway-family target linking
  coreloopthree / cognition) fails the build (exit code 2, fail-closed).

### 3. compilerflags.cmake — Unified Compiler Flags

Manages safe compilation options, warning levels, debug symbols,
optimization levels, code coverage and LTO configuration across MSVC /
GCC / Clang.

| Function | Description |
|----------|-------------|
| `airy_apply_compiler_flags()` | Applies baseline safe options (MSVC: /W4 /GS /guard:cf; GCC/Clang: -Wall -Wextra -fstack-protector-strong) |
| `airy_apply_compliance_strict(BANNED_HEADER <path>)` | Strict compliance mode: defines `AIRY_COMPLIANCE_STRICT` and globally injects banned_functions.h (GCC/Clang only) |
| `airy_apply_build_type_flags()` | Applies build-type options (Debug: -g -O0 -fno-inline; Release: -O3, plus -flto when `ENABLE_LTO=ON`) |
| `airy_apply_coverage()` | Applies coverage options (-fprofile-arcs -ftest-coverage, GCC/Clang only) |
| `airy_apply_all_compiler_flags([BANNED_HEADER <path>])` | Applies all of the above in one call |

**Options** (defined in the repository root CMakeLists):

| Option | Default | Description |
|--------|---------|-------------|
| `WARNINGS_AS_ERRORS` | OFF | Treat compiler warnings as errors |
| `ENABLE_COVERAGE` | OFF | Enable code-coverage instrumentation |
| `ENABLE_LTO` | ON | Enable link-time optimization (Release builds) |

### 4. platform.cmake — Platform Detection and Feature Macros

Detects the target platform (Linux / macOS / Windows) and compiler
(GCC / Clang / MSVC), and sets the corresponding POSIX feature-test macros
and platform definitions.

| Function | Description |
|----------|-------------|
| `airy_detect_platform()` | Detects the platform and sets `AIRY_PLATFORM_LINUX/MACOS/WINDOWS` and `AIRY_COMPILER_GCC/CLANG/MSVC` |
| `airy_print_platform_info()` | Prints a platform summary |
| `airy_is_unix_like(result_var)` | Checks whether the system is Unix-like |
| `airy_supports_sanitizers(result_var)` | Checks whether the compiler supports sanitizers |

**POSIX feature macros**:

| Platform | Macros |
|----------|--------|
| Linux | `_POSIX_C_SOURCE=200809L` `_XOPEN_SOURCE=700` `_GNU_SOURCE` |
| macOS | `_POSIX_C_SOURCE=200112L` `_DARWIN_C_SOURCE` |

### 5. dependencies.cmake — Unified Dependency Discovery

Centralizes discovery of required/optional system dependencies and sets the
corresponding `AIRY_HAS_*` compile definitions.

| Function | Description |
|----------|-------------|
| `airy_find_required_deps()` | Finds required dependencies (currently Threads only) |
| `airy_find_optional_deps()` | Finds optional dependencies (see table below) |
| `airy_find_all_deps()` | Finds all dependencies in one call |
| `airy_print_deps_summary()` | Prints a dependency-discovery summary |

**Dependencies discovered**:

| Dependency | Macro | Purpose |
|------------|-------|---------|
| Threads | — | multithreading (required) |
| SQLite3 | `AIRY_HAS_SQLITE3` | embedded database |
| cJSON | `AIRY_HAS_CJSON` | JSON parsing |
| libyaml | `AIRY_HAS_YAML` | YAML config parsing |
| OpenSSL | `AIRY_HAS_OPENSSL` | TLS/crypto |
| libcurl | `AIRY_HAS_CURL` | HTTP client |
| libmicrohttpd | `AIRY_HAS_MICROHTTPD` | embedded HTTP server |
| libwebsockets | `AIRY_HAS_LIBWEBSOCKETS` | WebSocket |
| libevent | `AIRY_HAS_LIBEVENT` | event loop |
| FAISS | `AIRY_HAS_FAISS` | vector retrieval (MemoryRovol component) |

### 6. sanitizers.cmake — Sanitizer Configuration

Manages sanitizer and hardening options. Whether they are applied is
controlled by the repository-root option `ENABLE_SANITIZERS`
(default **OFF**); when enabled, the sub-switches default as follows.

| Function | Description |
|----------|-------------|
| `airy_check_sanitizer_support()` | Checks whether the platform supports sanitizers |
| `airy_enable_asan(target scope)` | Enables AddressSanitizer + LeakSanitizer |
| `airy_enable_ubsan(target scope)` | Enables UndefinedBehaviorSanitizer |
| `airy_enable_tsan(target scope)` | Enables ThreadSanitizer (mutually exclusive with ASan) |
| `airy_enable_stack_protector(target scope)` | Enables the stack protector (-fstack-protector-strong) |
| `airy_enable_fortify(target scope)` | Enables FORTIFY_SOURCE=2 |
| `enable_airy_sanitizers(target)` | Enables all security checks in one call |
| `airy_print_sanitizer_summary()` | Prints a sanitizer-configuration summary |

**Options**:

| Option | Default | Description |
|--------|---------|-------------|
| `AIRY_ENABLE_ASAN` | ON | AddressSanitizer + LeakSanitizer |
| `AIRY_ENABLE_UBSAN` | ON | UndefinedBehaviorSanitizer |
| `AIRY_ENABLE_TSAN` | OFF | ThreadSanitizer (mutually exclusive with ASan) |
| `AIRY_ENABLE_STACK_PROTECTOR` | ON | Stack protector |
| `AIRY_ENABLE_FORTIFY` | ON | FORTIFY_SOURCE=2 |

### 7. utils.cmake — Build-Time Printing (Superset Module Library)

Contains all 8 printing functions of `airy_print.cmake` (identical color
control behavior), plus:

| Function | Purpose |
|----------|---------|
| `airy_print_verbose(msg)` | Conditional output (printed only when the environment variable `AIRY_VERBOSE=1` is set) |
| `airy_print_build_summary()` | Prints a build-environment summary |

### 8. AirymaxRTConfig.cmake.in — Package-Config Template

A CMake package-config template for downstream projects calling
`find_package(AirymaxRT CONFIG)`: exposes the AirymaxRT include path
(headers installed under `<prefix>/include/agentrt`) and version
information.

### 9. ctest_wrapper.sh.in — Test-Wrapper Script Template

Configured as `run_tests.sh`: sets `ASAN_OPTIONS` / `LSAN_OPTIONS` /
`UBSAN_OPTIONS` automatically and then executes `ctest "$@"`, so sanitizer
settings take effect during testing. Generated by the root CMakeLists only
when `BUILD_TESTS` and `ENABLE_SANITIZERS` (default OFF) are on and the
compiler is not MSVC.

### 10. windows_preinclude.h — Windows MSVC Pre-Include Header

Provides a POSIX compatibility layer for the MSVC compiler on Windows:

- `__builtin_*` functions mapped to standard C functions
  (`__builtin_memcpy` → `memcpy`)
- `ssize_t` / `pid_t` type definitions
- POSIX function mappings such as `PATH_MAX` / `strcasecmp` / `strdup` /
  `strtok_r`
- Atomic operation constants (`__ATOMIC_RELAXED` etc.)
- cJSON stub functions (when `AIRY_HAS_CJSON` is undefined)

## Usage

The root build already includes `airy_print.cmake` and
`airy_linkgate.cmake` automatically. The remaining modules are an opt-in
module library; in a CMakeLists.txt:

```cmake
# Include the modules you need (paths relative to the agentrt repository root)
include(${CMAKE_SOURCE_DIR}/cmake/platform.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/compilerflags.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/dependencies.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

# Use them
airy_detect_platform()
airy_apply_all_compiler_flags()
airy_find_all_deps()

add_executable(my_target main.c)
enable_airy_sanitizers(my_target)

airy_print_section("Build completed")
airy_print_ok("my_target configured")
```

All `.cmake` modules use `include_guard(GLOBAL)` and are safe to include
repeatedly.

## Upstream Dependencies

None — this directory is the lowest-level infrastructure of the repository
and depends on no other Airymax module. It only requires CMake ≥ 3.20;
optional dependency discovery in `dependencies.cmake` additionally needs the
system `pkg-config`.

## License

Copyright (c) 2025-2026 SPHARX Ltd. All Rights Reserved.

Dual-licensed: **AGPL-3.0-or-later OR Apache-2.0** (SPDX:
`AGPL-3.0-or-later OR Apache-2.0`). See [LICENSE](../LICENSE).
