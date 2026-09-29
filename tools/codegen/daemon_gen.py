#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# daemon_gen.py — agentrt daemon 五件套样板生成器（L3 SSoT）
#
# 解析 daemons/<name>/.manifest 契约源（每户唯一真值源），生成机制层三件：
#   a. src/main.c            入口引导（宏实例化 + 事件驱动装配 + 方法注册）
#   b. include/svc_<d>.h     唯一私有头（端点常量 + svc 钩子/handler 声明）
#   c. modules/sources.cmake CMake 真装配源清单
#
# 手写层（生成器不碰）：.manifest 本身、src/svc.c（钩子/handler 实现）、
# modules 内业务源文件、CMakeLists.txt、tests/。
#
# .manifest JSON schema v1:
#   {
#     "manifest_version": 1,
#     "daemon": "maths_d",                  # ^[a-z][a-z0-9_]*_d$
#     "cname": "maths",                     # 可缺省，默认去 _d 后缀
#     "sd_type": "maths",                   # SD 服务类型
#     "codegen": true,                      # 可缺省，默认 true；false =
#                                           # 仅登记声明（G17），不渲染
#                                           # 生成三件（形态特殊户：如零
#                                           # 依赖自持的 supervisor_d），
#                                           # --gen/--check 一律跳过
#     "ops": ["ipc", "llm", "tool"],        # 可缺省，默认 ["ipc"]；⊆ OPS_VOCAB
#     "cupolas": "pep",                     # 可缺省，默认 "pep"；安全穹顶
#                                           # 引导模式：pep=PEP 最小 guard
#                                           # （vault/entitlements/netsec 由
#                                           # PDP cupolas_d 集中持有），full=
#                                           # PDP 本体全量（四层+vault+
#                                           # entitlements+net_security）
#     "facades": ["ingress", ...],          # ⊆ FACADES_VOCAB
#     "slots": ["compute", ...],            # ⊆ SLOTS_VOCAB
#     "rpc": {
#       "unix": "maths.sock",               # ^[a-z][a-z0-9_]*\.sock$
#       "win_pipe": "airy_maths",           # ^[a-z][a-z0-9_]*$
#       "tcp": 8087,                        # 1024..65535
#       "tags": "maths,core",               # 逗号分隔小写词
#       "buffer": 65536,                    # >= 4096（可缺省）
#       "concurrent": true,                 # 可缺省，默认 false；并发客
#                                           # 户模式（长请求依赖并发取消）
#       "pool": {"max_events": 64, "min": 2, "max": 4, "queue": 256},
#                                           # 可缺省，取内置缺省
#       "methods": ["eval", ...]            # 小写下划线；shutdown 为协议
#                                           # 保留方法，由生成器自动注册，
#                                           # 禁止列入
#     },
#     "deps": {"required": [], "optional": []},
#     "modules": [{"name": "core", "sources": ["maths_service.c"]}]
#                                           # sources 为 src/ 下相对路径，
#                                           # 可含子目录（如 rpc/methods.c）；
#                                           # 禁绝对路径与 .. 上跳，须 .c 结尾
#   }
#
# 两种模式:
#   --gen   重新生成并写回产物（修改 .manifest 后使用）
#   --check 与仓库现有产物 diff，不一致返回非零退出码（CI 防漂移）
#
# 仅使用 Python 标准库，无第三方依赖。结构对齐 syscall_gen.py
# （parse/validate/render 三段式 + gen/check 双模式）。
#
# Generator version: 1.6.0

import argparse
import difflib
import json
import re
import sys
from pathlib import Path

GENERATOR_VERSION = "1.6.0"

# 生成产物相对 daemon 目录的固定落点（保持稳定，勿随意改名）
OUTPUT_MAIN = "src/main.c"
OUTPUT_HEADER = "include/svc_{daemon}.h"
OUTPUT_CMAKE = "modules/sources.cmake"

