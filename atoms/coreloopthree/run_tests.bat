@echo off
REM 测试运行脚本
REM 版权所有 (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."

echo ========================================
echo AgentOS CoreLoopThree 模块测试脚本
echo ========================================
echo.

echo 1. 检查测试文件...
if not exist "tests\test_cognition.c" (
    echo 错误: 找不到测试文件 tests\test_cognition.c
    exit /b 1
)

if not exist "tests\test_execution.c" (
    echo 错误: 找不到测试文件 tests\test_execution.c
    exit /b 1
)

echo ✓ 测试文件检查通过
echo.

echo 2. 注意: 由于编译环境依赖，实际测试需要以下步骤:
echo    - 安装 CMake 3.20+
echo    - 安装 C 编译器 (GCC/Clang/MSVC)
echo    - 配置项目依赖库
echo    - 运行 cmake 构建
echo    - 编译并运行测试
echo.

echo 3. 测试覆盖范围:
echo    - 认知引擎创建和销毁
echo    - 认知引擎处理流程
echo    - 错误处理机制
echo    - 执行引擎创建和销毁
echo    - 任务提交和查询
echo    - 任务等待和结果获取
echo    - 并发任务处理
echo.

echo 4. 手动测试步骤:
echo    a) 创建构建目录: mkdir build && cd build
echo    b) 配置项目: cmake ..
echo    c) 构建项目: cmake --build .
echo    d) 运行测试: ctest 或直接运行测试可执行文件
echo.

echo 5. 测试预期结果:
echo    - 所有测试用例通过
echo    - 无内存泄漏
echo    - 无运行时错误
echo    - 功能符合预期
echo.

echo ========================================
echo 测试脚本完成
echo ========================================
pause