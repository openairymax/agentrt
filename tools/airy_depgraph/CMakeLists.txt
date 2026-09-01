# airy_depgraph —— commons 子域依赖图校验工具（0.1.6 P1-2 + P1-1 原子化门禁）
#
# 构建期门禁：commons/deps.txt 声明子域依赖，本工具复用 taskflow
# graph_engine 构建 DAG，Kahn 拓扑排序 + 环检测（fail-closed）：
#   退出码 0 = 无环；1 = 有环（报环路径）；2 = manifest 错误 / include 漂移。
# include 漂移门禁（--root commons 根）：公共头跨子域引用必须已声明，
# 否则构建失败；声明的依赖在公共头中零实际 include 时输出陈旧边告警。

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

# 构建期门禁：ALL 依赖本目标；commons/deps.txt 有环或公共头跨子域
# 引用未声明（include 漂移）即构建失败
add_custom_target(depgraph-check
    COMMAND airy_depgraph
            -o "${CMAKE_BINARY_DIR}/depgraph-report.txt"
            --root "${CMAKE_CURRENT_SOURCE_DIR}/../../commons"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../commons/deps.txt"
    DEPENDS airy_depgraph
    COMMENT "depgraph: commons 依赖图环检测 + include 漂移门禁（0.1.6 P1-2/P1-1）"
    USES_TERMINAL
)
add_dependencies(depgraph-check airy_depgraph)

# 环/漂移检测用例（BUILD_TESTS 时注册 ctest）
if(BUILD_TESTS)
    add_test(NAME depgraph-commons-acyclic
             COMMAND airy_depgraph
                     --root "${CMAKE_CURRENT_SOURCE_DIR}/../../commons"
                     "${CMAKE_CURRENT_SOURCE_DIR}/../../commons/deps.txt")
    add_test(NAME depgraph-fixture-acyclic
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/tests/acyclic.txt")
    add_test(NAME depgraph-fixture-cyclic
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/tests/cyclic.txt")
    set_tests_properties(depgraph-fixture-cyclic PROPERTIES WILL_FAIL TRUE)
    add_test(NAME depgraph-fixture-unknown-dep
             COMMAND airy_depgraph "${CMAKE_CURRENT_SOURCE_DIR}/tests/unknown-dep.txt")
    set_tests_properties(depgraph-fixture-unknown-dep PROPERTIES WILL_FAIL TRUE)
    add_test(NAME depgraph-fixture-drift-ok
             COMMAND airy_depgraph
                     --root "${CMAKE_CURRENT_SOURCE_DIR}/tests/drift-root"
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/drift-declared.txt")
    add_test(NAME depgraph-fixture-drift-undeclared
             COMMAND airy_depgraph
                     --root "${CMAKE_CURRENT_SOURCE_DIR}/tests/drift-root"
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/drift-undeclared.txt")
    set_tests_properties(depgraph-fixture-drift-undeclared PROPERTIES WILL_FAIL TRUE)
    # M1-1b 链接白名单门禁（fixture：合规通过 / 故意加链 fail-closed）
    add_test(NAME depgraph-links-ok
             COMMAND airy_depgraph
                     --links "${CMAKE_CURRENT_SOURCE_DIR}/tests/link-whitelist.txt"
                     --actual "${CMAKE_CURRENT_SOURCE_DIR}/tests/links-ok.txt")
    add_test(NAME depgraph-links-violation
             COMMAND airy_depgraph
                     --links "${CMAKE_CURRENT_SOURCE_DIR}/tests/link-whitelist.txt"
                     --actual "${CMAKE_CURRENT_SOURCE_DIR}/tests/links-violation.txt")
    set_tests_properties(depgraph-links-violation PROPERTIES WILL_FAIL TRUE)
endif()
