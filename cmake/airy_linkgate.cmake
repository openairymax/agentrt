# airy_linkgate.cmake —— 模块链接白名单构建期门禁（M1-1b，2026-09-02）
#
# 架构约束（0.1.9 方案 §2.2）："认知引擎只对 think_d 暴露服务面"——
# gateway 系目标禁链 coreloopthree/cognition。本模块把该事实固化为
# 构建期断言：
#   - link-whitelist.txt（agentrt 根，单一权威）声明 目标 -> 允许库
#   - 各 CMakeLists 在目标定义后调用 airy_linkgate_collect() 收集目标
#     实际链接（get_target_property LINK_LIBRARIES），configure 期写入
#     ${CMAKE_BINARY_DIR}/linkgate/<target>.links.txt
#   - airy_depgraph 配置完成后调用 airy_linkgate_install_checks() 为每个
#     已登记目标创建构建期门禁 target（ALL 依赖）：airy_depgraph
#     --links <whitelist> --actual <links>，越权/未登记项目库即
#     fail-closed（退出码 2 阻断构建）
#
# 延迟安装原因：airy_depgraph target 在 tools/airy_depgraph 定义
# （晚于 daemons/gateway 子目录），add_dependencies(ALL ...) 要求
# 被依赖 target 已存在，故拆分 collect/install 两阶段。

# 已登记目标名列表（INTERNAL cache 跨目录共享）
# 每次 include 先重置：cache 值跨 configure 累积会重复追加
unset(AIRY_LINKGATE_TARGETS CACHE)
unset(AIRY_LINKGATE_WHITELIST CACHE)
set(AIRY_LINKGATE_TARGETS "" CACHE INTERNAL "linkgate collected targets")
set(AIRY_LINKGATE_WHITELIST "" CACHE INTERNAL "linkgate whitelist file")

# 收集目标实际链接并生成清单（目标定义后调用）
function(airy_linkgate_collect TARGET_NAME WHITELIST_FILE)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "airy_linkgate_collect: target '${TARGET_NAME}' not defined yet")
    endif()
    set(AIRY_LINKGATE_WHITELIST "${WHITELIST_FILE}" CACHE INTERNAL "")

    get_target_property(_link_list ${TARGET_NAME} LINK_LIBRARIES)
    set(_clean "")
    if(_link_list)
        foreach(_l IN LISTS _link_list)
            # 过滤生成器表达式（$<TARGET_OBJECTS:...>）、链接器选项
            # （-Wl,--start-group 等）与空项；系统库留待 airy_depgraph 判定
            if(_l MATCHES "^\\$<" OR _l MATCHES "^-Wl" OR _l STREQUAL "")
                continue()
            endif()
            list(APPEND _clean "${_l}")
        endforeach()
    endif()

    set(_out "${CMAKE_BINARY_DIR}/linkgate/${TARGET_NAME}.links.txt")
    # CMake list 展开为 ';' 分隔，需转空格分隔（airy_depgraph manifest 语法）
    string(REPLACE ";" " " _clean_str "${_clean}")
    file(WRITE "${_out}" "${TARGET_NAME}: ${_clean_str}\n")
    set(_current "${AIRY_LINKGATE_TARGETS}")
    if(_current)
        set(_current "${_current};${TARGET_NAME}")
    else()
        set(_current "${TARGET_NAME}")
    endif()
    set(AIRY_LINKGATE_TARGETS "${_current}" CACHE INTERNAL "")
    airy_print_info("[LINKGATE] collected '${TARGET_NAME}' actual links -> ${_out}")
endfunction()

# 为所有已登记目标创建构建期门禁 target（airy_depgraph 定义后调用）
function(airy_linkgate_install_checks)
    if(NOT AIRY_LINKGATE_TARGETS)
        return()
    endif()
    if(NOT TARGET airy_depgraph)
        message(FATAL_ERROR "airy_linkgate_install_checks: airy_depgraph target missing")
    endif()
    foreach(_t IN LISTS AIRY_LINKGATE_TARGETS)
        set(_out "${CMAKE_BINARY_DIR}/linkgate/${_t}.links.txt")
        add_custom_target(airy_linkgate_${_t}
            COMMAND airy_depgraph
                    --links "${AIRY_LINKGATE_WHITELIST}"
                    --actual "${_out}"
            DEPENDS airy_depgraph
            COMMENT "linkgate: ${_t} 链接白名单校验（禁 coreloopthree/cognition）"
            USES_TERMINAL
        )
        airy_print_ok("[LINKGATE] gate installed for '${_t}' (fail-closed, 默认参与构建)")
    endforeach()
endfunction()
