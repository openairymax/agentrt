# AgentOS C Language Coding Standard v1.1.1

Version: 1.1.1  
Release date: 2026-03-21  
Scope: All C-language modules in SpharxTeam/AgentOS (atoms/, domes/, dynamic/, backs/, tools/, etc.)

1. Core Philosophy (At a Glance)
- Engineering Cybernetics: Design feedback loops via error codes, logs, health checks and metrics so systems can observe and respond to anomalies.  
- Systems Engineering: Modular, interface-driven design with clear boundaries and replacement-friendly abstractions.  
- Dual-System Thinking: Provide System 1 (fast, low-latency) and System 2 (safe, thorough) paths and runtime selection mechanisms.  
- Minimalist Aesthetics: Clear interfaces, meaningful names, concise and purposeful comments that explain "why", not "what".

2. Scope & Terminology
- agentos_error_t: unified error-code type (int32_t); provide agentos_strerror() for human-readable messages.  
- Public headers: module/include/ — stable API only.  
- Private implementation: module/src/ or module/src/internal/.  
- Traces & spans: follow OpenTelemetry and W3C Trace Context (traceparent) formats.  
- SBOM: Software Bill of Materials (SPDX or CycloneDX) produced by CI and attached to releases.

3. Directory Template (Mandatory)
Every module MUST contain exactly these five top-level items:
- include/           — stable public headers (exported API)
- src/               — implementation and optional src/internal/
- tests/             — unit and integration tests
- CMakeLists.txt     — build, export and install rules
- README.md          — module summary, API compatibility level, configuration and quick examples

Notes:
- Only symbols required by consumers belong in include/. Implementation details remain in src/.
- README.md must list module API version (MAJOR.MINOR), supported platforms, and a quick start snippet.
- tests/ must include at least one unit test and one repeatable integration test suitable for CI.

4. Naming & Visibility
- Naming: snake_case for functions and variables; UPPER_CASE for macros and constants; types end with _t.  
- Public names must include the module prefix (e.g., atoms_, domes_, cognition_, llm_).  
- Private cross-file symbols: use `_module_` prefix or make static and place in internal headers.  
- Build with `-fvisibility=hidden` and explicitly export public symbols.

5. API and ABI Management
- Public headers must declare an API version macro at the top, e.g. `#define MODULE_API_VERSION 1`.  
- Maintain ABI compatibility within the same MAJOR version; breaking changes increment MAJOR and publish migration notes.  
- Exported types should use opaque pointers where possible; avoid exposing internal structure fields.  
- Consider symbol versioning (ELF) or an explicit export list to manage symbol evolution.

6. Function Design & Documentation
- Single responsibility and short functions (≤50 lines recommended). Functions >80 lines must be refactored and covered by tests.  
- Maximum of 5 parameters; otherwise wrap parameters into a struct.  
- Public APIs MUST be documented with Doxygen-style comments, including owner semantics (who frees), thread-safety, and error behavior.  
- Return conventions: use agentos_error_t (int32_t) or pointer returns (NULL indicates failure). Provide common error definitions and helpers.

Example:

```c
/**
 * @brief Submit a plan and return a task id.
 * @param engine [in] Engine handle (non-NULL).
 * @param plan   [in] Plan to submit (read-only).
 * @param out_id [out] Caller-allocated pointer to receive task id string (caller must free).
 * @return AGENTOS_OK (0) on success; otherwise an error code.
 */
int cognition_schedule(cognition_engine_t* engine,
                       const task_plan_t* plan,
                       char** out_id);
```

7. Error Handling & Diagnostics
- Use a unified error type agentos_error_t and provide agentos_strerror(agentos_error_t).  
- Error names should carry the module prefix (e.g., COG_ERR_OUT_OF_MEMORY).  
- Every error path must log at WARN/ERROR level and include trace_id and relevant non-sensitive context.  
- Support fault-injection hooks in test builds to validate error handling and recovery strategies.

8. Resource & Lifetime Management
- All resources must follow create/destroy pairing and clearly document ownership.  
- Prefer scope-guard patterns (cleanup attribute or macros) to reduce manual cleanup errors.  
- Use temporary pointers with realloc to avoid losing the original on failure:

```c
void* tmp = realloc(ptr, new_size);
if (!tmp) {
    // handle allocation failure; ptr remains valid
}
ptr = tmp;
```

- Long-running services should include memory quotas and reclaim strategies; expose related metrics in health checks.

9. Concurrency & Thread Safety
- Prefer C11 atomics (stdatomic.h). Avoid legacy __sync_* where possible.  
- Design hot paths lock-free where feasible and provide a locked fallback for safety (System2).  
- Document thread-safety contracts for each API. State whether the API is thread-safe, reentrant, or requires external synchronization.  
- Avoid lock nesting; if necessary, establish and document a global lock order. Use thread-local storage for per-request context (trace_id, request_id).

