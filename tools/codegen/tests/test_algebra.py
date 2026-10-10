#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# test_algebra.py — G14 组合代数 ⊗ 五性质自测（stdlib-only，无 pytest）
#
# 守门对象: tools/codegen/daemon_gen.py::check_algebra。以合成 data 直调
# 装配期判定函数，覆盖五性质正反双向，并显式验证：
#   结合律      —— 装配为无序并集语义：操作数任意分组/置换不改变判定；
#   ⊙ 单位元    —— deps 空集为不引入编排的空元（缺省即合法）。
# 末尾以真实 .manifest（discover_manifests）做端到端集成，确保现网 12 户
# （11 树内 + cupolas_d 外户）在 G14 下恒 rc0。
#
# 退出码约定对齐门禁脚本: 0=全通过 1=有失败（fail-closed）。
#
# 运行: python3 tools/codegen/tests/test_algebra.py
# CI:   .github/workflows/codegen-check.yml::daemon-codegen-check

import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
CODEGEN_DIR = SCRIPT_DIR.parent
sys.path.insert(0, str(CODEGEN_DIR))

import daemon_gen  # noqa: E402

GenError = daemon_gen.GenError
check_algebra = daemon_gen.check_algebra

FAILS = []


def base_data(**over):
    """最小合法装配声明：单面 + 单格 + 单件（core.c）+ 无编排目标。"""
    data = {
        "facades": ["ingress"],
        "slots": ["compute"],
        "daemon": "probe_d",
        "deps": {},
        "modules": [{"name": "core", "sources": ["core.c"]}],
    }
    data.update(over)
    return data


def expect_ok(label, data, daemon_dir):
    try:
        check_algebra(data, "<synth>", daemon_dir)
    except GenError as exc:
        FAILS.append("%s: 期望通过却 fail-closed: %s" % (label, exc))
    else:
        print("ok   %s" % label)


def expect_fail(label, data, daemon_dir):
    try:
        check_algebra(data, "<synth>", daemon_dir)
    except GenError:
        print("ok   %s" % label)
    else:
        FAILS.append("%s: 期望 fail-closed 却通过" % label)


def test_properties(daemon_dir):
    """五性质：正向 + 反向（每条违例必须 fail-closed）。"""
    # ① 类型律（A×A→C/F）：件名合法标识符 + 源文件真实存在
    expect_ok("type-law: 合法件名/源存在",
              base_data(modules=[{"name": "core", "sources": ["core.c"]}]),
              daemon_dir)
    expect_fail("type-law: 件名非法（大写）",
                base_data(modules=[{"name": "Core", "sources": ["core.c"]}]),
                daemon_dir)
    expect_fail("type-law: 件名非法（数字起首）",
                base_data(modules=[{"name": "1core", "sources": ["core.c"]}]),
                daemon_dir)
    expect_fail("type-law: 件名非法（连字符）",
                base_data(modules=[{"name": "co-re", "sources": ["core.c"]}]),
                daemon_dir)
    expect_fail("type-law: 源文件不存在",
                base_data(modules=[{"name": "core", "sources": ["ghost.c"]}]),
                daemon_dir)

    # ② 地位律（F⊗F 非法）：件名不得与形态面同名，面间只能 ⊙
    expect_fail("status-law: F⊗F 件名=形态面",
                base_data(modules=[{"name": "ingress",
                                    "sources": ["core.c"]}]),
                daemon_dir)

    # ③ 闭合律（结果仍为 C/F）：⊙ 目标须为合法 daemon 名且非自依赖
    expect_ok("closure-law: 合法编排目标",
              base_data(deps={"required": ["llm_d"], "optional": []}),
              daemon_dir)
    expect_fail("closure-law: 编排目标非法 daemon 名",
                base_data(deps={"required": ["NotADaemon"]}),
                daemon_dir)
    expect_fail("closure-law: 编排目标缺 _d 后缀",
                base_data(deps={"optional": ["llm"]}),
                daemon_dir)
    expect_fail("closure-law: 自依赖",
                base_data(deps={"required": ["probe_d"]}),
                daemon_dir)
    expect_fail("closure-law: deps 分组非数组",
                base_data(deps={"required": "llm_d"}),
                daemon_dir)

    # ④ 结合律（(a⊗b)⊗c = a⊗(b⊗c)）：结果为操作数无序并集，分组不改判定
    operands = [{"name": "core", "sources": ["core.c"]},
                {"name": "extra", "sources": ["extra.c"]},
                {"name": "third", "sources": ["third.c"]}]
    perms = [
        [0, 1, 2],
        [2, 1, 0],
        [1, 2, 0],
        [1, 0, 2],
        [0, 2, 1],
        [2, 0, 1],
    ]
    for order in perms:
        expect_ok("assoc-law: 分组置换 %s" % order,
                  base_data(modules=[operands[i] for i in order]),
                  daemon_dir)

    # ⑤ 幂等律（x⊗x = x）：面/格/件名重复装配一律拒绝
    expect_fail("idem-law: 形态面重复",
                base_data(facades=["ingress", "ingress"]),
                daemon_dir)
    expect_fail("idem-law: 能力格重复",
                base_data(slots=["compute", "compute"]),
                daemon_dir)
    expect_fail("idem-law: 原子件名重复",
                base_data(modules=[{"name": "core", "sources": ["core.c"]},
                                   {"name": "core", "sources": ["extra.c"]}]),
                daemon_dir)


def test_identity(daemon_dir):
    """⊙ 单位元：deps 空集为不引入编排的空元，缺省即合法。"""
    expect_ok("unit-law: deps 缺省", base_data(deps={}), daemon_dir)
    expect_ok("unit-law: deps 全空分组",
              base_data(deps={"required": [], "optional": []}),
              daemon_dir)


def test_real_manifests():
    """端到端：现网全部 codegen=true 户在 G14 下恒通过（防误杀）。"""
    manifests = daemon_gen.discover_manifests(None)
    checked = 0
    for manifest in manifests:
        if not daemon_gen.parse_manifest(manifest).get("codegen", True):
            continue
        try:
            daemon_gen.generate(manifest)
        except GenError as exc:
            FAILS.append("real-manifest %s: %s" % (manifest.name, exc))
        else:
            checked += 1
            print("ok   real-manifest: %s" % manifest)
    if checked == 0:
        FAILS.append("real-manifest: 未发现任何 codegen=true 户")
    else:
        print("ok   real-manifest: 共 %d 户通过 G14" % checked)


def main():
    with tempfile.TemporaryDirectory(prefix="g14-algebra-") as tmp:
        src = Path(tmp) / "src"
        src.mkdir()
        for name in ("core.c", "extra.c", "third.c"):
            (src / name).write_text("/* probe */\n", encoding="utf-8")
        test_properties(Path(tmp))
        test_identity(Path(tmp))
    test_real_manifests()

    if FAILS:
        print("\nFAIL: %d 项" % len(FAILS), file=sys.stderr)
        for line in FAILS:
            print("  - %s" % line, file=sys.stderr)
        return 1
    print("\nPASS: G14 组合代数五性质全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
