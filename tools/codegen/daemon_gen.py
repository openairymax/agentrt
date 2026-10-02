#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# daemon_gen.py — agentrt daemon 五件套样板生成器（L3 SSoT）
#
# 解析 daemons/<name>/.manifest 契约源（每户唯一真值源），生成三件：
#   a. src/main.c            入口策略面（符号宏实例化 + DAEMON_BOOT_WIRE
#                            接线 + daemon_boot_t 策略包填充；装配机制
#                            收敛于 daemon_boot.c）
#   b. include/svc_<d>.h     唯一私有头（端点常量 + svc 钩子/handler 声明
#                            + SVC_<D>_METHODS(X) 方法表清单）
#   c. modules/sources.cmake CMake 真装配源清单
#
# 手写层（生成器不碰）：.manifest 本身、src/svc.c（钩子/handler 实现）、
# modules 内业务源文件、CMakeLists.txt、tests/。
#
# .manifest JSON schema v2:
#   {
#     "manifest_version": 2,
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
#     "activate_noop": true,                # 可缺省，默认 false；true =
#                                           # 无激活策略户，DAEMON_BOOT_WIRE
#                                           # 引用机制层 daemon_svc_noop
#                                           # 缺省（0.1.19 §80），svc_*.h
#                                           # 不发 svc_activate 声明，
#                                           # src/svc.c 不再维护空桩
#     "facades": ["ingress", ...],          # ⊆ FACADES_VOCAB
#     "slots": ["compute", ...],            # ⊆ SLOTS_VOCAB
#     "rpc": {
#       "unix": "maths.sock",               # ^[a-z][a-z0-9_]*\.sock$
#       "win_pipe": "airy_maths",           # ^[a-z][a-z0-9_]*$
#       "tcp": "AIRY_PORT_MATHS_D",         # SSoT 端口符号
#                                           # ^AIRY_PORT_[A-Z0-9_]+$；真值
#                                           # 唯一定义于 commons/include/
#                                           # airy_defaults.h，此处只登记引用
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
# Generator version: 1.10.0

import argparse
import difflib
import json
import re
import sys
from pathlib import Path

GENERATOR_VERSION = "1.10.0"

# 生成产物相对 daemon 目录的固定落点（保持稳定，勿随意改名）
OUTPUT_MAIN = "src/main.c"
OUTPUT_HEADER = "include/svc_{daemon}.h"
OUTPUT_CMAKE = "modules/sources.cmake"

# main.c 行数预算（方案 §2.4：入口只保留策略包填充，不写装配逻辑）。
# 0.1.19 §79 装配骨架上提 daemon_boot() 后入口恒短；超限即 fail-closed
# （生成期与校验期均生效），防止装配回潮。
MAX_MAIN_LINES = 100

# 脚本所在目录推导仓库根（agentrt/）：codegen -> tools -> agentrt
SCRIPT_DIR = Path(__file__).resolve().parent
AGENTRT_ROOT = SCRIPT_DIR.parents[1]
DAEMONS_ROOT = AGENTRT_ROOT / "daemons"

SCHEMA_VERSION = 2

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
RE_PORT_SYM = re.compile(r"^AIRY_PORT_[A-Z0-9_]+$")


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

    activate_noop = data.get("activate_noop", False)
    if not isinstance(activate_noop, bool):
        raise GenError("%s: activate_noop 须为布尔: %r" % (path, activate_noop))
    data["activate_noop"] = activate_noop

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
    if not isinstance(tcp, str) or not RE_PORT_SYM.match(tcp):
        raise GenError(
            "%s: rpc.tcp 须为 SSoT 端口符号（^AIRY_PORT_[A-Z0-9_]+$，真值"
            "唯一定义于 commons/include/airy_defaults.h）: %r" % (path, tcp))

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
        " * manifest 派生产物；装配机制在 daemons/common，策略在 src/svc.c",
        " * 与 modules（手写域）。",
        " * 改 .manifest 后: python3 agentrt/tools/codegen/daemon_gen.py --gen",
        " */",
        "",
    ]


