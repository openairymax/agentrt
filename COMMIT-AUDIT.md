# 提交记录审计与勘误台账

本台账对应 0.1.21 施工项 **L-10 / L-11**（图纸 §16.20）。目的是把「工程纪律」
从口头评价变成可复算的数字，并公开说明历史提交里已经暴露的问题：哪些被修正、
哪些只做显示层归并、哪些明确不改写。

## 1. 口径与复现

普查只读，不改写任何提交对象。任意仓库可复跑，输出固定键值：

```bash
bash tools/scripts/git/commit-survey.sh <仓库路径>
```

| 键 | 定义 |
|---|---|
| `total` | 提交总数（不含 merge） |
| `no_dco` | body 无 `Signed-off-by` |
| `subj_over72` | 主题按字符计超过 72 |
| `no_prefix` | 主题缺 `<subsystem>: ` 前缀（Merge/Revert 不计） |
| `machine_sign` | 署名主体为机器人 / 代理 / `noreply` |
| `path_leak` | 主题或 body 含工具内部路径 |
| `fix_no_hash` | 修复类提交缺 `Fixes:` 或短 hash 不足 12 位 |

## 2. 普查结果（2026-11-17 实跑）

| 仓库 | total | no_dco | subj_over72 | no_prefix | machine_sign | path_leak | fix_no_hash |
|---|---:|---:|---:|---:|---:|---:|---:|
| `agentrt` | 2075 | 571 | 46 | 259 | 3 | 0 | 48 |
| `agentrt/daemons` | 757 | 45 | 13 | 19 | 0 | 0 | 10 |

图纸 §16.20 的全量普查（50 仓 / 14,356 提交）与本表口径一致；本表为治理目标仓的
当期切片，后续每月复跑一次，数字只减不增。

## 3. P1 内部路径泄露

两个治理仓 `path_leak=0`，无需脱敏。全量普查中出现的工具内部路径已在 §16.20 登记，
处置方式是**从后续提交起禁止**（`commit-rules.sh` 的 R9），不对历史做改写。

## 4. P2 机器署名

`agentrt` 历史中存在 3 条 `Signed-off-by: trae-solo-agent <solo-agent@trae.ai>`。
该类署名违反 DCO 的「自然人」要件：代理无法承担版权与责任。处置：

- 历史提交保留原样，登记于此，不追溯改写；
- `commit-msg` 钩子与 CI 门禁 `g47` 对新增提交一律拒绝机器身份（R2）；
- 发布自动化账号（`AgentRT Release`、`agentrt-bot`）仅用于打标签与发行提交，
  不得出现在 `Signed-off-by` 中（R2）。

## 5. P3 身份变体

`agentrt` 现存 8 个作者身份对，其中同一自然人的变体共 5 个：

| 规范身份 | 变体 |
|---|---|
| `Liren Wang <wangliren@spharx.cn>` | `Liren_Wang`、`LirenWang` |
| `zhixianzhou <zhouzhixian@spharx.cn>` | `lidecheng <zhouzhixian@spharx.cn>` |

处理方式是 `.mailmap` 归并：只影响 `git log` / `git blame` / `git shortlog` 的显示，
不改变任何提交对象及其 SHA。CI 的 `g47` 对新增提交执行同一规则（R7）。
`agentrt-bot <release@agentrt.airymax.io>` 为发布账号，保留但不计入自然人身份集。

## 6. P4 测试债样本

`agentrt` 头文件导出符号 757 个，测试树引用 442 个（58.4%），缺口 315 个，
明细见 [`TEST-DEBT.md`](./TEST-DEBT.md)。`daemons` 缺口 3 个，见
[`daemons/TEST-DEBT.md`](./daemons/TEST-DEBT.md)。缺口清单由
`tools/scripts/ci/quality/src/test_debt.py` 生成，可逐条销账。

## 7. 历史不改写

已发行的提交 SHA 是签名、校验和与发行资产的输入，改写会同时破坏：

1. 发行资产 `manifest` 与 `.sha256`/`.sig` 的对应关系；
2. 下游 fork 与镜像的合并基点；
3. `git blame` 与问题追溯链。

因此本项目**不因治理而改写已发行 SHA**。只有在下列三个前置条件同时满足时，
才允许对**未发行**的本地分支做一次性整理：

1. 该提交未被任何发行资产、签名或校验和引用（`git tag --contains` 为空且不在
   `RELEASE_NOTES.d` 记录的发行区间内）；
2. 所有下游分支与镜像已书面确认迁移窗口，并在同一窗口内完成；
3. 已有独立备份标签与可回滚脚本，且回滚路径经过一次真实演练。

## 8. 对后续提交的强制

| 位置 | 手段 | 规则 |
|---|---|---|
| 本地 | `scripts/git/install.sh` 安装的 `commit-msg` 钩子 | R1 / R2 / R3 / R8 / R9 |
| CI | `g47-commit-audit.sh`（`AIRY_COMMIT_BASE..HEAD`） | R1 / R2 / R3 / R5 / R7 / R8 / R9，R4 告警 |
| CI | `g48-test-debt.sh` | 新增导出符号必须有测试引用（R6） |

`AIRY_COMMIT_GATE=off` 只跳过本地钩子，CI 不读取该变量——门禁不可由提交者关闭。

## 9. 维护

- 新增提交由 CI 自动判定，无需人工登记；
- 本表在每月首次复跑普查后更新，数字变化必须与当月提交对上；
- 发现新的公开面问题，追加到对应 P 段并在 `Fixes:` 中引用本台账条目。
