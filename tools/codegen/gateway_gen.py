#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# gateway_gen.py — gateway 策略表生成器（L3 策略数据化）
#
# 输入: agentrt/gateway/.manifest（唯一真值源 SSoT，发布即冻结）
# 输出: <build>/gateway_policy_gen.inc  能力表 + 命名空间策略表 + L2 方法集
#       <build>/gateway_sock_gen.inc    sock 绑定键 -> ctx 端点字段偏移
#
# 生成期 fail-closed：schema 非法、命名空间/能力重复、kind 归属不符、
# 路由缺少 sock 绑定等任一错误 → 非零退出并中止构建，不产出半成品。
# 产物只写构建目录，源码树零生成物（保持 SLOC 口径真实）。
#
# 仅使用 Python 标准库，无第三方依赖。
#
# Generator version: 1.0.0

import argparse
import json
import re
import sys

from pathlib import Path

GENERATOR_VERSION = "1.0.0"

SCRIPT_DIR = Path(__file__).resolve().parent
AGENTRT_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_MANIFEST = AGENTRT_ROOT / "gateway" / ".manifest"

OUT_POLICY = "gateway_policy_gen.inc"
OUT_SOCK = "gateway_sock_gen.inc"

SPDX_HEADER = [
    "/* SPDX-FileCopyrightText: 2026 SPHARX Ltd. */",
    "/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */",
]

IDENT_RE = re.compile(r"^[a-z][a-z0-9_]*$")
PERM_RE = re.compile(r"^cap:[a-z][a-z0-9_.]*$")
DAEMON_RE = re.compile(r"^[a-z][a-z0-9_]*_d$")

KINDS = ("fwd", "mem", "llm_list", "tool_approve", "hall", "agent_run")
KIND_HOME = {
    "mem": "mem",
    "llm_list": "llm",
    "tool_approve": "tool",
    "hall": "hall",
    "agent_run": "agent",
}
KIND_ENUM = {
    "fwd": "GW_CAP_KIND_FWD",
    "mem": "GW_CAP_KIND_MEM",
    "llm_list": "GW_CAP_KIND_LLM_LIST",
    "tool_approve": "GW_CAP_KIND_TOOL_APPROVE",
    "hall": "GW_CAP_KIND_HALL",
    "agent_run": "GW_CAP_KIND_AGENT_RUN",
}


class GenError(Exception):
    """清单校验/渲染错误（中文说明，便于定位）"""


def fail(msg):
    raise GenError(msg)


def need_bool(value, where):
    if not isinstance(value, bool):
        fail("%s: 期望 bool，实际 %r" % (where, value))
    return value


def need_int(value, where, minimum=1):
    if isinstance(value, bool) or not isinstance(value, int):
        fail("%s: 期望整数，实际 %r" % (where, value))
    if value < minimum:
        fail("%s: 期望 >= %d，实际 %d" % (where, minimum, value))
    return value


def need_str(value, where, pattern=IDENT_RE):
    if not isinstance(value, str) or not pattern.match(value):
        fail("%s: 非法标识符 %r" % (where, value))
    return value


def opt_str(obj, key, where):
    value = obj.get(key)
    if value is None:
        return None
    return need_str(value, "%s.%s" % (where, key))


def reject_unknown(obj, allowed, where):
    extra = sorted(set(obj) - set(allowed))
    if extra:
        fail("%s: 未登记字段 %s" % (where, ", ".join(extra)))


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as fp:
            return json.load(fp)
    except OSError as exc:
        fail("无法读取清单 %s: %s" % (path, exc))
    except ValueError as exc:
        fail("清单 JSON 解析失败 %s: %s" % (path, exc))


