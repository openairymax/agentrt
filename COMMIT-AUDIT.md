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
| `agentrt` | 2132 | 571 | 48 | 259 | 3 | 0 | 48 |
| `agentrt/daemons` | 835 | 45 | 14 | 19 | 0 | 0 | 10 |

图纸 §16.20 的全量普查（50 仓 / 14,356 提交）与本表口径一致；本表为治理目标仓的
当期切片，脚本无随机性，同一 HEAD 复跑必得同一组键值。`total` 随提交单调增长属正常；
存量违规按「只减不增」清理——每销一条，对应计数下降且不得回升，新增提交必须全绿。

## 3. P1 内部路径泄露

两个治理仓 `path_leak=0`，无需脱敏。全量普查中的唯一命中是 `agent-workload` 仓的
`506b056`：subject 为一条写入 `/tmp/trae-*` 任务目录的 shell 命令。处置方式是
**从后续提交起禁止**（`commit-rules.sh` 的 R9），不对历史做改写。

## 4. P2 机器署名

`agentrt` 历史中存在 3 条 `Signed-off-by: trae-solo-agent <solo-agent@trae.ai>`：
`c423428`、`70bd163`、`a0ac382`（`git log --grep='solo-agent@trae.ai'` 可复现）。
该类署名违反 DCO 的「自然人」要件：代理无法承担版权与责任。处置：

- 历史提交保留原样，登记于此，不追溯改写；
- `commit-msg` 钩子与 CI 门禁 `g47` 对新增提交一律拒绝机器身份（R2）；
- 发布自动化账号（`AgentRT Release`、`agentrt-bot`）仅用于打标签与发行提交，
  不得出现在 `Signed-off-by` 中（R2）。
- 0.1.21 发行快照 `008d62b3`（`release: update manifest.stable.json …`，
  agentrt-bot 提交、无 Signed-off-by）由 `g47` 首轮审计暴露：发布机械提交此前
  没有自然人署名通道。根修在 tools 仓 `publish-release.sh`（`ae0aa9d`）：
  发布通道配置 `AIRY_SNAP_NAME/EMAIL`（发布操作者）后，快照以操作者身份
  `-s` 落署名；该历史提交保留不改写。

## 5. P3 身份变体

`agentrt` 现存 8 个作者身份对，其中同一自然人的变体共 5 个：

| 规范身份 | 变体（附代表提交） |
|---|---|
| `Liren Wang <wangliren@spharx.cn>` | `Liren_Wang`（`31c6796`）、`LirenWang`（`58bed4c`） |
| `Zhixian Zhou <zhouzhixian@spharx.cn>` | `lidecheng <zhouzhixian@spharx.cn>`（`eee6eb3`） |

处理方式是 `.mailmap` 归并：只影响 `git log` / `git blame` / `git shortlog` 的显示，
不改变任何提交对象及其 SHA。CI 的 `g47` 对新增提交执行同一规则（R7）。
`agentrt-bot <release@agentrt.airymax.io>` 为发布账号，保留但不计入自然人身份集。

## 6. P4 测试债样本

`agentrt` 头文件导出符号 757 个，测试树引用 442 个（58.4%），缺口 315 个，
明细见 [`TEST-DEBT.md`](./TEST-DEBT.md)。`daemons` 缺口 3 个，见
[`daemons/TEST-DEBT.md`](./daemons/TEST-DEBT.md)。缺口清单由
`tools/scripts/ci/quality/src/test_debt.py` 生成，可逐条销账。

P4 的混合巨型提交样本为 `ec1164f`（标题「channel service and agent child lifecycle」，
实际跨 7 个子系统、并删除 CI 工作流）与 `7878315`（重构移动，body 自称 183/183 通过、
而 `mem_service_recent` 零覆盖）。

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

## 10. 压实模式判定规范（C-29）

「压实」指删除容器元素后把末尾元素搬到空位（swap-remove）：`a[i] = a[n-1]; n--`。
它是 O(1) 的正确做法，但对**有序容器**会打乱相对顺序。判定不是看写法，而是看被
操作的结构是否承载顺序语义。逐条回答四问，任一问为否即止：