# 脚本所在目录推导仓库根（agentrt/）：codegen -> tools -> agentrt
SCRIPT_DIR = Path(__file__).resolve().parent
AGENTRT_ROOT = SCRIPT_DIR.parents[1]
DAEMONS_ROOT = AGENTRT_ROOT / "daemons"

SCHEMA_VERSION = 1

# shutdown：协议保留方法（DAEMON_DECLARE_SHUTDOWN_METHOD 自动注册响应
# {"status":"shutting_down"}），manifest methods 禁止列入
RESERVED_METHODS = frozenset({"shutdown"})

# ops 表词表：机制层 ops 引导设施（daemon_<op>_ops_init/cleanup），按
# OPS_ORDER 固定序装配（manifest 声明为集合语义，与书写顺序无关）。
OPS_VOCAB = frozenset({"ipc", "llm", "tool"})
OPS_ORDER = ("ipc", "llm", "tool")

# cupolas 安全穹顶引导模式（daemon_cupolas_bootstrap.c cupolas_bootstrap
# 的 pep_mode 参数声明化）：pep=PEP 最小 guard（消费方 16 户缺省），
# full=PDP 本体全量（仅 cupolas_d——vault/net/entitlements RPC 的承载者）
CUPOLAS_MODES = frozenset({"pep", "full"})

# UDS slots 词表（Unify Design SSoT）：facades 4 + slots 28（25 基础 + 3 补充）
FACADES_VOCAB = frozenset({"ingress", "execute", "state", "governance"})
SLOTS_VOCAB = frozenset({
    # 基础层 10
    "sync", "mutex", "sched", "isolate", "naming", "comm", "serial", "txn",
    "replicate", "recover",
    # 中间层 8
    "create", "delete", "update", "query", "discover", "config", "cache",
    "route",
    # 应用层 7
    "compute", "validate", "flow", "notify", "authz", "record", "display",
    # 补充 3
    "resilience", "stream", "calibrate",
})

# 事件循环装配内置缺省（manifest rpc.pool 可覆盖）
DEFAULT_POOL = {"max_events": 64, "min": 2, "max": 4, "queue": 256}
DEFAULT_BUFFER = 65536

SPDX_HEADER = [
    "/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */",
    "/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */",
]

RE_IDENT = re.compile(r"^[a-z][a-z0-9_]*$")
RE_DAEMON = re.compile(r"^[a-z][a-z0-9_]*_d$")
RE_SOCK = re.compile(r"^[a-z][a-z0-9_]*\.sock$")
RE_TAGS = re.compile(r"^[a-z][a-z0-9_]*(,[a-z][a-z0-9_]*)*$")


class GenError(Exception):
    """生成器校验/解析错误（带中文说明，便于快速定位）"""


def parse_manifest(path):
    """解析 .manifest JSON，返回 dict；JSON 语法错误转 GenError。"""
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise GenError(".manifest JSON 解析失败: %s" % exc) from exc
    if not isinstance(data, dict):
        raise GenError(".manifest 顶层必须是 JSON 对象")
    return data