def parse_manifest(doc):
    if not isinstance(doc, dict):
        fail("清单根节点必须是对象")
    reject_unknown(doc, ("manifest_version", "module", "generator", "facades", "slots",
                         "defaults", "l2_methods", "sock_bindings", "namespaces"), "root")
    if doc.get("manifest_version") != 1:
        fail("root.manifest_version: 仅支持 1，实际 %r" % doc.get("manifest_version"))
    if doc.get("module") != "gateway":
        fail("root.module: 期望 gateway，实际 %r" % doc.get("module"))

    for key in ("facades", "slots"):
        value = doc.get(key)
        if not isinstance(value, list) or not value:
            fail("root.%s: 期望非空数组" % key)
        for i, item in enumerate(value):
            need_str(item, "root.%s[%d]" % (key, i))

    defaults = doc.get("defaults")
    if not isinstance(defaults, dict):
        fail("root.defaults: 期望对象")
    reject_unknown(defaults, ("timeout_ms", "version"), "defaults")
    def_timeout = need_int(defaults.get("timeout_ms"), "defaults.timeout_ms")
    def_version = need_int(defaults.get("version"), "defaults.version")

    l2_raw = doc.get("l2_methods")
    if not isinstance(l2_raw, list) or not l2_raw:
        fail("root.l2_methods: 期望非空数组")
    l2_methods = []
    for i, item in enumerate(l2_raw):
        name = need_str(item, "root.l2_methods[%d]" % i)
        if name in l2_methods:
            fail("root.l2_methods: 重复方法 %s" % name)
        l2_methods.append(name)

    socks_raw = doc.get("sock_bindings")
    if not isinstance(socks_raw, list) or not socks_raw:
        fail("root.sock_bindings: 期望非空数组")
    socks = []
    for i, item in enumerate(socks_raw):
        key = need_str(item, "root.sock_bindings[%d]" % i)
        if key in socks:
            fail("root.sock_bindings: 重复绑定键 %s" % key)
        socks.append(key)

    ns_raw = doc.get("namespaces")
    if not isinstance(ns_raw, list) or not ns_raw:
        fail("root.namespaces: 期望非空数组")

    ns_map = {}
    for i, item in enumerate(ns_raw):
        where = "namespaces[%d]" % i
        if not isinstance(item, dict):
            fail("%s: 期望对象" % where)
        reject_unknown(item, ("ns", "owner", "route", "daemon", "sock", "timeout_ms",
                              "prefix", "l2_pass", "caps"), where)
        ns = need_str(item.get("ns"), where + ".ns")
        if len(ns) > 31:
            fail("%s.ns: 超过 31 字节" % where)
        if ns in ns_map:
            fail("%s.ns: 重复命名空间 %s" % (where, ns))
        owner = need_str(item.get("owner"), where + ".owner")
        route = need_bool(item.get("route"), where + ".route")
        daemon = opt_str(item, "daemon", where)
        sock = opt_str(item, "sock", where)
        if route:
            if not daemon or not DAEMON_RE.match(daemon):
                fail("%s.daemon: 路由命名空间必须声明 <name>_d" % where)
            if not sock or sock not in socks:
                fail("%s.sock: %r 不在 sock_bindings" % (where, sock))
        elif daemon or sock:
            fail("%s: 非路由命名空间不得声明 daemon/sock" % where)

        timeout = item.get("timeout_ms")
        timeout = def_timeout if timeout is None else need_int(timeout, where + ".timeout_ms")
        prefix = opt_str(item, "prefix", where)
        l2_pass = need_bool(item["l2_pass"], where + ".l2_pass") if "l2_pass" in item else False
        if l2_pass and not prefix:
            fail("%s.l2_pass: 为真时必须声明 prefix" % where)
        if "l2_pass" in item and not prefix:
            fail("%s.l2_pass: 仅 prefix 命名空间可声明" % where)

        caps_raw = item.get("caps", [])
        if not isinstance(caps_raw, list):
            fail("%s.caps: 期望数组" % where)
        caps = []
        for j, cap in enumerate(caps_raw):
            cwhere = "%s.caps[%d]" % (where, j)
            if not isinstance(cap, dict):
                fail("%s: 期望对象" % cwhere)
            reject_unknown(cap, ("name", "kind", "version", "perm"), cwhere)
            name = need_str(cap.get("name"), cwhere + ".name")
            kind = cap.get("kind", "fwd")
            if kind not in KINDS:
                fail("%s.kind: 非法 kind %r" % (cwhere, kind))
            if kind != "fwd" and KIND_HOME[kind] != ns:
                fail("%s.kind: %s 只能挂在 %s" % (cwhere, kind, KIND_HOME[kind]))
            version = cap.get("version")
            version = def_version if version is None else need_int(version, cwhere + ".version")
            perm = cap.get("perm")
            if perm is not None and not (isinstance(perm, str) and PERM_RE.match(perm)):
                fail("%s.perm: 非法权限名 %r" % (cwhere, perm))
            caps.append({
                "name": name,
                "kind": kind,
                "version": version,
                "perm": perm,
            })
        ns_map[ns] = {
            "ns": ns,
            "owner": owner,
            "route": route,
            "daemon": daemon,
            "sock": sock,
            "timeout": timeout,
            "prefix": prefix,
            "l2_pass": l2_pass,
            "caps": caps,
        }

    for ns, entry in ns_map.items():
        owner = entry["owner"]
        if owner != "gateway" and owner not in ns_map:
            fail("namespace %s: owner %r 未声明" % (ns, owner))
        if owner == "gateway" and entry["route"]:
            fail("namespace %s: gateway 为网关内实现域，不可路由" % ns)

    cap_keys = set()
    for ns, entry in ns_map.items():
        for cap in entry["caps"]:
            key = ns + "." + cap["name"]
            if key in cap_keys:
                fail("能力重复登记: %s" % key)
            cap_keys.add(key)
            if cap["kind"] != "fwd":
                continue
            owner = entry["owner"]
            if owner not in ns_map or not ns_map[owner]["route"]:
                fail("%s: FWD 归属 %r 缺失或不可路由" % (key, owner))

    return {
        "l2_methods": l2_methods,
        "socks": socks,
        "namespaces": list(ns_map.values()),
        "cap_count": len(cap_keys),
    }


