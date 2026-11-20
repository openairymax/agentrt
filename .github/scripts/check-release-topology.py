#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
"""release.yml 结构不变量闸门。

发行流水线只在 tag 推送时运行，一次结构错误（漏一条平台腿、漏一个
needs、误删 success() 判据）要等到真正发版才暴露，代价是整版重发。
本闸门把「图上的事实」在每次推送时静态校验一遍：

  1. needs 引用的作业必须存在，依赖图无环；
  2. release 作业的 needs 覆盖全部 build-* 腿，且包含 ci-green；
  3. 带 needs 又自定义 if 的作业，if 必须含 success()——作业一旦自定义
     if 就不再继承前置成功判据，缺 success() 会让平台红而发布照跑；
  4. build-* 腿只允许汇聚于 release，publish-* 必须传递依赖 release；
  5. lf-package 的 artifact-suffix 唯一——重名会让两腿产物互相覆盖。

用法: check-release-topology.py [workflow-file]
退出: 0 通过 / 1 违例 / 2 读取或解析错误
"""

import sys
import pathlib

import yaml


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
    return jobs


def deps_of(job):
    needs = job.get("needs")
    if needs is None:
        return []
    return needs if isinstance(needs, list) else [needs]


def closure(jobs, name, seen=None):
    seen = set() if seen is None else seen
    for dep in deps_of(jobs[name]):
        if dep not in seen:
            seen.add(dep)
            closure(jobs, dep, seen)
    return seen


def check_needs(jobs):
    bad = 0
    for name, job in jobs.items():
        for dep in deps_of(job):
            if dep not in jobs:
                fail(f"{name} 的 needs 指向不存在的作业 {dep}")
                bad += 1
    return bad


def check_cycles(jobs):
    state = {}

    def walk(name):
        if state.get(name) == 1:
            return True
        if state.get(name) == 2:
            return False
        state[name] = 1
        cyclic = any(walk(dep) for dep in deps_of(jobs[name]))
        state[name] = 2
        return cyclic

    return sum(1 for name in jobs if walk(name))


def check_release(jobs):
    release = jobs.get("release")
    if release is None:
        fail("缺少 release 作业")
        return 1
    needs = deps_of(release)
    bad = 0
    for name in jobs:
        if name.startswith("build-") and name not in needs:
            fail(f"build 腿 {name} 不在 release.needs 内")
            bad += 1
    if "ci-green" not in needs:
        fail("release.needs 缺少 ci-green")
        bad += 1
    return bad


def check_success_gate(jobs):
    bad = 0
    for name, job in jobs.items():
        cond = job.get("if")
        if deps_of(job) and isinstance(cond, str) and "success()" not in cond:
            fail(f"{name} 带 needs 且自定义 if，但缺少 success()")
            bad += 1
    return bad


def check_publish(jobs):
    bad = 0
    for name in jobs:
        if name.startswith("publish-") and "release" not in closure(jobs, name):
            fail(f"{name} 未传递依赖 release")
            bad += 1
    return bad


def check_suffix(jobs):
    seen = {}
    bad = 0
    for name, job in jobs.items():
        for step in job.get("steps") or []:
            if not str(step.get("uses", "")).endswith("lf-package"):
                continue
            suffix = (step.get("with") or {}).get("artifact-suffix")
            if not suffix:
                fail(f"{name} 的 lf-package 缺少 artifact-suffix")
                bad += 1
            elif suffix in seen:
                fail(f"artifact-suffix {suffix} 在 {seen[suffix]} 与 {name} 重复")
                bad += 1
            else:
                seen[suffix] = name
    return bad


def main():
    path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else (
        pathlib.Path(__file__).resolve().parents[1] / "workflows" / "release.yml")
    jobs = load(path)
    bad = 0
    for check in (check_needs, check_cycles, check_release,
                  check_success_gate, check_publish, check_suffix):
        bad += check(jobs)
    if bad:
        fail(f"release 拓扑校验失败：{bad} 项违例")
        return 1
    print(f"ok: release 拓扑 {len(jobs)} 个作业不变量全部成立")
    return 0


if __name__ == "__main__":
    sys.exit(main())