def validate(data, path):
    """校验 manifest 契约一致性（schema v1 全量规则，见文件头注释）。"""
    if data.get("manifest_version") != SCHEMA_VERSION:
        raise GenError("%s: manifest_version 必须为 %d" % (path, SCHEMA_VERSION))

    daemon = data.get("daemon")
    if not isinstance(daemon, str) or not RE_DAEMON.match(daemon):
        raise GenError("%s: daemon 名非法（须 ^[a-z][a-z0-9_]*_d$）: %r"
                       % (path, daemon))
    data["cname"] = data.get("cname") or daemon[:-2]
    for key in ("cname", "sd_type"):
        val = data.get(key)
        if not isinstance(val, str) or not RE_IDENT.match(val):
            raise GenError("%s: %s 非法（小写下划线标识符）: %r"
                           % (path, key, val))

    for key, vocab in (("facades", FACADES_VOCAB), ("slots", SLOTS_VOCAB)):
        words = data.get(key)
        if not isinstance(words, list) or not words:
            raise GenError("%s: %s 必须为非空数组" % (path, key))
        bad = [w for w in words if w not in vocab]
        if bad:
            raise GenError("%s: %s 含词表外词 %s（词表见 daemon_gen.py 常量）"
                           % (path, key, bad))

    ops = data.get("ops") or ["ipc"]
    if not isinstance(ops, list) or not ops:
        raise GenError("%s: ops 必须为非空数组" % path)
    bad = [w for w in ops if w not in OPS_VOCAB]
    if bad:
        raise GenError("%s: ops 含词表外词 %s（⊆ %s）"
                       % (path, bad, sorted(OPS_VOCAB)))
    if len(set(ops)) != len(ops):
        raise GenError("%s: ops 存在重复" % path)
    data["ops"] = [op for op in OPS_ORDER if op in ops]

    cupolas = data.get("cupolas", "pep")
    if cupolas not in CUPOLAS_MODES:
        raise GenError("%s: cupolas 须为 %s 之一: %r"
                       % (path, sorted(CUPOLAS_MODES), cupolas))
    data["cupolas"] = cupolas

    rpc = data.get("rpc")
    if not isinstance(rpc, dict):
        raise GenError("%s: 缺 rpc 段" % path)

    unix = rpc.get("unix")
    if not isinstance(unix, str) or not RE_SOCK.match(unix):
        raise GenError("%s: rpc.unix 非法（须 <name>.sock）: %r" % (path, unix))
    win_pipe = rpc.get("win_pipe")
    if not isinstance(win_pipe, str) or not RE_IDENT.match(win_pipe):
        raise GenError("%s: rpc.win_pipe 非法: %r" % (path, win_pipe))

    tcp = rpc.get("tcp")
    if not isinstance(tcp, int) or not 1024 <= tcp <= 65535:
        raise GenError("%s: rpc.tcp 须在 1024..65535: %r" % (path, tcp))

    tags = rpc.get("tags")
    if not isinstance(tags, str) or not RE_TAGS.match(tags):
        raise GenError("%s: rpc.tags 非法（逗号分隔小写词）: %r" % (path, tags))

    buffer_size = rpc.get("buffer", DEFAULT_BUFFER)
    if not isinstance(buffer_size, int) or buffer_size < 4096:
        raise GenError("%s: rpc.buffer 须 >= 4096: %r" % (path, buffer_size))
    rpc["buffer"] = buffer_size

    concurrent = rpc.get("concurrent", False)
    if not isinstance(concurrent, bool):
        raise GenError("%s: rpc.concurrent 须为布尔: %r" % (path, concurrent))
    rpc["concurrent"] = concurrent

    pool = dict(DEFAULT_POOL)
    pool.update(rpc.get("pool") or {})
    for key in ("max_events", "min", "max", "queue"):
        if not isinstance(pool[key], int) or pool[key] < 1:
            raise GenError("%s: rpc.pool.%s 须为正整数: %r"
                           % (path, key, pool[key]))
    rpc["pool"] = pool

    methods = rpc.get("methods")
    if not isinstance(methods, list) or not methods:
        raise GenError("%s: rpc.methods 必须为非空数组" % path)
    for m in methods:
        if not isinstance(m, str) or not RE_IDENT.match(m):
            raise GenError("%s: rpc.methods 含非法方法名: %r" % (path, m))
        if m in RESERVED_METHODS:
            raise GenError("%s: rpc.methods 禁止列入协议保留方法 %r"
                           % (path, m))
    if len(set(methods)) != len(methods):
        raise GenError("%s: rpc.methods 存在重复" % path)

    deps = data.get("deps")
    if deps is not None and not isinstance(deps, dict):
        raise GenError("%s: deps 必须为对象" % path)

    modules = data.get("modules")
    if not isinstance(modules, list) or not modules:
        raise GenError("%s: modules 必须为非空数组" % path)
    seen = set()
    for mod in modules:
        if not isinstance(mod, dict) or not mod.get("name"):
            raise GenError("%s: modules 项须含 name" % path)
        sources = mod.get("sources")
        if not isinstance(sources, list) or not sources:
            raise GenError("%s: modules[%s].sources 必须为非空数组"
                           % (path, mod["name"]))
        for src in sources:
            if not isinstance(src, str) or not src.endswith(".c") \
                    or "\\" in src or src.startswith("/") \
                    or ".." in src.split("/") or "" in src.split("/"):
                raise GenError("%s: modules[%s] 源文件须为 src/ 下相对路径"
                               "（禁绝对路径与上跳，须 .c 结尾）: %r"
                               % (path, mod["name"], src))
            if src in seen:
                raise GenError("%s: 源文件重复列出: %s" % (path, src))
            seen.add(src)

    return data


