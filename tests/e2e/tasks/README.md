# L-8 端到端任务闭环回归（tests/e2e/tasks）

针对 0.1.21 L-8 需求（`docs-closed/agent-workload-dev/0.1.21-架构改进方案.md`）的
测试面：七条用例全部经 gateway JSON-RPC `task.submit` 发起，验证任务从计划、
工具调用、verifier 到终态/账本/作业审计的完整闭环。测试不桩实现、不直连内部
实现细节，只消费对外契约（RPC 响应、`hall.board`、`hall.replay`、`tool.jobs`、
`/jobs show`、`agent.turns` 检查点）。

## 文件

| 文件 | 职责 |
| --- | --- |
| `run_tasks.sh` | 入口：`--profile ci\|runtime`、`--shard 1\|2\|3`；汇总与退出码 |
| `lib_tasks.sh` | 沙箱（`mktemp -d` AIRY_HOME）、九守护进程栈启停、mock 控制、RPC/断言/证据助手、包管理器适配器、`chk_case` 通用核对、`td_gate` |
| `case_faults.sh` | 五类故障注入：`schema` `unknown` `netbreak` `deny` `restart` |
| `case_install.sh` | 真实安装基线：`firefox` `tencent-meeting`（cli/tui 双端同源） |
| `fixtures/mock_llm.py` | 可控 mock LLM：text/install/tool/schema/unknown/deny/fail500/empty/hang 九种模式，可返回 `tool_calls` 驱动真实 `shell_run` |
| `fixtures/model.yaml.in` | 模型配置模板（`__PORT__` 注入 mock 端口） |
| `fixtures/permission_rules.yaml` | 工具许可规则（含 `allow_dangerous` 放行路径的对照规则） |
| `fixtures/build_offline_repo.sh` | 现场构建离线源：`dpkg-deb` 真实小包 + `Packages.gz`；有 `rpmbuild` 时同时构建 noarch rpm |

## 运行

```bash
# CI 档：五类故障注入按 shard 轮转（2/2/1），SKIP 视为失败
bash tests/e2e/tasks/run_tasks.sh --profile ci --shard 1
bash tests/e2e/tasks/run_tasks.sh --profile ci --shard 2
bash tests/e2e/tasks/run_tasks.sh --profile ci --shard 3

# runtime 档：全量七例（含真实包安装）
bash tests/e2e/tasks/run_tasks.sh --profile runtime
```

退出码：`0` 全绿；`1` 存在失败（CI 档出现 SKIP 也计失败）；`10` 环境不可用
（打印 `gate:runtime` 与手工构建/前置步骤）。构建产物路径可经
`AIRY_L8_BIN` / `AIRY_L8_CLI` 覆盖（默认 `/tmp/rt21`）。

## 用例清单

| 用例 | 注入 | 期望 |
| --- | --- | --- |
| `schema` | tool_call 参数违反 schema（缺 `command`） | `failed`；message 含字段级原因；`failure.reason=tool_fuse` |
| `unknown` | tool_call 引用不存在的工具名 | `failed`（非 SUCCESS）；目录撤销后报 `not available in this run`；`tool_fuse` |
| `netbreak` | 工具执行中断网（mock 停摆）+ 500/空正文 | `paused`（可重试失败帧 `provider_unreachable`、`retryable=true`、保留续跑令牌）；恢复后可完成任务 |
| `deny` | 高危 `shell_run` 不带逐次授权（D-9 fail-closed） | `failed`；`failure.origin=tool`、`reason=approval_denied`；作业/回放保留 `requires approval` |
| `restart` | 工具执行中 `SIGKILL` agent_d 并重启 | 遗留任务迁为 `paused`（检查点保留令牌）；`resume_token` 续跑至 `completed`，副作用只执行一次 |
| `firefox` | 真实安装 firefox 夹具包（cli + tui） | `completed`；verifier 经包管理器查询判通过；双端同源 |
| `tencent-meeting` | 真实安装腾讯会议夹具包（cli + tui） | 同上 |

## 断言清单（每条用例）

`chk_case` 统一核对，任一不满足即 FAIL：

1. `task_id` 存在且格式合法；
2. `plan_id` 合法或显式 `plan_unavailable`；
3. ≥1 次真实工具调用（`tool.jobs` 中 `created_ms >= 用例起点` 的 `shell_run`）；
4. verifier 逐条结果（`failed` 用例只要求未判通过）；
5. 终态与预期一致（`hall.board` 该 task_id 的状态）；
6. token/轮次预算不越界；
7. `/jobs show` 能还原时间线（含工具、终态、退出码/错误）；
8. `hall.board` 出现该 task_id 且为终态；
9. `hall.replay(task_id)` 含 `progress` / `result` / `issue` 三类事件；
10. **`completed` 必须 `verifier.ok=true`**——「succeeded 但 verifier 未过」判 FAIL。

故障用例额外断言：`schema` 字段级原因与 `tool_fuse`；`unknown` 目录不可用；
`netbreak` 断网前已完成 ≥1 次真实调用、断网后 `retryable=true` 且检查点保留
`resume_token`；`deny` 归因 `tool/approval_denied` 与「requires approval」；
`restart` 停机后 `paused`、续跑无重复执行（副作用标记恰一行）。

安装用例额外断言：载荷真实落在私有根、`shell_run` 审计可见包管理器命令
（证明安装由任务链执行而非测试脚本代劳）、cli/tui 两端 task_id 各一且事件
类别与终态同构、TUI 静态门禁 `TD1 PASS`。

## 排程

- **ci**：`schema/unknown/netbreak/deny/restart` 按 `i % 3 == shard-1` 轮转，
  每 shard 约 1/3（2/2/1）；不允许 SKIP。