def cap_wire(cap, entry, l2_methods):
    if entry["prefix"]:
        if entry["l2_pass"] and cap["name"] in l2_methods:
            return cap["name"]
        return entry["prefix"] + "_" + cap["name"]
    return cap["name"]


def cap_row(ns, cap, entry, l2_methods):
    kind = cap["kind"]
    target = entry["owner"] if kind == "fwd" else ns
    method = cap_wire(cap, entry, l2_methods)
    perm = '"%s"' % cap["perm"] if cap["perm"] else "NULL"
    return '    {"%s.%s", "%s", "%s", %s, %d, %s},' % (
        ns, cap["name"], target, method, KIND_ENUM[kind], cap["version"], perm)


def render_policy(model):
    lines = list(SPDX_HEADER)
    lines.append("")
    lines.append("/* 由 tools/codegen/gateway_gen.py 从 gateway/.manifest 生成，勿手改。 */")
    lines.append("/* Generator: v%s */" % GENERATOR_VERSION)
    lines.append("")
    lines.append("static const gw_ns_pol_t GW_NS_POLICY[] = {")
    for ns in model["namespaces"]:
        lines.append('    {"%s", "%s", %s, %s, %uu, %s, %d, %d},' % (
            ns["ns"], ns["owner"],
            '"%s"' % ns["daemon"] if ns["daemon"] else "NULL",
            '"%s"' % ns["sock"] if ns["sock"] else "NULL",
            ns["timeout"],
            '"%s"' % ns["prefix"] if ns["prefix"] else "NULL",
            1 if ns["l2_pass"] else 0,
            1 if ns["route"] else 0))
    lines.append("};")
    lines.append("#define GW_NS_POLICY_N (sizeof(GW_NS_POLICY) / sizeof(GW_NS_POLICY[0]))")
    lines.append("")
    lines.append("static const gw_cap_t GW_CAP_REGISTRY[] = {")
    for ns in model["namespaces"]:
        for cap in ns["caps"]:
            lines.append(cap_row(ns["ns"], cap, ns, model["l2_methods"]))
    lines.append("};")
    lines.append("#define GW_CAP_REGISTRY_N (sizeof(GW_CAP_REGISTRY) / sizeof(GW_CAP_REGISTRY[0]))")
    lines.append("")
    lines.append("static const char *const GW_L2_METHODS[] = {")
    for name in model["l2_methods"]:
        lines.append('    "%s",' % name)
    lines.append("};")
    lines.append("#define GW_L2_METHODS_N (sizeof(GW_L2_METHODS) / sizeof(GW_L2_METHODS[0]))")
    lines.append("")
    return "\n".join(lines)


def render_sock(model):
    lines = list(SPDX_HEADER)
    lines.append("")
    lines.append("/* 由 tools/codegen/gateway_gen.py 从 gateway/.manifest 生成，勿手改。 */")
    lines.append("/* Generator: v%s */" % GENERATOR_VERSION)
    lines.append("")
    lines.append("static const gw_sock_bind_t GW_SOCKS[] = {")
    for key in model["socks"]:
        lines.append('    {"%s", GW_SOCK_FIELD(%s)},' % (key, key))
    lines.append("};")
    lines.append("#define GW_SOCKS_N (sizeof(GW_SOCKS) / sizeof(GW_SOCKS[0]))")
    lines.append("")
    return "\n".join(lines)


def write_if_changed(path, content):
    try:
        if path.exists() and path.read_text(encoding="utf-8") == content:
            return False
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return True
    except OSError as exc:
        fail("写入 %s 失败: %s" % (path, exc))


def render_caps_tsv(model):
    """能力清单查询面（门禁消费）：cap_key、目标 ns、wire 方法名。"""
    lines = []
    for ns in model["namespaces"]:
        for cap in ns["caps"]:
            key = "%s.%s" % (ns["ns"], cap["name"])
            target = ns["owner"] if cap["kind"] == "fwd" else ns["ns"]
            lines.append("%s\t%s\t%s" % (key, target, cap_wire(cap, ns, model["l2_methods"])))
    return "\n".join(lines) + "\n"


def main(argv):
    parser = argparse.ArgumentParser(description="gateway 策略表生成器（L3 策略数据化）")
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST), help="清单路径")
    parser.add_argument("--out-dir", help="产物输出目录（构建目录）")
    parser.add_argument("--dump-caps", action="store_true",
                        help="输出 cap_key<TAB>ns<TAB>wire_method（供门禁查询）")
    args = parser.parse_args(argv)
    if not args.dump_caps and not args.out_dir:
        parser.error("--out-dir 或 --dump-caps 至少指定一项")

    try:
        model = parse_manifest(load_json(args.manifest))
        if args.dump_caps:
            sys.stdout.write(render_caps_tsv(model))
        if args.out_dir:
            out_dir = Path(args.out_dir)
            write_if_changed(out_dir / OUT_POLICY, render_policy(model))
            write_if_changed(out_dir / OUT_SOCK, render_sock(model))
    except GenError as exc:
        sys.stderr.write("gateway_gen: %s\n" % exc)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