def _emit_generated_banner():
    """生成产物头（SPDX + @generated 标记，禁止手工修改）。"""
    return [
        "/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */",
        "/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */",
        "",
        "/* @generated DO NOT EDIT — daemon_gen.py v%s (L3 SSoT) 生成。" % GENERATOR_VERSION,
        " * 机制层装配；策略层在 src/svc.c 与 modules（手写域）。",
        " * 改 .manifest 后: python3 agentrt/tools/codegen/daemon_gen.py --gen",
        " */",
        "",
    ]


def render_main(d):
    """渲染 src/main.c：机制层装配（目标 <= 160 行）。"""
    daemon = d["daemon"]
    cname = d["cname"]
    rpc = d["rpc"]
    pool = rpc["pool"]
    upper = daemon.upper()
    total_methods = len(rpc["methods"]) + 1  # + 协议保留 shutdown

    lines = _emit_generated_banner()
    lines += [
        '#include "platform.h"',
        '#include "airy_rt.h"',
        '#include "svc_%s.h"' % daemon,
        "",
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        "",
        '#include "daemon_main.h"',
    ]
    for op in d["ops"]:
        lines.append('#include "daemon_%s_ops_bootstrap.h"' % op)
    lines += [
        "",
        "DAEMON_DECLARE_COMMON(%s, %s," % (daemon, cname),
        "                      %s_SOCKET_UNIX, %s_SOCKET_WIN," % (upper, upper),
        "                      %s_TCP_PORT, %s_MAX_BUFFER)" % (upper, upper),
        "",
        "DAEMON_DECLARE_SHUTDOWN_METHOD(%s)" % daemon,
        "",
        "int main(int argc, char **argv)",
        "{",
        "    const char *config_path = NULL;",
        "    int use_tcp = 0;",
        "",
        "    int parse_rc = daemon_parse_args(argc, argv, &config_path, &use_tcp,",
        "                                     print_usage_%s);" % daemon,
        "    if (parse_rc > 0) return parse_rc == 1 ? 0 : 1;",
        "",
        "    airy_sock_init();",
        "    airy_mtx_init(&g_running_lock_%s);" % daemon,
        "",
        "#ifdef _WIN32",
        "    SetConsoleCtrlHandler((PHANDLER_ROUTINE)signal_handler_%s, TRUE);" % daemon,
        "#else",
        "    DAEMON_SETUP_SIGNALS(%s);" % daemon,
        "#endif",
        "",
        "    airy_logger_config_t log_cfg = {0};",
        "    const char *dbg = getenv(\"AIRY_%s_DEBUG\");" % upper,
        "    log_cfg.level = (dbg && dbg[0] == '1') ? (log_level_t)LOG_LEVEL_DEBUG :",
        "                     (log_level_t)LOG_LEVEL_WARN;",
        "    airy_log_init(&log_cfg);",
        "    atexit(log_cleanup);",
        "",
        "    int core_ret = airy_init();",
        "    if (core_ret == AIRY_SUCCESS)",
        "        SVC_LOG_INFO(\"corekern core initialized (%s runs on corekern)\");" % daemon,
        # 注：daemon 名已在生成期字面嵌入，无运行期格式符
        "    else",
        "        SVC_LOG_WARN(\"corekern init failed (%d), degraded (badge=0)\", core_ret);",
        "",
        '    daemon_cupolas_init%s("%s");'
        % ("_pep" if d["cupolas"] == "pep" else "", daemon),
    ]
    for op in d["ops"]:
        lines.append('    daemon_%s_ops_init("%s");' % (op, daemon))
    lines += [
        "",
        "    if (svc_prepare(config_path) != 0) {",
        "        SVC_LOG_ERROR(\"Service prepare failed\");",
        "        goto fail_svc;",
        "    }",
        "",
        "    daemon_endpoint_t ep;",
        "    svc_endpoint(&ep, use_tcp);",
        "",
        "    airy_sock_t server_fd = daemon_create_server_socket(",
        "        ep.use_tcp, ep.tcp_port, ep.sock_unix, ep.sock_win);",
        "    if (server_fd < 0) {",
        "        SVC_LOG_ERROR(\"Failed to create server socket\");",
        "        goto fail_svc;",
        "    }",
        "",
        "    daemon_event_config_t ev_config = {",
        "        .max_events = %d, .thread_pool_min = %d," % (pool["max_events"], pool["min"]),
        "        .thread_pool_max = %d, .thread_pool_queue_size = %d," % (pool["max"], pool["queue"]),
        "        .use_jsonrpc = true,",
    ]
    if rpc["concurrent"]:
        lines.append("        .concurrent_clients = true,")
    lines += [
        "        .on_client = daemon_on_client_%s," % daemon,
        "    };",
        "",
        "    const char *sock_addr = ep.use_tcp ? ep.tcp_host : ep.sock_unix;",
        "    int ret = daemon_init_event_driver(",
        "        \"%s\", \"%s\", sock_addr," % (daemon, d["sd_type"]),
        "        ep.use_tcp ? ep.tcp_port : 0, \"%s\"," % rpc["tags"],
        "        ep.use_tcp, &ev_config, &g_event_driver_%s, &g_bsd_%s," % (daemon, daemon),
        "        &g_bipc_%s);" % daemon,
        "    if (ret != AIRY_SUCCESS || !g_event_driver_%s) {" % daemon,
        "        SVC_LOG_ERROR(\"Failed to create event driver\");",
        "        airy_sock_close(server_fd);",
        "        goto fail_svc;",
        "    }",
        "",
        "    g_dispatcher_%s = daemon_event_driver_get_dispatcher(" % daemon,
        "        g_event_driver_%s);" % daemon,
        "    static const daemon_method_entry_t SVC_METHODS[] = {",
    ]
    for m in rpc["methods"]:
        lines.append('        {"%s", m_%s},' % (m, m))
    lines += [
        '        {"shutdown", on_shutdown_method_%s},' % daemon,
        "    };",
        "    DAEMON_REGISTER_METHODS(g_dispatcher_%s, SVC_METHODS);" % daemon,
        '    SVC_LOG_INFO("Registered %d RPC methods (%s.* namespace)");'
        % (total_methods, cname),
        "    svc_attach(g_dispatcher_%s);" % daemon,
        "",
        "    if (daemon_event_driver_add_server_fd(g_event_driver_%s," % daemon,
        "                                          (int)server_fd) != 0) {",
        "        SVC_LOG_ERROR(\"Failed to add server fd to event driver\");",
        "        goto fail_driver;",
        "    }",
        "",
        "    if (svc_activate(g_event_driver_%s, g_bsd_%s) != 0) {"
        % (daemon, daemon),
        "        SVC_LOG_ERROR(\"Service activate failed\");",
        "        goto fail_driver;",
        "    }",
        "",
        '    SVC_LOG_INFO("%s service running (event-driven mode)");' % cname,
        "    daemon_event_driver_run(g_event_driver_%s);" % daemon,
        "",
        "    svc_teardown();",
        "    daemon_cleanup_standard(g_bipc_%s, g_bsd_%s," % (daemon, daemon),
        "                            g_event_driver_%s, server_fd," % daemon,
        "                            ep.sock_unix, svc_destroy,",
        "                            &g_running_lock_%s);" % daemon,
    ]
    for op in reversed(d["ops"]):
        lines.append("    daemon_%s_ops_cleanup();" % op)
    lines += [
        "    daemon_cupolas_cleanup();",
        "    log_cleanup();",
        "    return 0;",
        "",
        "fail_driver:",
        "    daemon_event_driver_destroy(g_event_driver_%s);" % daemon,
        "    airy_sock_close(server_fd);",
        "fail_svc:",
        "    svc_destroy();",
        "    airy_mtx_destroy(&g_running_lock_%s);" % daemon,
        "    airy_sock_cleanup();",
        "    return EXIT_FAILURE;",
        "}",
        "",
    ]
    return "\n".join(lines)


