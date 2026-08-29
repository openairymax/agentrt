# airy_depgraph —— commons 子域依赖图校验工具（0.1.6 P1-2）
#
# 构建期门禁：commons/deps.txt 声明子域依赖，本工具复用 taskflow
# graph_engine 构建 DAG，Kahn 拓扑排序 + 环检测（fail-closed）：
#   退出码 0 = 无环；1 = 有环（报环路径）；2 = manifest 错误。

add_executable(airy_depgraph
    main.c
)

target_compile_features(airy_depgraph PRIVATE c_std_99)

if(TARGET airy_taskflow AND TARGET airy_common)
    target_link_libraries(airy_depgraph PRIVATE
        airy_taskflow
        airy_common
    )
endif()

if(Threads_FOUND)
    target_link_libraries(airy_depgraph PRIVATE Threads::Threads)
endif()
if(NOT MSVC)
    target_link_libraries(airy_depgraph PRIVATE m)
endif()

# 构建期门禁：ALL 依赖本目标，commons/deps.txt 有环即构建失败
add_custom_target(depgraph-check
    COMMAND airy_depgraph
            -o "${CMAKE_BINARY_DIR}/depgraph-report.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../commons/deps.txt"
    DEPENDS airy_depgraph
    COMMENT "depgraph: commons 子域依赖图环检测（0.1.6 P1-2）"
    USES_TERMINAL
)
add_dependencies(depgraph-check airy_depgraph)

# 环检测用例（BUILD_TESTS 时注册 ctest）
if(BUILD_TESTS)
    add_test(NAME depgraph-commons-acyclic
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/../../commons/deps.txt")
    add_test(NAME depgraph-fixture-acyclic
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/tests/acyclic.txt")
    add_test(NAME depgraph-fixture-cyclic
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/tests/cyclic.txt")
    set_tests_properties(depgraph-fixture-cyclic PROPERTIES WILL_FAIL TRUE)
    add_test(NAME depgraph-fixture-unknown-dep
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/tests/unknown-dep.txt")
    set_tests_properties(depgraph-fixture-unknown-dep PROPERTIES WILL_FAIL TRUE)
endif()