- **runtime**：五类故障注入 + 两条真实安装基线，共七例；安装前置不足时显式
  SKIP 并写明原因，SKIP 不计为通过。

## 离线源与包管理器适配

安装动作只由任务链经 `shell_run` 真实执行，测试脚本只做目录准备与断言：

- **有 dnf + rpm（285H runtime）**：`fixtures/build_offline_repo.sh` 用
  `rpmbuild` 现场构建 noarch rpm（及 repodata），`shell_run` 内以
  `dnf --installroot=<私有根> --repofrompath=l8,file://…` 安装，verifier 用
  `rpm --root=<私有根> -q <pkg>`。
- **无 dnf（本机 Ubuntu 26.04）**：`dpkg-deb` 现场构建真实 `.deb` 小包并生成
  本地离线仓库，`shell_run` 内以 `DPKG_ADMINDIR`/`--instdir` 指向
  `$AIRY_HOME/ws` 下私有根安装，verifier 用 `dpkg -s <pkg>`；宿主
  `/var/lib/dpkg` 与 `/usr` 不被触碰。
- **两者皆缺**：显式 SKIP 并写明原因；CI 档不含安装用例，故 CI 永不 SKIP。

## 双端同源

动态用例以 `client=cli` 与 `client=tui` 对同一 submit 内容各发一次：`schema`、
`unknown`、`deny` 三条故障用例与 `firefox`、`tencent-meeting` 两条安装基线均断言
两端 task_id 各一且互不相同、`hall.replay` 事件类别同构、`hall.board` 终态同构、
失败归因（`failure.reason`）同构。

`netbreak` / `restart` 未做双端二次提交：两者的终态由中断注入位置（断网时刻、
SIGKILL 时刻）决定，属状态性注入；`restart` 的续跑令牌只能消费一次，二次提交会
变成重复执行而非同构验证。client 标签不参与引擎裁决（同一 gateway 路径、同一
`agent_task_decide_mode`），故这两条以单端终态 + 检查点令牌断言覆盖。

「TUI 不得直发 agent.run」属执行面静态约束：TUI 的 agent.run 调用点若存在，只有
真实 TUI 进程才会触发，mock 运行时无法证明。因此复用
`scripts/verify_release_gates.sh` 的 `[组TD] TD1`（扫描 TUI 源树确认零
`agent.run`）作为静态证据，输出落 `$AIRY_HOME/evidence/td-gate.log`，与双端
同源的动态断言共同构成判据①。

## 证据

- 每个用例：`$AIRY_HOME/cases/<case>/evidence/*.json`（响应原始 JSON、`/jobs show`、
  `hall.board`、`hall.replay`、`agent.turns` 等）；
- 汇总：`$AIRY_HOME/evidence/summary.txt`；
- TUI 门禁：`$AIRY_HOME/evidence/td-gate.log`；
- 单用例也可独立运行：`bash tests/e2e/tasks/case_faults.sh netbreak`。

## 已知环境限制

- 本机（Ubuntu 26.04）无 `dnf`/`rpm`，dnf 分支仅经代码路径与夹具脚本覆盖，
  未在本机实跑；285H runtime 需以 `--profile runtime` 验证该分支。
- `mode=task` 的 verifier 依赖 tool_d 可达；verifier 不可用时终态为
  `unverified`，本测试面按「不得冒充 completed」断言。
- 守护重启用例以 `SIGKILL` 注入，检查点排水期约 1~2s，用例已按轮询等待。

## 本轮实测（2026-11-18，本机 Ubuntu 26.04）

```bash
bash tests/e2e/tasks/run_tasks.sh --profile ci --shard 1   # schema+deny      PASS=63  FAIL=0 SKIP=0
bash tests/e2e/tasks/run_tasks.sh --profile ci --shard 2   # unknown+restart  PASS=50  FAIL=0 SKIP=0
bash tests/e2e/tasks/run_tasks.sh --profile ci --shard 3   # netbreak         PASS=36  FAIL=0 SKIP=0
bash tests/e2e/tasks/run_tasks.sh --profile runtime        # 全量七例         PASS=225 FAIL=0 SKIP=0
```

（runtime：schema 31 / unknown 31 / netbreak 36 / deny 32 / restart 19 /
firefox 38 / tencent-meeting 38；退出码均为 0，证据根
`/tmp/airy-l8.*/evidence`。）

## 本轮产品修复（≤50 行插入，系统性根因）

| 文件 | 根因 | 证据 |
| --- | --- | --- |
| `daemons/agent_d/src/run/agent_task_contract.h`、`agent_run_rpc.c`、`agent_run_internal.h`、`agent_run_loop.c` | `task.submit` 无法透传 `allow_dangerous`，高危 `shell_run` 被 D-9 无条件拒绝，任务链无法执行任何命令 | `deny` 用例修复前全部工具调用 EPERM |
| `agent_run_exec.c` | 成功任务不写 `issue` 事件，`hall.replay` 缺 issue 类 | 安装/恢复用例 replay 类别断言 |
| `agent_run_engine.c`、`agent_run_loop.c`、`tests/test_run_loop.c` | 工具熔断被映射为 `paused`（与 `retryable=false` 矛盾） | `schema`/`unknown` 终态断言 |
| `agent_run_loop.c` | provider 类可重试失败（`paused`）删除检查点令牌，`paused` 实际不可续跑 | `netbreak` 检查点令牌断言 |
| `agent_verify.c` | verifier 端点用相对 socket 路径导致探活恒失败 | 安装/恢复用例 verifier.ok |
| `gateway/.manifest` | 缺 `jobs`/`job` 能力，`/jobs show` 与 jobs 审计视图不可用 | 各用例 `/jobs show` 断言 |