def render_header(d):
    """渲染 include/svc_<d>.h：端点常量 + svc 钩子/handler 声明。"""
    daemon = d["daemon"]
    rpc = d["rpc"]
    upper = daemon.upper()
    guard = "SVC_%s_H" % upper

    lines = _emit_generated_banner()
    lines += [
        "#ifndef %s" % guard,
        "#define %s" % guard,
        "",
        '#include "platform.h"',
        '#include "daemon_main.h"',
        "",
        "#include <cjson/cJSON.h>",
        "",
        "/* 端点常量（wire 契约，与 .manifest rpc 段一致；svc_endpoint 缺省基线） */",
        "#define %s_SOCKET_UNIX airy_runtime_dir_socket(\"%s\")" % (upper, rpc["unix"]),
        '#define %s_SOCKET_WIN "\\\\\\\\.\\\\pipe\\\\%s"' % (upper, rpc["win_pipe"]),
        "#define %s_TCP_PORT %d" % (upper, rpc["tcp"]),
        "#define %s_MAX_BUFFER %d" % (upper, rpc["buffer"]),
        "",
        "/* 端点解析钩子：常量户回填上方基线；可配置户在 svc.c 完成",
        " * config/env 覆盖后与 cmdline use_tcp 融合。实现: src/svc.c。 */",
        "void svc_endpoint(daemon_endpoint_t *ep, int cmdline_tcp);",
        "",
        "/* 生命周期钩子（实现: src/svc.c）；activate 收到事件驱动句柄与",
        " * SD bootstrap 句柄，供事件耦合激活策略（如监控采样线程）与",
        " * manifest deps 驱动的依赖探测健康面使用。 */",
        "int svc_prepare(const char *config_path);",
        "int svc_activate(daemon_event_driver_t *driver, daemon_bootstrap_sd_t *bsd);",
        "void svc_teardown(void);",
        "void svc_destroy(void);",
        "",
        "/* 策略层附加装配挂点：静态注册表（SVC_METHODS）落库后的动态",
        " * 注册出口（如 roadmap.* 方法族）。实现: src/svc.c；无附加",
        " * 注册的户提供空实现。dispatcher 为 method_dispatcher_t。 */",
        "void svc_attach(void *dispatcher);",
        "",
        "/* RPC handler 族（实现: src/svc.c）。签名对齐 method_fn；",
        " * 命名 m_<method>，与 .manifest rpc.methods 一一对应。 */",
    ]
    for m in rpc["methods"]:
        lines.append("void m_%s(cJSON *params, int id, void *user_data);" % m)
    lines += [
        "",
        "#endif /* %s */" % guard,
        "",
    ]
    return "\n".join(lines)