1. **命中点是否真在搬移容器元素**——排除字符串去尾（`s[--n]='\0'`）、边界判断
   （`buf[size-1]`）、排序插入（`a[i]=a[i-1]` 前移）这三类形似而非的写法。
2. **被删位置与末元素之间是否跨过其他元素**——只有跨过才可能改变相对次序。
3. **容器是否有顺序契约**——按下标取值的快照/游标、按写入序或时间序展示的列表、
   范围与前缀语义、分页基线，均视为有序；纯集合（按 key 查找，list 顺序不承诺）为无序。
4. **是否存在绕过容器的外部稳定引用**——保存的下标、区间指针、跨调用游标。

结论只有三种取值，台账不得留空：

| 结论 | 处置 |
|---|---|
| `有序（已修）` | 改 `memmove` 保序，并新增覆盖删除首/中/尾与重载的用例 |
| `无序（安全）` | 保留 swap-remove，补一条「删除后剩余元素仍可查」的用例锁定行为 |
| `假阳性（非压实）` | 记录形态与理由，不改代码、不补用例 |

复跑（只读，任一仓可执行）：

```bash
grep -rnE '\[[a-zA-Z_][a-zA-Z0-9_]*\] *= *[a-zA-Z_][a-zA-Z0-9_>.-]*\[[a-zA-Z_][a-zA-Z0-9_ >.-]*- *1\]' \
  --include='*.c' <仓库>/daemons | grep -v '/tests/'
```

## 11. 压实候选逐条判定台账（C-29，2026-11-17）

7 处 r15 候选全部有结论；另登记 1 处列表外新增候选与 C-28 的已修点。

| # | 位置 | 命中形态 | 结论 | 依据 |
|---|---|---|---|---|
| 1 | `mem_d/src/core/service.c:500`（C-28） | `records[idx..last]` 前移 | `有序（已修）` | 记录下标即写入序，`mem_service_recent` 依赖倒序；已改 `AIRY_MEMMOVE` 并重建索引。用例 `test_recent_order_delete_{head,middle,tail,all}`、`same_second`、`after_reload`（`mem_d/tests/test_service.c:785-926`） |
| 2 | `providers/src/core/http.c:428` | `p[--n]='\0'` | `假阳性（非压实）` | 仅截断 header 行尾空白，未移动容器元素 |
| 3 | `mem_d/src/engine/mem_persist.c:365` | `line[--llen]='\0'` | `假阳性（非压实）` | 去掉 JSON 行尾 `\r`，r15 已判 |
| 4 | `llm_d/src/accounting/cost_tracker.c:86` | `model_pattern[len-1]` | `假阳性（非压实）` | 通配符后缀判断，无删除与搬移 |
| 5 | `llm_d/tests/bench_routing_latency.c:50` | `samples[count-1]` | `假阳性（非压实）` | 已排序样本取最大值，非删除路径 |
| 6 | `tool_d/src/plugin/plugin_discovery.c:84` | `rest[val_len-1]` | `假阳性（非压实）` | 去 value 尾部空白，无元素搬移 |
| 7 | `tool_d/src/builtin/builtin_fs_grep.c:135` | `line[tlen-1]` | `假阳性（非压实）` | 去行尾换行后再匹配，无元素搬移 |
| 8 | `agent_d/src/service/service_child.c:70/109` | `buf_size-1` | `假阳性（非压实）` | 缓冲区边界；同文件 `:219` 的 `paths[pos]=paths[pos-1]` 是插入排序前移，保序，不是 swap-remove |
| 9 | `channel_d/src/book/channel_book.c:149` | `channels[idx]=channels[count-1]` | `无序（安全）` | 通道按 `channel_id` 查找，`channel_service_list` 无顺序契约，无外部下标/游标；补 `test_close_middle_keeps_others`（删除首、删除尾两条分支） |

> 列表外候选 #9 由本轮复跑发现（r15 的 grep 未覆盖 `channel_d`）。它不改变判据结论，
> 但说明「候选清单」本身也需要每次复跑，而不是一次性冻结。

