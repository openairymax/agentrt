# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Atoms Module Security Scan Configuration
# Version: 1.0.0
#
# 本配置文件定义atoms模块的安全扫描策略和规则

# ==================== 静态分析配置 ====================

# Cppcheck配置
set(CPPCHECK_ENABLED ON CACHE BOOL "Enable cppcheck analysis")
set(CPPCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/.cppcheck-suppressions" CACHE FILEPATH "Cppcheck suppressions file")
set(CPPCHECK_CHECKERS
    "warning"
    "performance"
    "portability"
    "style"
    "unusedFunction"
    "missingInclude"
)

# Clang-Tidy配置
set(CLANG_TIDY_ENABLED ON CACHE BOOL "Enable clang-tidy analysis")
set(CLANG_TIDY_CHECKS
    "bugprone-*"
    "cert-*"
    "cppcoreguidelines-*"
    "clang-analyzer-*"
    "modernize-*"
    "performance-*"
    "portability-*"
    "readability-*"
    "-modernize-use-trailing-return-type"
    "-readability-magic-numbers"
)

# ==================== 安全规则配置 ====================

# 禁止的函数列表（不安全函数）
set(FORBIDDEN_FUNCTIONS
    # 字符串函数（无边界检查）
    "strcpy"      # 使用 strncpy 或 strlcpy 替代
    "strcat"      # 使用 strncat 或 strlcat 替代
    "sprintf"     # 使用 snprintf 替代
    "vsprintf"    # 使用 vsnprintf 替代
    "gets"        # 使用 fgets 替代
    
    # 格式化函数（格式字符串漏洞）
    "printf"      # 使用带格式检查的版本
    "fprintf"     # 使用带格式检查的版本
    "vprintf"     # 使用带格式检查的版本
    "vfprintf"    # 使用带格式检查的版本
    
    # 内存函数
    "malloc"      # 建议使用 calloc 或检查返回值
    "realloc"     # 建议检查返回值
    "free"        # 建议置空指针
    
    # 随机数函数（不安全）
    "rand"        # 使用 arc4random 或 getrandom 替代
    "srand"       # 使用更安全的随机种子
    
    # 时间函数（不安全）
    "ctime"       # 使用 strftime 替代
    "asctime"     # 使用 strftime 替代
    "gmtime"      # 使用 gmtime_r 替代
    "localtime"   # 使用 localtime_r 替代
    
    # 环境函数
    "getenv"      # 检查返回值
    "setenv"      # 检查返回值
    "putenv"      # 避免使用
    
    # 执行函数
    "system"      # 使用 exec 系列替代
    "popen"       # 使用更安全的替代方案
)

# 危险模式检测
set(DANGEROUS_PATTERNS
    # 硬编码密码
    "password\\s*="
    "passwd\\s*="
    "secret\\s*="
    "api_key\\s*="
    "private_key\\s*="
    
    # 硬编码IP
    "\\b(?:[0-9]{1,3}\\.){3}[0-9]{1,3}\\b"
    
    # SQL注入模式
    "SELECT.*FROM.*WHERE.*\\+"
    "INSERT.*INTO.*VALUES.*\\+"
    "UPDATE.*SET.*WHERE.*\\+"
    "DELETE.*FROM.*WHERE.*\\+"
    
    # 命令注入模式
    "system\\s*\\(.*\\+"
    "popen\\s*\\(.*\\+"
    "exec.*\\(.*\\+"
    
    # 路径遍历模式
    "\\.\\./"
    "\\.\\\\"
)

# ==================== 安全扫描目标 ====================

# 创建静态分析目标
if(CPPCHECK_ENABLED)
    find_program(CPPCHECK_EXECUTABLE cppcheck)
    if(CPPCHECK_EXECUTABLE)
        # 构建cppcheck参数
        set(CPPCHECK_ARGS
            --enable=${CPPCHECK_CHECKERS}
            --std=c11
            --platform=unix64
            --force
            --inline-suppr
            --quiet
            --error-exitcode=1
        )
        
        if(EXISTS ${CPPCHECK_SUPPRESSIONS_FILE})
            list(APPEND CPPCHECK_ARGS --suppressions-list=${CPPCHECK_SUPPRESSIONS_FILE})
        endif()
        
        # 添加静态分析目标
        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE}
                ${CPPCHECK_ARGS}
                -j ${PROCESSOR_COUNT}
                --xml --xml-version=2
                --output-file=${CMAKE_BINARY_DIR}/cppcheck_report.xml
                ${CMAKE_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running cppcheck static analysis..."
        )
        
        # 添加到构建依赖
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_dependencies(${PROJECT_NAME} cppcheck)
        endif()
    endif()
endif()

