#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
"""发行流水线结构不变量闸门（R26/R29 三段式）。

发行流水线只在 tag 推送时运行，一次结构错误（漏一条平台腿、漏一个
needs、误删 success() 判据）要等到真正发版才暴露，代价是整版重发。
自 0.1.21 起发行面拆为三段（构建矩阵 / 打包签名 / 发布），本闸门在
每次推送时把「图上的事实」静态校验一遍：

  1. 编排器 release.yml 必须按 build → package → publish 串联三个
     workflow_call 阶段文件，且阶段文件都存在并声明 workflow_call；
  2. 每个阶段文件作业数 ≤5（R29），needs 引用存在、依赖图无环；
  3. 构建段必须且只能产出九平台制品后缀（矩阵腿从 matrix.suffix 取值
     并逐条核对），发布段不得出现 build-* 作业，反之亦然；
  4. package 段必须有 release 作业（跨段门禁由编排器的 needs 表达），
     全部 publish-* 作业必须落在 publish 段；
  5. 带 needs 又自定义 if 的作业，if 必须含 success()——作业一旦自定义
     if 就不再继承前置成功判据，缺 success() 会让平台红而发布照跑。

用法: check-release-topology.py [workflows-dir|workflow-file]
退出: 0 通过 / 1 违例 / 2 读取或解析错误
"""

import sys
import pathlib

import yaml

ORCH = "release.yml"
STAGES = ("build", "package", "publish")
PLATFORMS = {
    "linux-x86-64", "linux-riscv-64", "linux-x86-32",
    "linux-arm-32", "linux-arm-64",
    "macos-arm-64", "macos-x86-64",
    "windows-x86-64", "windows-x86-32",
}
MAX_JOBS = 5


def fail(msg):
    print(f"::error::{msg}", file=sys.stderr)


def load(path):
    try:
        with open(path, encoding="utf-8") as fh:
            doc = yaml.safe_load(fh)
    except (OSError, yaml.YAMLError) as exc:
        fail(f"无法解析 {path}: {exc}")
        sys.exit(2)
    jobs = doc.get("jobs") if isinstance(doc, dict) else None
    if not isinstance(jobs, dict) or not jobs:
        fail(f"{path} 缺少 jobs")
        sys.exit(2)
    return doc, jobs


def deps_of(job):
    needs = job.get("needs")
    if needs is None:
        return []
    return needs if isinstance(needs, list) else [needs]


def check_needs(name, jobs):
    bad = 0
    for job, body in jobs.items():
        for dep in deps_of(body):
            if dep not in jobs:
                fail(f"{name}: {job} 的 needs 指向不存在的作业 {dep}")
                bad += 1
    return bad


def check_cycles(name, jobs):
    state = {}

    def walk(job):
        if state.get(job) == 1:
            return True
        if state.get(job) == 2:
            return False
        state[job] = 1
        cyc = any(walk(dep) for dep in deps_of(jobs[job]))
        state[job] = 2
        return cyc

    bad = 0
    for job in jobs:
        if walk(job):
            fail(f"{name}: 依赖图存在环，路径经过 {job}")
            bad += 1
    return bad


def check_size(name, jobs):
    if len(jobs) > MAX_JOBS:
        fail(f"{name}: 作业数 {len(jobs)} 超过上限 {MAX_JOBS}（R29）")
        return 1
    return 0


def check_success(name, jobs):
    bad = 0
    for job, body in jobs.items():
        cond = body.get("if")
        if deps_of(body) and isinstance(cond, str) and "success()" not in cond:
            fail(f"{name}: {job} 带 needs 且自定义 if，但缺少 success()")
            bad += 1
    return bad


def check_prefix(name, jobs, prefix, allowed=True):
    bad = 0
    for job in jobs:
        if job.startswith(prefix) != allowed:
            fail(f"{name}: 不应出现的作业 {job}")
            bad += 1
    return bad


def stage_file(uses):
    if not isinstance(uses, str) or not uses.startswith("./.github/workflows/"):
        return None
    return uses.rsplit("/", 1)[-1]


def check_orch(doc, jobs):
    bad = 0
    for stage in STAGES:
        job = jobs.get(stage)
        if job is None:
            fail(f"{ORCH}: 缺少编排作业 {stage}")
            bad += 1
            continue
        if not stage_file(job.get("uses")):
            fail(f"{ORCH}: {stage} 必须 uses 本仓 workflow_call 文件")
            bad += 1
    for prev, nxt in zip(STAGES, STAGES[1:]):
        if prev in jobs and nxt in jobs:
            if prev not in deps_of(jobs[nxt]):
                fail(f"{ORCH}: {nxt}.needs 缺少 {prev}（阶段不可乱序/并行）")
                bad += 1
    bad += check_needs(ORCH, jobs) + check_cycles(ORCH, jobs)
    bad += check_success(ORCH, jobs)
    return bad