def render_main(d):
    """渲染 src/main.c：入口策略面（静态表 + DAEMON_BOOT_WIRE 接线宏 +
    策略包填充），机制在 daemon_boot.c（daemon_main.h）。"""
    daemon = d["daemon"]
    cname = d["cname"]
    rpc = d["rpc"]
    pool = rpc["pool"]
    upper = daemon.upper()
    total_methods = len(rpc["methods"]) + 1  # + 协议保留 shutdown
    # 激活钩子策略面（0.1.19 §80）：实体户传 svc_activate（src/svc.c
    # 生命周期钩子），无激活策略户传机制层 daemon_svc_noop 缺省。
    activate = "daemon_svc_noop" if d["activate_noop"] else "svc_activate"

    lines = _emit_generated_banner()
    lines += [
        '#include "platform.h"',
        '#include "airy_rt.h"',
        '#include "svc_%s.h"' % daemon,
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
        "static const daemon_method_entry_t SVC_METHODS[] = {",
        "    SVC_%s_METHODS(DAEMON_METHOD_ENTRY)" % upper,
        "};",
        "",
        "static const daemon_op_t SVC_OPS[] = {",
    ]
    for op in d["ops"]:
        lines.append("    { daemon_%s_ops_init, daemon_%s_ops_cleanup }," % (op, op))
    lines += [
        "};",
        "",
        "int main(int argc, char **argv)",
        "{",
        "    daemon_boot_t boot = {",
        '        .daemon = "%s",' % daemon,
        '        .cname = "%s",' % cname,
        '        .env_debug = "AIRY_%s_DEBUG",' % upper,
        '        .sd_type = "%s",' % d["sd_type"],
        '        .tags = "%s",' % rpc["tags"],
        "        .method_total = %d," % total_methods,
        "        .running_lock = &g_running_lock_%s," % daemon,
        "        .signal_handler = signal_handler_%s," % daemon,
        "        .log_toggle = svc_log_toggle_handler_%s," % daemon,
        "        .print_usage = print_usage_%s," % daemon,
        "        .on_client = daemon_on_client_%s," % daemon,
        "        .dispatcher = &g_dispatcher_%s," % daemon,
        "        .event_driver = &g_event_driver_%s," % daemon,
        "        .bsd = &g_bsd_%s," % daemon,
        "        .bipc = &g_bipc_%s," % daemon,
        "        .pool_max_events = %d," % pool["max_events"],
        "        .pool_min = %d," % pool["min"],
        "        .pool_max = %d," % pool["max"],
        "        .pool_queue = %d," % pool["queue"],
    ]
    if rpc["concurrent"]:
        lines.append("        .concurrent_clients = 1,")
    lines += [
        "        DAEMON_BOOT_WIRE(SVC_OPS, SVC_METHODS, %s, daemon_cupolas_init%s),"
        % (activate, "_pep" if d["cupolas"] == "pep" else ""),
        "    };",
        "    return daemon_boot(argc, argv, &boot);",
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
        '#include "airy_defaults.h"',
        "",
        "#include <cjson/cJSON.h>",
        "",
        "/* 端点常量（wire 契约，与 .manifest rpc 段一致；svc_endpoint 缺省基线） */",
        "#define %s_SOCKET_UNIX airy_runtime_dir_socket(\"%s\")" % (upper, rpc["unix"]),
        '#define %s_SOCKET_WIN "\\\\\\\\.\\\\pipe\\\\%s"' % (upper, rpc["win_pipe"]),
        "/* TCP 口为 SSoT 引用，真值唯一定义于 airy_defaults.h */",
        "#define %s_TCP_PORT %s" % (upper, rpc["tcp"]),
        "#define %s_MAX_BUFFER %d" % (upper, rpc["buffer"]),
        "",
        "/* 端点解析钩子：常量户回填上方基线；可配置户在 svc.c 完成",
        " * config/env 覆盖后与 cmdline use_tcp 融合。实现: src/svc.c。 */",
        "void svc_endpoint(daemon_endpoint_t *ep, int cmdline_tcp);",
        "",
    ]
    if d["activate_noop"]:
        # 无激活策略户：svc.c 无 svc_activate，声明由机制层
        # daemon_svc_noop 承担（daemon_main.h），此处不再发出。
        lines += [
            "/* 生命周期钩子（实现: src/svc.c）；激活钩子无策略需求，由",
            " * 机制层 daemon_svc_noop 缺省（daemon_main.h，0.1.19 §80），",
            " * svc.c 不再维护空桩副本。 */",
            "int svc_prepare(const char *config_path);",
            "void svc_teardown(void);",
            "void svc_destroy(void);",
        ]
    else:
        lines += [
            "/* 生命周期钩子（实现: src/svc.c）；activate 收到事件驱动句柄与",
            " * SD bootstrap 句柄，供事件耦合激活策略（如监控采样线程）与",
            " * manifest deps 驱动的依赖探测健康面使用。 */",
            "int svc_prepare(const char *config_path);",
            "int svc_activate(daemon_event_driver_t *driver, daemon_bootstrap_sd_t *bsd);",
            "void svc_teardown(void);",
            "void svc_destroy(void);",
        ]
    lines += [
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
        "/* RPC 方法表清单（唯一声明源，源自 .manifest rpc.methods）。",
        " * main.c 以 X 宏展开为 daemon_method_entry_t[]：",
        " *   #define X(n, f) {(n), (f)},",
        " *   static const daemon_method_entry_t T[] = { SVC_%s_METHODS(X) };" % upper,
        " * 装配行数与方法数解耦（机制层装配，策略数据在此单点维护）。 */",
        "#define SVC_%s_METHODS(X) \\" % upper,
    ]
    for m in rpc["methods"]:
        lines.append('    X("%s", m_%s) \\' % (m, m))
    lines.append('    X("shutdown", on_shutdown_method_%s)' % daemon)
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


def check_main_budget(contents, daemon):
    """main.c 行数预算 fail-closed：超 MAX_MAIN_LINES 即报错。

    装配行数应与方法数解耦（方法表经 SVC_<D>_METHODS X 宏展开），任何
    户超限都是装配回潮的硬信号，--gen/--check 均须阻断。
    """
    n = len(contents[OUTPUT_MAIN].splitlines())
    if n > MAX_MAIN_LINES:
        raise GenError("%s/%s 超预算：%d 行 > %d 行上限"
                       % (daemon, OUTPUT_MAIN, n, MAX_MAIN_LINES))


def generate(manifest_path):
    """解析并校验 manifest，渲染全部生成产物，返回 {相对路径: 内容}。"""
    manifest_path = Path(manifest_path).resolve()
    daemon_dir = manifest_path.parent
    d = validate(parse_manifest(manifest_path), manifest_path)
    daemon = d["daemon"]
    contents = {
        OUTPUT_MAIN: render_main(d),
        OUTPUT_HEADER.format(daemon=daemon): render_header(d),
        OUTPUT_CMAKE: render_cmake(d),
    }
    check_main_budget(contents, daemon)
    return contents, daemon_dir


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