def render_cmake(d):
    """渲染 modules/sources.cmake：CMake 真装配源清单。"""
    daemon = d["daemon"]
    var = "%s_SOURCES" % daemon.upper()
    lines = [
        "# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.",
        "# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0",
        "#",
        "# @generated DO NOT EDIT",
        "# 由 daemon_gen.py v%s 自 .manifest 生成（L3 SSoT），禁手改。" % GENERATOR_VERSION,
        "# 手写域: src/svc.c 与 modules 业务源；本文件只做真装配。",
        "",
        "set(%s" % var,
        "    src/main.c",
        "    src/svc.c",
    ]
    for mod in d["modules"]:
        for src in mod["sources"]:
            lines.append("    src/%s" % src)
    lines += [
        ")",
        "",
    ]
    return "\n".join(lines)


def generate(manifest_path):
    """解析并校验 manifest，渲染全部生成产物，返回 {相对路径: 内容}。"""
    manifest_path = Path(manifest_path).resolve()
    daemon_dir = manifest_path.parent
    d = validate(parse_manifest(manifest_path), manifest_path)
    daemon = d["daemon"]
    return {
        OUTPUT_MAIN: render_main(d),
        OUTPUT_HEADER.format(daemon=daemon): render_header(d),
        OUTPUT_CMAKE: render_cmake(d),
    }, daemon_dir