# Clang-Tidy集成
if(CLANG_TIDY_ENABLED)
    find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
    if(CLANG_TIDY_EXECUTABLE)
        # 设置clang-tidy检查
        string(REPLACE ";" "," CLANG_TIDY_CHECKS_STR "${CLANG_TIDY_CHECKS}")
        
        # 创建clang-tidy目标
        add_custom_target(clang-tidy
            COMMAND ${CLANG_TIDY_EXECUTABLE}
                -checks=${CLANG_TIDY_CHECKS_STR}
                -p ${CMAKE_BINARY_DIR}
                --export-fixes=${CMAKE_BINARY_DIR}/clang-tidy-fixes.yaml
                ${CMAKE_SOURCE_DIR}/**/*.c
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running clang-tidy analysis..."
        )
        
        # 设置编译时的clang-tidy检查
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CMAKE_C_CLANG_TIDY
                "${CLANG_TIDY_EXECUTABLE};-checks=${CLANG_TIDY_CHECKS_STR};--warnings-as-errors=*"
            )
        endif()
    endif()
endif()

# ==================== 安全扫描脚本 ====================

# 创建安全扫描目标
find_program(PYTHON_EXECUTABLE python3 python)
if(PYTHON_EXECUTABLE)
    add_custom_target(security-scan
        COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/security_scan.py
            --source ${CMAKE_SOURCE_DIR}
            --output ${CMAKE_BINARY_DIR}/security_report.json
            --forbidden-functions "${FORBIDDEN_FUNCTIONS}"
            --dangerous-patterns "${DANGEROUS_PATTERNS}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running security scan..."
    )
endif()

# ==================== 编译时安全选项 ====================

# GCC/Clang安全编译选项
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        # 栈保护
        -fstack-protector-strong
        
        # 地址空间布局随机化
        -fPIE
        
        # 位置无关代码
        -fPIC
        
        # RELRO (Relocation Read-Only)
        -Wl,-z,relro,-z,now
        
        # 不允许执行栈
        -Wl,-z,noexecstack
        
        # 警告作为错误
        -Werror=return-type
        -Werror=implicit-function-declaration
        
        # 格式字符串检查
        -Wformat=2
        -Werror=format-security
        
        # 未初始化变量检查
        -Wuninitialized
        -Werror=uninitialized
    )
    
    # 调试模式下添加额外检查
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(
            # 运行时边界检查
            -D_FORTIFY_SOURCE=2
            
            # 栈检查
            -fstack-check
        )
    endif()
endif()

# MSVC安全编译选项
if(MSVC)
    add_compile_options(
        # 运行时错误检查
        /RTC1
        
        # 安全异常处理
        /sdl
        
        # 缓冲区安全检查
        /GS
        
        # 数据执行保护
        /NXCOMPAT
        
        # 地址空间布局随机化
        /DYNAMICBASE
        
        # 控制流保护
        /guard:cf
        
        # 警告级别
        /W4
        
        # 警告作为错误
        /WX
    )
    
    # 定义安全宏
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
        _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
    )
endif()

# ==================== 安全报告生成 ====================

# 创建综合安全报告
add_custom_target(security-report
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/security
    COMMAND ${CMAKE_COMMAND} -E echo "Generating security report..."
    COMMAND ${CMAKE_COMMAND} -E echo "# Security Scan Report" > ${CMAKE_BINARY_DIR}/security/report.md
    COMMAND ${CMAKE_COMMAND} -E echo "" >> ${CMAKE_BINARY_DIR}/security/report.md
    COMMAND ${CMAKE_COMMAND} -E echo "Date: $(date)" >> ${CMAKE_BINARY_DIR}/security/report.md
    COMMAND ${CMAKE_COMMAND} -E echo "" >> ${CMAKE_BINARY_DIR}/security/report.md
    COMMAND ${CMAKE_COMMAND} -E echo "## Static Analysis Results" >> ${CMAKE_BINARY_DIR}/security/report.md
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating security report..."
)

# ==================== 安全门禁 ====================

# 创建安全门禁检查
add_custom_target(security-gate
    DEPENDS cppcheck security-scan
    COMMAND ${PYTHON_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/scripts/check_security_gate.py
        --cppcheck ${CMAKE_BINARY_DIR}/cppcheck_report.xml
        --security ${CMAKE_BINARY_DIR}/security_report.json
        --max-errors 0
        --max-warnings 10
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Checking security gates..."
)

# ==================== 消息输出 ====================

message(STATUS "=== Security Configuration ===")
message(STATUS "Cppcheck: ${CPPCHECK_ENABLED}")
message(STATUS "Clang-Tidy: ${CLANG_TIDY_ENABLED}")
message(STATUS "Forbidden functions check: ${#FORBIDDEN_FUNCTIONS} functions")
message(STATUS "Dangerous patterns check: ${#DANGEROUS_PATTERNS} patterns")