def on_keys(doc):
    """PyYAML 把裸 on: 解析为布尔 True，两种键都要认。"""
    val = doc.get(True, doc.get("on"))
    if isinstance(val, str):
        return [val]
    if isinstance(val, dict):
        return [str(k).lower() for k in val]
    return []


def check_callable(name, doc):
    if "workflow_call" not in on_keys(doc):
        fail(f"{name}: 阶段文件必须以 workflow_call 触发")
        return 1
    return 0


def pkg_suffixes(jobs):
    """收集制品平台后缀：lf-package 的 suffix、矩阵 include 的 suffix、
    以及 Windows 内联打包脚本里 $artifact 的 windows-* 字面量。"""
    import re
    seen, bad = {}, 0
    tok = re.compile(r"(?:linux|macos|windows)-[a-z0-9]+(?:-[a-z0-9]+)*")

    def add(value, job):
        nonlocal bad
        if value in seen:
            if seen[value] != job:
                fail(f"suffix {value} 在 {seen[value]} 与 {job} 重复")
                bad += 1
        else:
            seen[value] = job

    for job, body in jobs.items():
        for step in body.get("steps") or []:
            if str(step.get("uses", "")).endswith("lf-package"):
                with_ = step.get("with") or {}
                suffix = with_.get("artifact-suffix")
                if isinstance(suffix, str) and "matrix.suffix" in suffix:
                    matrix = ((body.get("strategy") or {}).get("matrix") or {})
                    for entry in matrix.get("include") or []:
                        value = entry.get("suffix")
                        if not value:
                            fail(f"{job}: matrix include 缺少 suffix")
                            bad += 1
                        else:
                            add(value, job)
                elif suffix:
                    add(suffix, job)
                else:
                    fail(f"{job}: lf-package 缺少 artifact-suffix")
                    bad += 1
            for value in tok.findall(str(step.get("run", ""))):
                # 仅 Windows 腿内联打包（无 lf-package），其余腿由上面的
                # lf-package/矩阵取值覆盖；避免脚本文案误报重复。
                if value.startswith("windows-") and value in PLATFORMS:
                    add(value, job)
    return seen, bad


def check_build(name, jobs):
    bad = check_prefix(name, jobs, "publish-", allowed=False)
    seen, extra = pkg_suffixes(jobs)
    bad += extra
    missing = PLATFORMS - set(seen)
    extra_sfx = set(seen) - PLATFORMS
    for sfx in sorted(missing):
        fail(f"{name}: 缺少平台制品后缀 {sfx}")
        bad += 1
    for sfx in sorted(extra_sfx):
        fail(f"{name}: 未知平台制品后缀 {sfx}")
        bad += 1
    return bad


def check_pkg(name, jobs):
    bad = 0
    if "release" not in jobs:
        fail(f"{name}: 缺少 release 聚合作业")
        bad += 1
    bad += check_prefix(name, jobs, "build-", allowed=False)
    bad += check_prefix(name, jobs, "publish-", allowed=False)
    return bad


def check_pub(name, jobs):
    bad = 0
    if not any(job.startswith("publish-") for job in jobs):
        fail(f"{name}: 缺少 publish-* 作业")
        bad += 1
    bad += check_prefix(name, jobs, "build-", allowed=False)
    if "release" in jobs:
        fail(f"{name}: publish 段不应重现 release 聚合作业")
        bad += 1
    return bad


def main():
    arg = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else None
    here = pathlib.Path(__file__).resolve().parents[1] / "workflows"
    wfdir = (arg.parent if arg and arg.is_file() else arg) if arg else here

    docs, alljobs = {}, {}
    total = 0
    for stage in (ORCH,) + tuple(f"release-{s}.yml" for s in STAGES):
        doc, jobs = load(wfdir / stage)
        docs[stage] = doc
        alljobs[stage] = jobs
        total += len(jobs)

    bad = check_orch(docs[ORCH], alljobs[ORCH])
    for stage in STAGES:
        fname = f"release-{stage}.yml"
        doc, jobs = docs[fname], alljobs[fname]
        bad += check_callable(fname, doc)
        bad += check_size(fname, jobs)
        bad += check_needs(fname, jobs)
        bad += check_cycles(fname, jobs)
        bad += check_success(fname, jobs)
        if stage == "build":
            bad += check_build(fname, jobs)
        elif stage == "package":
            bad += check_pkg(fname, jobs)
        else:
            bad += check_pub(fname, jobs)

    if bad:
        fail(f"发行流水线拓扑校验失败：{bad} 项违例")
        return 1
    print(f"ok: 发行三段拓扑 {total} 个作业不变量全部成立")
    return 0


if __name__ == "__main__":
    sys.exit(main())