def write_outputs(contents, daemon_dir):
    """将生成产物写回 daemon 目录（--gen 模式）。"""
    for rel, content in contents.items():
        out = Path(daemon_dir) / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(content, encoding="utf-8")
        print("Generated: %s" % out)


def check_outputs(contents, daemon_dir):
    """对比生成产物与仓库现有文件，任一不一致即返回非零（--check 模式）。"""
    rc = 0
    for rel, content in contents.items():
        out = Path(daemon_dir) / rel
        if not out.exists():
            print("ERROR: output file does not exist: %s" % out, file=sys.stderr)
            print("       Run 'daemon_gen.py --gen' first and commit the result.",
                  file=sys.stderr)
            rc = 1
            continue
        existing = out.read_text(encoding="utf-8")
        if existing == content:
            print("OK: generated content matches committed file (%s)" % out)
            continue
        print("ERROR: generated content differs from committed file: %s" % out,
              file=sys.stderr)
        print("       Run 'daemon_gen.py --gen' to regenerate and commit the result.",
              file=sys.stderr)
        diff = difflib.unified_diff(
            existing.splitlines(keepends=True),
            content.splitlines(keepends=True),
            fromfile=str(out) + " (committed)",
            tofile=str(out) + " (generated)",
        )
        sys.stderr.writelines(diff)
        rc = 1
    return rc


def discover_manifests(daemon):
    """定位 .manifest：指定 daemon 时取单户，否则扫描全部 daemons。"""
    if daemon:
        path = DAEMONS_ROOT / daemon / ".manifest"
        if not path.exists():
            raise GenError(".manifest 不存在: %s" % path)
        return [path]
    paths = sorted(DAEMONS_ROOT.glob("*/.manifest"))
    if not paths:
        raise GenError("daemons/ 下未发现任何 .manifest")
    return paths


def main():
    parser = argparse.ArgumentParser(
        description=".manifest → main.c / svc_<d>.h / sources.cmake codegen (L3 SSoT)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
示例:
  %(prog)s --gen --daemon maths_d   # 生成/写回单户产物
  %(prog)s --check                  # 全部已声明 daemon 防漂移校验（CI）
  %(prog)s --gen                    # 全部已声明 daemon 重新生成
""",
    )
    parser.add_argument(
        "--daemon", "-d",
        default=None,
        help="目标 daemon 名（默认: 扫描 daemons/*/.manifest 全部）",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--gen",
        action="store_true",
        help="生成模式：重新生成并写回产物",
    )
    mode.add_argument(
        "--check",
        action="store_true",
        help="校验模式：与仓库产物 diff，不一致返回非零退出码（防漂移）",
    )
    args = parser.parse_args()

    try:
        rc = 0
        for manifest in discover_manifests(args.daemon):
            if not parse_manifest(manifest).get("codegen", True):
                print("SKIP (codegen=false): %s" % manifest)
                continue
            contents, daemon_dir = generate(manifest)
            if args.check:
                rc |= check_outputs(contents, daemon_dir)
            else:
                write_outputs(contents, daemon_dir)
    except GenError as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
