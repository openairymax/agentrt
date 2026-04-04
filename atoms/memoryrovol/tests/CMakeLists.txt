# memoryrovol 单元测试
cmake_minimum_required(VERSION 3.15)

# 添加测试可执行文件
add_executable(memoryrovol_tests
    test_main.c
    test_advanced_storage.c
    test_layer2_feature.c
    test_layer3_structure.c
    test_layer4_pattern.c
)

# 链接核心库
target_link_libraries(memoryrovol_tests
    agentos_memoryrovol
    agentos_core
)

# 包含头文件路径
target_include_directories(memoryrovol_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../coreloopthree/include
)

# 安全编译选项
if(MSVC)
    target_compile_options(memoryrovol_tests PRIVATE /W4 /WX)
    target_compile_definitions(memoryrovol_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(memoryrovol_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(memoryrovol_tests PRIVATE -fstack-protector-strong)
    target_compile_options(memoryrovol_tests PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(memoryrovol_tests PRIVATE -fPIE)
endif()

# 添加测试
add_test(NAME memoryrovol_storage_tests COMMAND memoryrovol_tests)