10. Logging, Tracing & Metrics (Observability)
- Logs must be structured JSON and compatible with OpenTelemetry logs. Minimum fields: timestamp, level, module, function, trace_id, request_id, message, err_code.  
- Use W3C Trace Context for traces (traceparent) and propagate trace_id across components.  
- Metrics: expose Prometheus-compatible metrics via an endpoint or exporter. Modules should provide /metrics and /healthz (liveness) and /readyz (readiness).  
- Implement sampling or rate-limiting for high-throughput logs while keeping error logs un-sampled.

11. Configuration & Hot Reload
- Config precedence: defaults < config file < environment variables < runtime overrides.  
- Validate config files with YAML/JSON schema at load time; fail fast on invalid config.  
- Hot reload must replace configuration atomically (swap pointers or use read/write locks) and ensure short-term backward compatibility. Keep sensitive values out of disk-based config files; inject via secret managers.

12. Security Practices & Runtime Hardening
- Recommended compile-time hardening flags (release builds):  
  - -fstack-protector-strong  
  - -D_FORTIFY_SOURCE=2  
  - -fPIE -pie  
  - -Wl,-z,relro -Wl,-z,now  
  - -fvisibility=hidden  
  - LTO when appropriate  
- Run sanitizers in CI: ASAN, UBSAN, TSAN, MSAN (selected by job matrix).  
- Integrate fuzzing (libFuzzer/AFL++) for parsers and external-input surfaces with continuous runs for critical targets.  
- Supply-chain protections: CI must generate SBOMs (SPDX/CycloneDX), run dependency vulnerability scans, and sign artifacts with Sigstore/Cosign on release.  
- Apply the principle of least privilege at runtime: drop capabilities, use seccomp/AppArmor where appropriate.  
- Do not log secrets; use redaction helpers and central secret managers for sensitive data.

13. Build, Static/Dynamic Analysis & CI
- Minimal CI pipeline: format check (clang-format), build (CMake), static analysis (clang-tidy, cppcheck), unit tests with coverage, sanitizers, fuzz jobs, dependency and license checks, SBOM generation, artifact signing and publish.  
- Pre-commit hooks: clang-format, license header checks, shellcheck for scripts, commit-msg validation (Conventional Commits).  
- CI should fail on high-severity security findings and on format/static-analysis regressions.

14. Testing Strategy
- Unit tests: CMocka, Criterion, or equivalent; exercise success, failure and boundary paths.  
- Integration tests: containerized harnesses or isolated test environments that mock external dependencies.  
- Fuzzing: use libFuzzer for parsers, deserializers and protocol handlers; keep corpus and regression tests.  
- Performance benchmarks: microbenchmarks for critical paths with regression alerts.  
- Coverage goals: critical modules aim for ≥85%; CI can enforce thresholds or at least warn.

15. Release & Supply-Chain Compliance
- Use semantic versioning (SemVer). Every release must include changelog, SBOM and signature.  
- Block releases if dependency vulnerabilities exceed the configured severity threshold.  
- Produce reproducible build metadata (build logs, compiler flags, SBOM, signed artifact) with each released binary.

16. Development Process & Code Review
- Commit/PRs follow Conventional Commits. PRs must include tests and update documentation as needed.  
- CODEOWNERS governs reviewers for critical modules; require at least one reviewer, two for core modules.  
- Review checklist: API/ABI impact, tests added/updated, static-analysis/sanitizer results, SBOM/third-party licensing impact, and security considerations.

Appendix A — Recommended Compiler Flags (CMake example)

```cmake
# Release build flags (recommended)
target_compile_options(${target} PRIVATE
  -Wall -Wextra -Werror
  -Wformat -Wformat-security
  -fstack-protector-strong
  -D_FORTIFY_SOURCE=2
  -fPIE
  -fno-omit-frame-pointer
)
target_link_options(${target} PRIVATE
  -Wl,-z,relro -Wl,-z,now -pie
)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON) # enable LTO when appropriate
```

Appendix B — Example Code Snippets (concise & idiomatic)

Resource allocation and cleanup:

```c
service_t* service_create(const char* path) {
    if (!path) return NULL;
    service_t* svc = calloc(1, sizeof(*svc));
    if (!svc) return NULL;
    svc->file = fopen(path, "rb");
    if (!svc->file) goto fail;
    // ... initialization ...
    return svc;
fail:
    service_destroy(svc);
    return NULL;
}
```

Simplified lock-free ring buffer skeleton:

```c
typedef struct ring_buffer {
    void** buffer;
    size_t mask; // capacity = mask + 1, power of two
    atomic_size_t head;
    atomic_size_t tail;
} ring_buffer_t;
```

Health check example:

```c
char* module_health_check(void) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "healthy");
    cJSON_AddNumberToObject(root, "requests_total",
                           (double)atomic_load(&requests_total));
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json; // caller must free
}
```

Appendix C — Common Error Codes (example; centralize and expand)

```c
#define AGENTOS_OK           0
#define AGENTOS_EINVAL      -22
#define AGENTOS_ENOMEM      -12
#define AGENTOS_EBUSY       -16
#define AGENTOS_ETIMEDOUT  -110
```

Copyright
© 2026 SPHARX Ltd. All Rights Reserved.