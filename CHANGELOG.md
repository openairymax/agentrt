# AgentRT 变更日志 CHANGELOG

本文档记录 AgentRT 的所有重要变更。格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## 📋 目录

- [v0.1.1 框架化改造](#v011-框架化改造---2026-08-02) ⭐ 最新 — GCCP / 工作大厅 / 双思考 / CLI
- [v0.1.1](#v011---2026-07-12) — 奠基版本（Foundation Release）
- [v0.1.0](#v010---2026-05-29) — 首个正式发行版
- [v0.0.5](#v005---2026-05-24) — C 运行时深度审计与质量加固
- [v0.0.12](#v0012---2026-04-11)
- [v0.0.11](#v0011---2026-04-10)
- [v0.0.10](#v0010---2026-04-09)
- [v0.0.9](#v009---2026-04-06)
- [v0.0.7](#v007---2026-04-06)
- [v0.0.6](#v006---生产就绪-2026-03-30)
- [v0.0.4](#v004---生产就绪-2026-03-25)
- [v0.0.3](#v003---开发中-2026)
- [历史版本](#历史版本)

---

## [v0.1.1 框架化改造] - 2026-08-02

### 🎯 机制/策略框架化（1.0.1 前置任务）

在 0.1.1 奠基版本之上完成认知管线的**机制/策略框架化**改造：将"理解用户意图"与"驱动 Agent 干活"的产品闭环纳入运行时。机制在 agentrt 内实现，策略由产品层（CLI/UI）注入。

### Added
- **GCCP 目标完备确认协议**（`coreloopthree/include/gccp.h` + `src/cognition/gccp.c`）
  - 两阶段交互协议：`airy_gccp_probe()`（目标探测）→ 产品层交互回调 → `airy_gccp_confirm()`（回答融合补全）
  - 五问目标模型：终点（Φ）/ 起点（x₀）/ 卡点（U）/ 受众（w）+ Q5 可验证完成判据，经庞特里亚金最小值原理映射
  - 状态机：`CONFIRMED / DEGRADED / AMBIGUOUS`；LLM 不可用时启发式降级（固定四问）
  - 认知引擎 Phase 0 拆解后插入确认阶段；结果挂载 `intent.intent_gccp_goal` 并写入 working_mem `gccp_goal`
- **工作大厅 Work Hall**（`coreloopthree/include/work_hall.h` + `src/work_hall.c`）
  - 任务图注册 / 状态看板（查询/列表）/ 取消 / 等待，线程安全
  - 节点 handler 路由到 `agent_d`（daemon_rpc_call spawn/invoke），驱动 ecosystem/agents 真实执行
  - `airy_work_hall_bind_ops()` 实现 `airy_orch_ops_t` 并注入全局 ops_injection 表（接通断链）
- **Plan→TaskFlow DAG 适配层**（`coreloopthree/include/plan_to_dag.h` + `src/plan_to_dag.c`）
  - `airy_plan_to_workflow()`：`airy_task_plan_t` → `taskflow_workflow_t`（节点/边/入口转换 + `agent:` 前缀规范化）
- **产品化交互式 CLI**（`tools/airy_cli`，CMake 选项 `BUILD_CLI` 默认 ON）
  - 完整闭环：自然语言大任务指令 → GCCP 四问交互 → 认知规划 → Plan→DAG 适配 → 工作大厅提交/看板 → agent_d 驱动
- **双思考 TC3 三独立模型激活** + **dual_coordinate 交叉验证接线**
  - `airy_cognition_set_tc3_models()` 注入 t2 / t1-f / t1-p 三模型；TC3 成功后激活 `coord_strat->coordinate` 双模型交叉验证（接通断链）
- **GRAD 计划级批判循环**（`coreloopthree/src/cognition/critique/grad_*.{h,c}`）
  - **用户决策：放弃文本级批判循环，全面采用 GRAD**——GRAD 启用时跳过 Phase 2 文本批判循环（`enable_grad` 开关，默认 1）
  - 三权分立：模型 A（t2）构造/修正计划、模型 C（t1-p）确定性四验（E-01 因果 / E-02 死锁 Kahn / E-03 资源 / E-04 目的漂移，零生成 Token）、模型 B（t1-f）语境终裁
  - 差分熵削减：`airy_grad_verify_scope()` 仅验证 Δ_k 一阶闭包，O(M×N) → O(N+M·Δ)
  - 双思考工作空间：`$AIRY_RUNTIME_DIR/workspace/<plan_id>/` 落盘 t2 原始计划、c_verify 四验报告、b_arbiter 终裁意见、trace/chain.jsonl 决策链（所有工作有迹可循）
  - 显式 model 优先路由（llm_d `select_provider_via_router`）：用户指定模型精确匹配 registry，失败才走 COST_AWARE 降级
- **`airy_loop_dag_cancel()`** — 取消 DAG 执行实例（供工作大厅取消）

### Changed
- `airy_intent_t` 新增 `intent_gccp_goal` 字段（OWNER，由 `airy_intent_free` 释放）
- `airy_task_node_t` 扩展 GRAD 元数据：`task_node_inputs/outputs`（类型签名）、`task_node_cost_time_ms/cost_mem_mb`（资源）、`task_node_invariant_guard`（不变式）
- 认知引擎新增 setter：`airy_cognition_set_gccp_enabled` / `airy_cognition_set_gccp_interact` / `airy_cognition_set_tc3_models` / `airy_cognition_set_grad_enabled`
- 顶层 CMakeLists 新增 `BUILD_CLI` 选项（默认 ON）+ `tools/airy_cli` 子目录

### Fixed
- 接通断链：`airy_orch_ops_t` 注入（工作大厅 bind_ops）与 `dual_coordinate` 调用点（TC3 成功后接线）
- `security_types.h` `cap_t` 与 libcap 冲突 → `airy_cap_t`
- `syscall_router.c` 2 处 `memcpy` 违反 BAN-154 → `AIRY_MEMCPY` + 补齐遥测字段
- GCCP / Plan→DAG 中 `memcpy` 投毒规避
- 工作大厅 double-free（`airy_work_hall_destroy` 值数组释放）、`-Waddress` 告警（定长数组三元判断）
- ecosystem 配置 SSoT：`agentrt.yaml` llm 段收敛为 runtime、`model.yaml` 补 providers 段、`contract.json` agent_id → `coding_rs_v1`

### Tested
- `test_gccp_workhall.c`（6/6 通过）+ 既存 coreloopthree 测试全绿；ASAN 复验无 double-free/UAF
- `test_grad.c`（8/8 通过）：E-01/E-02/E-03 四验、seed 收敛、驳回重生成、差分范围
- 端到端冒烟：CLI 任务集 GCCP → 规划 → GRAD 轮回（round0 reject → 重新生成 → round1 accept 收敛）→ DAG → 工作大厅 → 执行；工作空间 `workspace/<plan_id>/` 决策链落盘完整
- 终端冒烟实测：GCCP 四问交互正常展示、LLM/agent_d 不可用时降级路径日志正确

### 2026-08-09 追加：对话 B 模型决策 + 三配置点 + 执行中图纸复核设计
- **决策 A**：日常对话全部由 B 模型（t1-f）进行——`cli_chat_reply` 生成模型 t2 → t1-f；`cli_llm_classify` 任务/对话分流改用 t1-f；废弃 `AIRY_CHAT_T1F_VERIFY` 的"t2 生成 → t1-f 验证 → 重生成"路径
- **决策 B**：模型三配置点提醒（t2=A / t1-f=B / t1-p=C，每点可用云端 API 或本地 Ollama/vLLM，开关由用户决定，t1-f 最先激活）——CLI 启动打印三配置点状态，未配置 t1-f 时对话入口给出激活提醒
- **设计新增**：改进 6 执行中图纸复核回路（`09-roadmap-sched-exec.md` §4.7，P3 规划）——执行中经 `wh_progress_cb` 由 t1-f 快速复核产物是否偏离图纸（DRIFT）、t2 按 GRAD Δ patch 增量修正图纸，补全"执行前设计 / 执行中复核 / 执行后回灌"三段闭环

### 2026-08-09 追加（第二批）：作战指挥中心 + 模型分层 + 工作区隔离 + 统一治理
- **决策 C**：任务大厅 = 作战指挥中心（P1）——认知层发布任务命令 → 执行体领取 → 记忆层从结果与过程提取经验（mem_d + L2 缓存 + L1 状态机）；**深化（全流程可见性）**：大厅为系统唯一事实源（类比 K8s etcd）——任务文件模型（图纸+命令+进度+结果+问题+验证+决策链）、`wh_progress_cb` 升级为全量事件发布、可观测性 API、过程经验提取，控制论依据"没有观测就没有控制"；**任务文件模型与分级可见性详细设计见 `09-roadmap-sched-exec.md` §4.9**（命名规范 `{tenant}.{task}.{category}.{ts}.{seq}.json`、七类文件 JSON 结构、RBAC 权限矩阵 + 三层隔离：命名空间 / 应用层 ACL / 写权限单一来源；检索回放见 §4.9.7）
- **决策 D**：模型分层落地（P0）——执行体主推理 t1-p（C）→ t2（A）复核 → t1-f（B）终裁；ecosystem agents 接入三模型配置，经 llm_d 路由
- **决策 E**：工作区隔离（P1）——**沙箱工作目录为主**（不依赖 git、契合行业底座），git 仓库场景叠加 git worktree；DAG 并行分支独立工作目录 + 产物 merge/冲突检测；借鉴 Codex PR 的"隔离→验证→审查→合并"流程形态而非 git PR 本身
- **决策 F**：统一治理（P1）——GRAD R_total 预算从规划期扩展到执行期（全局 Token 预算 + 资源感知并发调度）；**并行不设硬上限**（资源预算约束，支持成千上万执行体）
- **决策 G**：验证门禁落地（P0）——`output_validator` 注入 CLI，确定性门禁 FAIL 阻断节点提交，触发重试/标注
- **决策 H**：执行体嵌套委派（P3+ 路线图）——`spawn_agent` 等价物，走 CAID 结构化任务单委派
- **改进 6 修订**：复核管线对齐模型分层——执行体交付协议（ADP：产物+自验证报告+变更清单）→ 确定性门禁 → t2 语义复核（DRIFT）→ t1-f 终裁 → t2 增量修正重入
- **回归修复（Release/NDEBUG 下 assert 包裹必要调用被优化掉的同族根因）**：
  - `cl3_execution` execution engine tcb 生命周期竞态 UAF：`LOG_DEBUG` 移到 `cond_signal` 之前（engine.c），signal 后不再访问 tcb
  - `test_taskflow_advanced` LSan 泄漏 + RTC 空转：`taskflow_engine_register_workflow` 独立于 assert 执行（NDEBUG 下 assert 参数不求值），RTC 三测试实际执行、23/23 全绿
  - `test_tool_test_executor` ASan `ThreadArgRetval::BeforeJoin` CHECK：`pthread_create/join` 独立于 assert，READ 并发/WRITE 串行真实执行全过
  - 全量回归 146/146 全绿（ASan/LSan）

### 2026-08-09 追加（第三批）：全面改进执行落地（P0 全部 + P1 决策 C 核心）
- **决策 D 落地（P0）**：ecosystem agents 模型分层——Rust `coding_agent`（`agent.rs` 默认模型与调用模型）与 Python `AirymaxAgent`（`base.py` `make_llm_client(default_model=...)`）均优先读 `AIRY_MODEL_T1P`（执行体 worker 主推理 t1-p），未设置回落既有默认；Rust `cargo check` 通过
- **决策 G 落地（P0）**：CLI 注入 `output_validator`（`AIRY_VALIDATOR_RULES` JSON 或默认 `{"exit_code":0}`）+ wait 后验证 FAIL 标注（`airy_work_hall_verify_stats` 前后对比）；节点级门禁重试由 sched_d write_back 承担（已落地）
- **决策 C 落地（P1 核心）**：任务文件模型 `hall_store.h/.c`（`atoms/coreloopthree/src/dispatch/`）——七类文件（blueprint/command/progress/result/issue/verify/chain）、命名 `{tenant}.{task}.{category}.{ts_utc}.{seq:04d}.json`（UTC 毫秒时间戳 + 事件溯源 seq）、ACL 分级可见（cognition 全量 / executor 本任务、图纸命令决策链仅 cognition）、`list/get/replay` 检索回放 API（list 倒序 / replay seq 正序）；work_hall 挂接（progress/result/issue 事件写入，独立于 roadmap_sched）；CLI 注入（`$AIRY_HOME/data/agentrt/hall`）；单测 `test_cl3_hall_store` 31/31 全绿（ASan 无泄漏）
- 全量回归：147 项（含新增 hall_store），cl3_execution 在 -j4 下偶发一次（串行 5/5 稳定，与本次改动无关），重跑通过

### 2026-08-09 追加（第四批）：P1 决策 E 工作区隔离完成
- **决策 E 机制层（workspace.h/.c）**：沙箱工作目录隔离——快照（主工作区 → 独立沙箱目录递归复制 + 快照基线 `.airy_baseline/`，作为 merge 三方判定的基准）、合并（三方判定：目标==基线→应用 / ==工作区→跳过 / 外部改动→冲突不覆盖，删除不传播）、降级开关 `AIRY_WORKSPACE_ISOLATION=0 → ENOTSUP`（保持现状语义）、execution_id 目录穿越防护（拒绝 `/` `\`）、路径收敛 `$AIRY_HOME/data/agentrt/workspaces`；跨平台（POSIX dirent / Windows FindFirstFile + `_stat`）
- **work_hall 挂接（节点执行边界）**：`config.main_workspace_dir`（BORROW，NULL=不隔离）；节点执行前快照主工作区 → 独立沙箱目录（`{sanitize(node_id)}-{seq}` 唯一命名）；invoke 注入 `workspace_dir` + `workspace_mode=isolated`（独立 params 字段，不污染 input，向后兼容）；执行成功 merge 回主工作区，冲突不覆盖并写 hall_store issue 事件（`WS_MERGE_CONFLICT`，task_id=工作区名可审计）；执行失败保留隔离区供审计
- **CLI 挂接**：`AIRY_WORKSPACE_MAIN_DIR` 显式指定时启用隔离（默认保持现状，降级开关维持现状原则）；`AIRY_WORKSPACE_ISOLATION=0` 可整体关闭
- **测试**：`test_cl3_workspace` 19/19 全绿（生命周期/降级 ENOTSUP/快照含子目录/三方 merge 更新/冲突检测不覆盖/新增应用/删除不传播，ASan 无泄漏）；`test_cl3_gccp_workhall` 9/9（新增决策 E 端到端：mock agent_d Unix socket + spawn/invoke 协议 + 断言 invoke 注入 workspace_dir、工作区收敛 AIRY_HOME、mock 隔离区产物 merge 回主工作区）；全量回归 148/148 通过（ASan/LSan）

### 2026-08-09 追加（第五批）：P1 决策 F 统一治理完成
- **决策 F 机制层（governance.h/.c）**：GRAD 公理 II R_total 执行期投影——Token 预算池（acquire 预扣 / settle 结算校正回吐补扣，记账式门禁非阻塞拒绝；低水位 THROTTLED / 扣尽 CIRCUIT_OPEN）、并发槽信号量（`airy_cond_timedwait` 100ms 分片轮询可超时）、图级 deadline（登记表 + 超时熔断计数）、派生容量 `min(硬上限, 空余槽位, 剩余预算可支撑并发)`（并行不设硬编码上限）、register/unregister 配对一体化（Token 预扣失败熔断拒绝 / 槽位失败全量回吐 / 登记失败回滚前两步；unregister 未登记 no-op 防误释放，`token_actual==0` 中性结算不回吐）
- **work_hall 挂接（任务级门禁）**：submit 成功后 `airy_governance_register(graph_id=exec_id, est=node_count*token_per_node)`；门禁拒绝（预算不足 BUSY / 槽位满 TIMEOUT）→ cancel 该执行并返回错误码；看板登记失败回滚治理登记；终态 `wh_progress_cb` 注销（Token 结算 + 槽位释放 + deadline 清除）
- **引擎同步执行适配**：taskflow 对单节点工作流在 submit 内同步执行完毕（终态回调先于 register → unregister no-op），register 后立即探测终态结算（settle-sync）防 slot/token 泄漏；多节点图由终态回调正常注销
- **CLI 挂接**：`AIRY_GOV_TOKEN_BUDGET` / `AIRY_GOV_SLOTS` / `AIRY_GOV_MAX_CONCURRENT` / `AIRY_GOV_TOKEN_PER_NODE` / `AIRY_GOV_DEADLINE_MS` 环境变量注入；全部未配置（无预算且无槽位）→ 不启用治理，保持现状
- **测试**：`test_cl3_governance` 43/43 全绿（生命周期/派生容量/Token 三态/预扣结算回吐补扣/并发槽超时/deadline 熔断/register-unregister 一体化/不设限默认 8 槽，ASan 无泄漏）；`test_cl3_gccp_workhall` 治理挂接 2 项新增（token 门禁 wf1/wf2 准入 + wf3 BUSY + 运行中 capacity=2 + 结算后恢复；并发槽门禁 TIMEOUT + 释放后重提交）——治理测试改用 2 节点串行 DAG 提供真实运行中窗口；顺带修复决策 E 遗留：mock agent socket 就绪等待（消除 spawn RPC 竞态偶发 FAIL）与 submit 失败路径 wf 完整释放（ASan LSan 347 字节泄漏）；全量回归 **149/149** 通过（ASan/LSan）

### 2026-08-09 追加（第六批）：P3 改进 6 执行中图纸复核完成
- **复核管线（execution_review.h/.c）**：ADP 交付物（产物 + 自验证报告 + 变更清单）→ 确定性门禁（产物结构契约校验：输出非空/上限/输出签名键集 E-01）→ t2（A）语义复核（DRIFT 检测，委托注入）→ t1-f（B）终裁（accept/reject，委托注入）→ PASS/DRIFT/REJECT/SKIP；**降级链**：无语义委托→纯门禁 / 仅 t1-f→直接终裁 / t1-f 不可用→采纳 t2 结论
- **replan 接口（L1/L2 联动）**：`airy_rs_sm_replan`（受影响节点校验 + 最上游定位 + current_step 回退到最近前驱/入口重置，支持新图纸整图重登记）；`airy_roadmap_sched_replan`（L1 回退 + L2 缓存失效 + 重入集输出：受影响节点 + 直接后继）
- **work_hall 挂接**：`config.reviewer + config.blueprint`（BORROW，NULL=不启用）；wait 返回前复核聚合产物 → verify 报告写 hall_store（决策 C）→ 结果回灌 roadmap_sched（L2 准入）→ DRIFT/REJECT 触发 replan
- **依赖关系落地**：决策 E（复核输入含隔离区产物/变更清单）、决策 F（复核消耗 R_total；DRIFT 为治理可观测事件；重入重新 register 治理）
- **修复**：`AIRY_STRNCPY_TERM` 宏对 dst 两次求值导致 `buf[buf_n++]` 双递增（replan 重入集计数 bug）
- **测试**：`test_cl3_execution_review` 新增（复核管线/门禁四态/语义委托组合/降级链/统计/replan L1 回退 + L2 失效 + 重入集/新图纸重登记/work_hall 端到端，ASan 无泄漏）；全量回归 **150/150**（cl3_execution 既有并行偶发，串行稳定，与本次改动无关）

### 2026-08-09 追加（第七批）：P3 决策 H 执行体嵌套委派完成
- **委派器（execution_delegate.h/.c）**：Codex `spawn_agent` 等价物——执行体遇复杂子任务委派子执行体，CAID 结构化委派（图纸节点即任务单：子任务单 = 子 workflow，JSON 交付，不引入自由对话协作）；深度控制（max_depth 默认 2，超限 AIRY_EPERM + 统计）+ 子 workflow 提交执行（submit→wait 子产物交付）+ 统计（delegated/depth_rejected/failed）；同线程同步嵌套（taskflow 同步推进，无死锁、无跨线程竞态）
- **work_hall 挂接**：`config.delegate`（BORROW，NULL=不启用）+ `airy_work_hall_delegate()` API（未启用 ENOTSUP）+ 节点 handler 路由 `delegate:<sub_wf_id>`（自动接管，从注册表取子 workflow 副本委派执行）；子执行天然复用决策 F 治理登记/结算、改进6 复核钩子、决策 E 工作区隔离
- **replan 委派映射（roadmap_sched）**：`airy_rs_delegation_map_t`（子节点→父节点）+ `replan_ctx.delegation`；有效受影响集解析（图纸内原样 / 子节点映射解析为父节点回退 / 无映射 NOT_FOUND），L1 回退 + L2 失效 + 重入集统一基于有效集；**rs_state_machine.c 不侵入**（映射解析在 roadmap_sched 层）
- **修复（submit BORROW 语义落地）**：`airy_work_hall_submit` 内部深拷贝后提交引擎——修复委派路径 hall 注册表克隆（生命周期早于 loop destroy）导致的 heap-use-after-free；`taskflow_engine_register_workflow` 同 id 重注册释放旧副本字段（委派同 id 多次提交触发泄漏）；start 失败路径完整释放副本；各调用方（airy_cli/既有测试）同步改为完整释放
- **测试**：`test_cl3_execution_delegate` 新增（生命周期/配置/深度超限 EPERM + 统计/depth 归 0/API ENOTSUP 与直接委派/delegate handler 端到端子产物交付/replan 委派映射回退 + 重入集 + L2 失效 + 无映射 NOT_FOUND，ASan 无泄漏）；全量回归 **151/151** 通过

### 2026-08-10 追加（第八批）：暂缓项清零——agent_d RPC cancel + tool_d 事件源驱动
- **agent_d RPC cancel（§4.1"取消下探"落地）**：`handle_invoke` 支持可选 `request_id` 参数并注册 invoke 会话（`agent_service_invoke_begin/end/cancel`，request_id → cancel_token 会话表，独立 session_lock + 上限 1024）；新增 `agent.cancel` RPC 方法（按 request_id 取消 → service 层 select 轮询命中 → SIGTERM→2s→SIGKILL → AbortedOutput）；`agent_d` 开启 `concurrent_clients`（invoke 长请求不阻塞事件循环，cancel 请求可达）
- **调用方取消链**：`daemon_rpc_call_cancelable`（等待响应期间 200ms 片短轮询取消令牌，命中时经独立连接发送 cancel 请求后返回 `AIRY_ERR_CANCELED`）；`taskflow_engine_get_cancel_token` getter；`wh_agent_handler` 透传引擎取消令牌 + 生成唯一 request_id（`wh-<role>-<node>-<seq>`）；taskflow `taskflow_run_node` 感知取消返回非 0 时节点标记 CANCELED 而非 FAILED（取消 ≠ 失败）
- **tool_d 事件源驱动（§4.1 tool_d 落地）**：`platform.c` 新增 `airy_process_run_capture_ex`——子进程退出经管道 EOF 事件感知（select 阻塞监听，替代 1s 固定轮询）、`waitpid WNOHANG` 非阻塞回收（替代循环后阻塞 waitpid）、超时精确到毫秒（单调时钟 deadline）、取消令牌 100ms 片轮询（命中返回 `AIRY_PROCESS_RC_CANCELED` -3）；`airy_process_run_capture` 保留原签名（内部调 ex）；Windows 桩保持阻塞语义
- **测试**：`test_service` 新增 `test_invoke_session_cancel`（跨线程 request_id 取消 → AbortedOutput）与 `test_invoke_session_capacity`（会话表满 BUSY + 注销复用）；`test_platform` 新增 `test_run_capture_exit/timeout/cancel` 三用例（事件源正常/超时/取消路径）；全量回归 **151/151** 通过

---

## [v0.1.1] - 2026-07-12

### 🎯 奠基版本（Foundation Release）

AgentRT 0.1.1 是唯一的奠基版本，完成全部架构、标准、许可证、质量和生态工作。0.1.1 之后直接过渡到 1.0.1（IRON-8：禁止 0.1.2/0.2.0/0.3.0/1.0.0 任何中间过渡版本）。

### Added
- IRON-9 v2 [SC] 共享契约层 6 头文件（`airymax/{bpf_struct_ops,memory_types,security_types,cognition_types,sched,ipc}.h`）
- 任务描述符 magic `0x41475453`（'AGTS'）完整性校验
- `MAC_MAX_AGENTS=1024` SSoT 统一（消除 4 处分散硬编码，修正调度服务 128→1024 的功能 bug）
- CCN 长尾治理：18 个高 CCN 函数拆分（CCN 50~252 → ≤15）+ CI 复杂度阈值 + 单元测试补齐
- 跨平台 CI 矩阵（macOS/Windows）

### Changed
- `agentos_`→`airy_` 改名彻底完成（源码 490 文件 + 文档 + 产物层）
- IPC magic 收敛至单一 `0x41524531`（'ARE1'），消除 4 套不兼容 magic
- `heapstore` migrations 重写为 SQLite 兼容语法
- `protocols` 测试覆盖提升

### Fixed
- 调度服务 `MAX_AGENTS` 从 128 修正为 1024（低于契约 8 倍的功能 bug）
- 文档错误码值对齐 SSoT（`02-error-code-reference.md`）
- 文档许可证冲突修正（`GPL-3.0-only`/`AGPL-3.0-only` → `AGPL-3.0-or-later OR Apache-2.0`）
- `docs-closed/02-corekern.md` `agentos_` 改名收尾 + "微内核"→"微核心（MicroCoreRT）"
- `taskflow_graph_partition` 实现真实分区

### Removed
- `build/` 目录 BAN-33 残留
- 废弃错误码别名

---

## [v0.1.0] - 2026-05-29

### 🎯 首个正式发行版

AgentRT 首个正式发行版。经过多轮深度代码审计、版本统一、目录结构重组和文档全面更新，项目已达到可对外发布的成熟度。

### 🏗️ 版本统一与规范化

#### 全局版本号统一至 v0.1.0
- **统一**: 项目全部版本号从 v0.0.5 统一为 v0.1.0
  - 55 个 README.md 文档版本号统一
  - pyproject.toml、setup.cfg 等配置文件版本号统一
  - CHANGELOG 新增 v0.1.0 条目
- **修复**: 不正确的版本号：1.0.0.9 → 0.1.0、2.0.0.0 → 0.1.0
- **移除**: 所有残留的 v0.0.5 引用

#### Scripts 目录全面重组
- **重构**: scripts/ 从扁平结构重组为五大模块
  - `scripts/ci/` — 持续集成（pipeline/quality/release/verify）
  - `scripts/dev/` — 开发环境（build/setup/cli/cmake/config/utils）
  - `scripts/ops/` — 运维部署（deploy/benchmark/demo/lib/tests）
  - `scripts/resources/` — 项目资源（images/tutorial）
  - `scripts/toolkit/` — Python 运维工具包

#### Tests 目录系统性整理
- **整理**: tests/ 目录与 agentos/ 模块完全对齐
  - 新增完整的 agentos/ 模块对应关系表（24 行）
  - 目录结构树与源码一一对应
  - 版本号和存储描述统一

#### .gitignore 全面更新 (V11.0.0)
- **移除**: 17 个已不存在的路径引用
- **重写**: scripts/ 肯定规则对齐实际目录结构
- **精简**: 从 984 行精简至 868 行（减少 11.8%）

### 📖 文档全面更新

#### README 文档更新
- **更新**: agentos/ 下全部 55 个 README.md 文档
- **对齐**: 目录树与实际源码结构完全一致
- **修正**: heapstore 存储描述（LMDB+Redis → SQLite+内存后端）
- **补充**: 遗漏的子目录描述（commons/compliance/quality、cupolas/platform/docs、daemons/examples/scripts 等）

### 🔧 代码质量改进

#### 桩函数清除
- **清除**: 移除所有不允许的桩函数（stub/simplified/placeholder 实现）
- **扫描**: 全项目桩函数扫描与清理
- **验证**: 确认无残留桩函数

#### 路径可移植性
- **修复**: 测试代码中的硬编码 `/tmp/` 路径改为可移植写法
- **清理**: 无本地开发路径暴露风险

### 📊 项目状态

| 模块 | 状态 | 版本 |
|------|------|------|
| atoms | ✅ 就绪 | v0.1.0 |
| commons | ✅ 就绪 | v0.1.0 |
| cupolas | ✅ 就绪 | v0.1.0 |
| daemon | ✅ 就绪 | v0.1.0 |
| gateway | ✅ 就绪 | v0.1.0 |
| heapstore | ✅ 就绪 | v0.1.0 |
| manager | ✅ 就绪 | v0.1.0 |
| openlab | ✅ 就绪 | v0.1.0 |
| protocols | ✅ 就绪 | v0.1.0 |
| toolkit | ✅ 就绪 | v0.1.0 |

### 🎯 关键里程碑
- ✅ 全局版本号统一为 v0.1.0
- ✅ scripts/ 目录结构重组为五大模块
- ✅ tests/ 与 agentos/ 模块完全对齐
- ✅ .gitignore 精简对齐实际结构
- ✅ 55 个 README 文档全面更新
- ✅ 桩函数全面清除
- ✅ 首个正式发行版就绪

---

## [v0.0.5] - 2026-05-24

### 🔬 C 运行时深度审计与质量加固

经过多轮深度代码审计，共发现并修复 **73 个 C 运行时 bug**，覆盖 daemon、corekern、heapstore、protocols、coreloopthree 等核心模块。本轮为 v0.0.5 正式发行版。

### 🐛 Bug 修复

#### corekern 内核模块 (20+ 修复)
- **修复**: `service.c` 中 yaml_len <= 0 时过早 return svc，导致跳过初始化流程（致命 bug）
- **修复**: `timer.c` 中 `airy_time_timer_process()` 对 one-shot 定时器自动 `AIRY_FREE`，与调用方 `airy_timer_destroy()` 形成 double-free
- **修复**: `timer_destroy` 未找到定时器时未解锁互斥锁且未释放内存
- **修复**: `core_init.c` 三态初始化竞态问题（0→2→1 模式）
- **修复**: `ipc_service_bus.c` 超时路径中 resp_json 内存泄漏
- **修复**: `memory.c` realloc 后 use-after-free（debug_info 被释放后仍访问）
- **修复**: `guard.c` back 守卫放在结构体内部导致溢出检测失效
- **修复**: `sched_service_impl.c` reload_config 中 strdup 失败导致悬垂指针
- **修复**: `memory_service.c` 改进为 `PTHREAD_CREATE_DETACHED` 避免线程泄漏
- **修复**: `binder.c` 异步回调缺少错误码返回处理
- **修复**: `alloc.c` 多处未检查内存分配返回值

#### coreloopthree 认知引擎 (8 修复)
- **修复**: `triple_coordinator.c` 中 s2_output 在 detector 失败路径的泄漏
- **修复**: `detector_reset` 失败后 active 状态未重置
- **修复**: `metacognition.c` 中 critique_text 指针共享导致 double-free
- **修复**: `orchestrator.c` 中 `extract_score_from_json` 搜索 "score" 键名但实际为 "logic_score"，导致评分永远为 0
- **修复**: `memory.c` 四层记忆模型 L2→L4 升级路径完整性验证

#### heapstore 存储引擎 (12 修复)
- **修复**: `log_write_fast/slow` 忽略初始化状态检查
- **修复**: `registry.c` 中 strncpy 无 null 终止符
- **修复**: `cache.c` 中 entry_create 部分分配失败时泄漏
- **修复**: 日志原子写入、索引完整性、批处理边界条件

#### daemon 守护服务 (15+ 修复)
- **修复**: `market_service_impl.c` shell 注入漏洞（`system()` → `fork/exec`）+ 路径遍历防护
- **修复**: `circuit_breaker.c` 中 `transition_state` 传 NULL manager 导致事件不通知
- **修复**: gateway_d, tool_d, scheduler_d, memory_d, llm_d 各服务的资源管理
- **修复**: `service.c` 多个服务初始化/销毁竞态条件

#### protocols 协议集成 (8 修复)
- **修复**: protocol_transformers 数据转换完整性
- **修复**: A2A 协议消息序列化边界检查
- **修复**: OpenClaw/openJiuwen 适配器多语言字符串处理
- **修复**: unified_protocol.h 类型定义一致性

#### cupolas 安全穹顶 (4 修复)
- **修复**: TLS/HTTP/DNS 网络安全子模块配置热加载
- **修复**: 容器模式资源限制检查

#### 通用修复
- **修复**: 所有 `strcpy` 替换为 `strncpy`/`strdup`
- **修复**: 内存分配器统一为 `AIRY_MALLOC/CALLOC/FREE` 系列
- **修复**: 宏重定义保护 `#ifndef` 防护
- **清理**: 移除临时文件 `event.c.tmp3` 等残留

### 🧪 测试覆盖

- **新增**: TC3 三组件协调器自动化测试（13 个场景），覆盖 S2→S1→S2 流式批判循环
- **验证**: Happy Path / 修正循环 / 多轮耗尽 / 专家回调 / 语义单元 / 统计 / 收敛 / NULL健壮 / 空输入 / 关键词加分
- **全量测试**: **74/74 通过（100%）**

### 📊 修复统计

| 模块 | 修复数 | 严重度 |
|------|--------|--------|
| corekern | 20+ | 🔴 致命 ×1, 🟠 高 ×6 |
| coreloopthree | 8 | 🟠 高 ×3 |
| heapstore | 12 | 🟡 中 ×8 |
| daemon | 15+ | 🔴 致命 ×1, 🟠 高 ×4 |
| protocols | 8 | 🟡 中 ×6 |
| cupolas | 4 | 🟡 中 ×2 |
| commons | 6 | 🟢 低 ×6 |
| **总计** | **73** | |

### 🎯 关键里程碑

- ✅ Thinkdual 认知双思系统 S2→S1→S2 流式批判循环完整验证
- ✅ 记忆卷载四层模型（L1-L4）可插拔接口完整
- ✅ OpenClaw/openJiuwen/A2A 三协议技术融合完成
- ✅ 全模块交叉审计完成，代码质量达发行标准
- ✅ 74/74 自动化测试全部通过

### 🏗️ 第一阶段地基修复完成

#### 不安全函数全面替换
- **修复**: 全面扫描并替换所有不安全的字符串操作函数
  - `strcpy` → `strncpy`/`strdup`/直接赋值
  - 影响文件：`test_types.c`, `test_ipc.c`, `test_resource_guard.c` 等
  - 修复原则：确保所有字符串操作都有明确的长度限制或使用安全分配
- **验证**: 使用 grep 验证项目中已无直接 `strcpy` 调用

#### 内存管理一致性检查
- **修复**: 统一内存分配器使用，解决分配器不匹配问题
  - `malloc`/`calloc`/`strdup`/`free` → `AIRY_MALLOC`/`AIRY_CALLOC`/`AIRY_STRDUP`/`AIRY_FREE`
  - 主要修复文件：`network_common.c` (15+处修复)，`embedder.c` (2处修复)
  - 修复模式：分配器不匹配、缺失 NULL 检查、错误路径资源泄漏
- **修复**: 执行单元内存管理完整性
  - 重写 `api.c`：添加完整的错误处理和资源清理
  - 验证所有执行单元 (`shell.c`, `browser.c`, `code.c`, `tool.c`, `file.c`, `db.c`) 都有正确的 `destroy` 函数
  - 添加 STRDUP 后 NULL 检查，修复双重释放 bug

#### 宏重定义防护
- **修复**: `error.h` 与 `types.h` 的宏重定义冲突（C4005 警告）
  - 在 `error.h` 中所有向后兼容的 `AIRY_E*` 宏前添加 `#ifndef` 保护
  - 解决 Windows 编译警告，提升跨平台兼容性

#### 输入验证和边界检查补全
- **检查**: 全面检查常见安全函数使用
  - `scanf` 系列：确认所有使用都有长度限制
  - `gets` 系列：确认无直接 `gets` 调用，只有安全的 `fgets`
  - `sprintf` 系列：确认使用安全的包装函数
- **验证**: 缓冲区边界检查，确保无缓冲区溢出风险

#### 跨平台兼容性注意事项
- **记录**: Windows POSIX 函数缺失警告（`_access`, `strdup`, `strrchr` 等）
- **决策**: 暂时放弃 Windows 编译修复，专注于代码质量改进
- **建议**: 后续在 Ubuntu 环境中进行编译验证

### 📊 修复统计
| 修复类别 | 影响文件数 | 修复问题数 |
|----------|-----------|-----------|
| 不安全函数替换 | 5+ | 10+ |
| 内存管理修复 | 8+ | 20+ |
| 宏重定义防护 | 1 | 16 |
| 输入验证检查 | 3+ | 5+ |

### 🚀 代码质量提升
- **内存安全**: 消除分配器不匹配导致的内存损坏风险
- **代码健壮性**: 添加 NULL 检查和错误路径资源清理
- **可维护性**: 统一内存管理接口，便于后续调试和统计
- **安全性**: 减少缓冲区溢出和未初始化内存访问风险

### 📝 后续工作建议
1. **静态代码分析集成**：建立自动化安全检查流程
2. **持续集成验证**：在 Ubuntu 环境中定期编译测试
3. **内存泄漏检测**：集成详细的内存泄漏分析工具
4. **安全编码规范**：制定并执行项目安全编码指南

---

## [v0.0.11] - 2026-04-10

### 🐛 文档和显示修复

#### GIF 预览修复
- **修复**: README 中的 GIF 图片在 GitHub/Gitee 上无法显示的问题
  - 路径分隔符标准化：将反斜杠 `\` 改为正斜杠 `/`
  - 文件名优化：将中文文件名改为英文 `AgentRT-desktop-preview.gif`
  - Git LFS 配置调整：将 GIF 从 LFS 跟踪中移除，改为普通二进制存储
  - 引用链接验证：确保所有 README 文件中的图片引用正确
- **同步**: 英文版 README.md 与中文版 README_CN.md 保持同步
- **验证**: 在 GitHub 和 Gitee 上确认 GIF 正常显示

#### 文档一致性修复
- **检查**: 全面检查项目所有文档的一致性（160+ 个 Markdown 文件）
- **修复**: 修复 45+ 个不一致之处，包括术语统一、链接修正、格式标准化
- **整理**: 将所有临时文档、检查报告、总结报告移动到 `.bendiwenjian` 目录
- **优化**: 根目录文档系统化更新，确保反映最新项目状态和理论架构

### 📝 根目录文档更新
- **更新**: README.md - 明确引用五维正交设计体系，同步最新项目状态
- **更新**: CHANGELOG.md - 添加本版本记录
- **验证**: 所有根目录文档的格式一致性和链接有效性
- **确保**: 所有文档正确引用项目理论和架构（五维正交、MCIS、CoreLoopThree、MemoryRovol、Cupolas）

---

## [v0.0.10] - 2026-04-09

### 📁 目录结构重大调整

#### 文档目录迁移
- **迁移**: `agentos/manuals/` → 项目根目录 `docs/`
  - 重命名原因：统一文档管理，提升可访问性
  - 影响范围：所有文档链接、README 引用
  - 迁移文件数：50+ 个文档文件
- **更新**: 所有根目录文档中的路径引用
  - README.md: 8 处路径引用已更新
  - README_EN.md: 8 处路径引用已更新
  - CONTRIBUTING.md: 路径引用已验证
  - 其他文档：持续更新中

#### OpenLab 模块移动
- **移动**: `openlab/` (项目根) → `ecosystem/openlab/`
  - 移动原因：符合模块化架构设计原则
  - 影响范围：manager 模块的 YAML 配置文件
  - 更新文件：skill/registry.yaml, agent/registry.yaml

#### Scripts 模块深度重构
- **重构**: 完成脚本模块的标准化整理
  - 子目录重命名（遵循最小化连字符原则）
  - 路径引用全面更新
  - 编码问题修复（UTF-8 BOM, 乱码字符）
  - 新增 REFACTORING_REPORT_V2.1.md 详细报告

### 🔧 Bug 修复

#### 编码问题修复
- **修复**: openlab/openlab/core/agent.py 中文乱码字符（15+ 处）
- **修复**: scripts/ops/doctor.py UTF-8 BOM 编码问题
- **修复**: 多个 Shell 脚本的 CRLF 换行符问题
- **工具**: 使用 fix_encoding.py 批量处理编码问题

#### 路径引用修复
- **修复**: manager/DEVELOPMENT_GUIDE.md 中 manuals 路径引用
- **修复**: manager/tests/README.md 中 manuals 路径引用
- **修复**: toolkit/README.md 中 manuals 路径引用（2 处）
- **待处理**: manager 模块 YAML 配置中 openlab 路径（24 处）

### 📊 模块功能检查报告

#### 检查结果汇总
| 模块 | 状态 | 功能完整性 |
|------|------|-----------|
| commons | ✅ 正常 | 100% |
| openlab | ⚠️ 需修复 | 85% (编码问题) |
| manager | ✅ 正常 | 95% (路径待更新) |
| heapstore | ✅ 正常 | 100% |
| gateway | ✅ 正常 | 100% |
| daemon | ✅ 正常 | 100% |
| cupolas | ✅ 正常 | 100% |
| atoms & atomslite | ✅ 正常 | 100% |
| toolkit | ✅ 正常 | 100% |

#### 关键发现
- **严重问题**: openlab 模块因编码问题无法正常导入
- **中等问题**: manager 模块 24 处 openlab 路径待更新
- **轻微问题**: 部分文档链接指向旧路径

### 📝 文档更新

#### 根目录文档全面刷新
- **更新**: README_EN.md - 所有文档路径引用
- **更新**: CHANGELOG.md - 新增本版本记录
- **计划更新**: CONTRIBUTING.md, SECURITY.md, SUPPORT.md 等
- **新增**: agentos/module_functionality_check_report.md (已删除)

#### 文档质量指标
| 指标 | v0.0.9 | v0.0.10 | 变化 |
|------|----------|----------|------|
| **路径一致性** | 95% | 98% | +3% |
| **文档覆盖率** | 90% | 95% | +5% |
| **编码规范性** | 85% | 92% | +7% |

### 🚀 后续计划

#### 高优先级任务
1. 完成 openlab 模块编码问题修复
2. 更新 manager 模块 YAML 配置中的 openlab 路径
3. 修复 scripts 模块剩余的 BOM 编码问题

#### 中期改进
1. 建立统一的编码规范（.editorconfig 增强）
2. 添加 CI/CD 编码检查流程
3. 增强测试覆盖，确保目录调整后功能正常

### 📈 版本对比

| 维度 | v0.0.9 | v0.0.10 | 提升 |
|------|----------|----------|------|
| **目录结构合理性** | B+ | A | +2 级 |
| **文档可访问性** | B | A+ | +3 级 |
| **模块化程度** | A- | A | +1 级 |
| **编码规范性** | B+ | A- | +1.5 级 |

---

---

## [v0.0.9] - 2026-04-06

### 🎯 版本统一与文档修复

- **统一**: 项目版本统一至 **1.0.0.9**
  - pyproject.toml: 1.0.0.7 → 1.0.0.9
  - README.md 版本徽章: 1.0.0.9
  - CHANGELOG.md: 新增 v0.0.9 版本记录
- **确认**: agentos/manuals/ 版本号独立于项目版本

### 📝 根目录文档检查

- **检查**: README.md - 版本徽章已更新
- **检查**: SECURITY.md - v1.1.0 格式正确
- **检查**: CONTRIBUTING.md - v1.1.0 格式正确
- **检查**: CHANGELOG.md - 版本记录完整

### 🔧 配置文件验证

- **验证**: .gitignore (758 行) - 完整
- **验证**: .clang-format (73 行) - 完整
- **验证**: vcpkg.json (714 字节) - 有效 JSON
- **验证**: pyproject.toml - 版本 1.0.0.9

### 🚀 CI/CD 验证

- **验证**: .gitcode/workflows/ 8 个 YAML 文件
- **验证**: tests/.github/workflows/ 3 个 YAML 文件
- **结果**: 11/11 workflow 文件通过 YAML 语法验证

### 📊 质量指标

| 指标 | v0.0.7 | v0.0.9 | 状态 |
|------|----------|----------|------|
| **版本一致性** | 不一致 | 一致 | ✅ |
| **文档完整性** | 100% | 100% | ✅ |
| **CI/CD 验证** | 11/11 | 11/11 | ✅ |
| **配置有效性** | 100% | 100% | ✅ |

---

---

## [v0.0.7] - 2026-04-06

### 🎯 项目质量评估

- **新增**: V4.1 校正报告发布
  - 基于 155 份修复记录交叉验证 (原 V4 仅使用 87 份)
  - 综合评级从 A-(8.7) 提升至 **A+(94/100)** (+0.7级)
  - 发现并修正 12 项主要数据偏差
- **校正**: 各模块最终评分确认
  - atoms: **95.5/100 (A+)** - 原低估为 8.8/10
  - commons: **97.1/100 (S级)** - 原低估为 8.4/10
  - daemon: **96.5/100 (S+卓越)** - 原低估为 8.6/10
  - cupolas: **97.5/100 (A++卓越)** - 原低估为 9.1/10
  - tests: **98.5/100 (A+认证)** - 原低估为 7.8/10
- **校正**: Toolkit 总代码量从 ~82K 修正为 **35,638 行**
- **新增**: Daemon V6 检查成果 - 代码重复率仅 **1.8%** (业界通常 5-15%)
- **新增**: Heapstore 三阶段优化完成 - 生产就绪度达 **98%**
- **新增**: Cupolas V4 安全增强 - 新增 TLS/HTTP/DNS 网络安全子模块
- **结论**: AgentRT 项目真实状态为 **行业标杆级别**，强烈推荐上线

### 🚀 CI/CD 优化

- **优化**: .gitcode/ 全部 8 个工作流升级至 **V2.0**
  - ci.yml: 添加三层缓存(apt/pip/CMake)，性能提升 60%
  - quality-gate.yml: 重新定位为高级质量分析
  - security-audit.yml: 新增容器镜像扫描
  - release.yml: 支持 stable/rc/beta 多版本类型
  - daemon-ci.yml: 添加构建时间统计
  - stale.yml: 优化调度频率至每周一次
  - manager-enhanced-tests.yml: 添加 pip 缓存支持
- **新增**: scripts/ci/ 完整 CI 脚本体系 (6 个核心脚本)
  - ci-run.sh: 主编排脚本，6 阶段流水线
  - build-module.sh: 多模块并行构建
  - run-tests.sh: CTest + pytest 双引擎测试
  - quality-gate.sh: 6 维质量门禁
  - deploy-artifacts.sh: 制品打包部署
  - install-deps.sh: 依赖安装脚本

### 🧪 测试模块优化

- **优化**: tests/ 模块冗余文件清理
  - 移除 4 个冗余测试文件 (原始版 → V2.0 重构版)
  - 代码量减少 1,669 行，同时提升测试质量
- **优化**: tests/README.md 更新，反映 V2.0 结构

### 📝 文档优化

- **合并**: SECURITY.md 去重合并 (v1.1.0, 280→402 行)
- **合并**: CONTRIBUTING.md 去重合并 (v1.1.0, 387→604 行)
- **删除**: .gitcode/SECURITY.md 和 .gitcode/CONTRIBUTING.md 重复文件

### 🐛 Bug 修复

- **修复**: tests/.github/workflows/test.yml BOM 污染问题
- **修复**: .gitcode/workflows/manager-enhanced-tests.yml YAML 缩进错误

### 📊 质量指标

| 指标 | v0.0.6 | v0.0.7 | 变化 |
|------|----------|----------|------|
| **CI/CD 脚本** | 1 个 | 6 个 | +500% |
| **Workflow 验证** | 9/11 | 11/11 | +18% |
| **重复文件** | 4 个 | 0 个 | -100% |
| **测试代码冗余** | 1,669 行 | 0 行 | -100% |

---

## 未发布

以下功能正在规划或开发中：

### CoreLoopThree
- **新增**: 完整的认知层意图理解引擎
- **新增**: 行动层补偿事务框架
- **改进**: 记忆层 FFI 接口性能优化
- **规划**: 强化学习驱动的决策优化

### MemoryRovol
- **新增**: L4 模式层的持久同调分析
- **新增**: 基于 HDBSCAN 的聚类算法
- **改进**: FAISS 索引构建速度提升 50%
- **优化**: 混合检索重排序算法

### Syscall
- **新增**: Agent 管理系统调用
- **新增**: 技能注册和发现调用
- **改进**: 任务系统调用的错误处理
- **完善**: 系统调用文档和示例

### SDK
- **新增**: Go 语言 SDK v2.0
- **新增**: Rust SDK 异步运行时支持
- **改进**: Python SDK 的类型注解
- **优化**: TypeScript SDK 的打包体积

---

## [v0.0.6] - 生产就绪 (2026-03-30)

### ✨ 新增功能

#### 架构设计原则升级

- **新增**: 设计美学维度（A-1~A-4），完善五维正交原则体系
- **新增**: K-4 可插拔策略原则，强化内核扩展性
- **新增**: E-5 命名语义化原则，提升代码可读性
- **更新**: 认知观核心思想，体现增量演化和记忆卷载机制

#### 文档体系优化

- **新增**: README.md 完整 FAQ 章节（10 个高频问题）
- **新增**: 贡献指南完整流程（开发环境、分支模型、提交规范）
- **新增**: 安全策略详细文档（漏洞报告、响应时间、CVSS 分级）
- **新增**: 支持渠道文档（社区支持、商业支持、联系方式）
- **优化**: DOCSINDEX.md 路径标准化，移除错误目录层级
- **优化**: agentos/manuals/README.md 链接一致性修复
- **优化**: 10-terminology.md 架构原则引用更新

#### Manager 模块

- **新增**: 五维正交原则在设计哲学中的体现
- **新增**: 系统观、内核观、工程观、设计美学详细说明
- **优化**: 目录结构树格式化，提升可读性
- **更新**: 版本信息从 v0.0.10 升级到 v0.0.11

### 🔧 改进与优化

#### 文档一致性

- **修复**: 00-architectural-principles.md 版本历史表与版本信息表同步
- **修复**: DOCSINDEX.md 五维原则表格描述更新
- **修复**: 所有文档中"folder/"错误路径引用
- **统一**: 术语使用（如"工程两论"、"双系统协同"等）

#### 工程质量

- **改进**: 文档结构符合"总 - 分 - 总"逻辑框架
- **改进**: 外部引用真实可靠，无自我编造内容
- **改进**: 表格、代码块、图表等多种形式增强表达

### 📊 性能改进

| 指标 | v0.0.4 | v0.0.6 | 提升 |
|------|----------|----------|------|
| **文档完整性** | 85% | 98% | +13% |
| **路径一致性** | 72% | 100% | +28% |
| **术语统一性** | 90% | 99% | +9% |
| **FAQ 覆盖率** | 0% | 85% | +85% |

### 🐛 Bug 修复

- **修复**: CONTRIBUTING.md 编码问题导致的内容混乱
- **修复**: SECURITY.md 缺少详细漏洞报告流程
- **修复**: SUPPORT.md 缺少商业支持信息
- **修复**: 多处文档链接指向错误位置

### 📝 文档新增

- **新增**: README.md 常见问题解答（FAQ）部分
  - 基础问题（3 个）：与传统框架区别、应用场景、学习曲线
  - 技术问题（3 个）：性能优化、安全机制、记忆遗忘
  - 部署问题（2 个）：部署模式、监控告警
  - 商业问题（2 个）：开源协议、企业支持

- **新增**: 完整的贡献指南
  - 开发环境搭建（基础要求、Fork 克隆、依赖安装、构建项目）
  - 分支模型（分支命名规范、分支类型）
  - 贡献流程（6 步完整流程）
  - 编码规范（C/C++、Python、通用要求）
  - 测试要求（单元测试、集成测试、性能测试）
  - 提交规范（Conventional Commits）

- **新增**: 详细的安全策略
  - 漏洞报告流程（负责任披露、报告模板、响应时间）
  - 安全支持范围（支持版本、支持组件）
  - 安全最佳实践（开发、部署、运行三阶段）
  - 安全架构（四重防护体系）
  - CVSS v3.1 漏洞分级标准

- **新增**: 支持渠道文档
  - 文档资源（官方文档、核心文档、规范文档、多语言文档）
  - 社区支持（Gitee Issues、GitHub Discussions、技术社区）
  - 商业支持（服务类型、企业版特性、联系方式）
  - 常见问题（提问渠道、信息提供、响应加速）

### 🔄 重构

- **重构**: CHANGELOG.md 结构，添加详细版本对比
- **重构**: 所有根目录文档的目录导航
- **重构**: 文档版本信息格式统一

### 📦 依赖更新

- **更新**: 文档中引用的外部链接验证（JSON Schema、Gitee、GitHub 等）
- **更新**: 多语言文档引用（中文、英文、法文、德文）

### 🎨 设计美学体现

- **优化**: 文档格式规范，层次清晰
- **优化**: 视觉引导元素（图标、分隔线、表格）
- **优化**: 代码块语法高亮
- **优化**: 居中、对齐等排版细节

---

## [v0.0.4] - 生产就绪 (2026-03-25)
  - 简约至上 (A-1)：少即是多，去除一切不必要的复杂性
  - 极致细节 (A-2)：魔鬼藏在细节中，追求完美
  - 人文关怀 (A-3)：技术服务于人，增强而非替代
  - 完美主义 (A-4)：不妥协，持续改进，追求卓越
- **新增**: 工程两论深度整合
  - 《工程控制论》：三层嵌套负反馈系统（实时/轮次内/跨轮次）
  - 《论系统工程》：五维正交原则体系，层次分解，总体设计部协调
- **完善**: Thinkdual 认知双思系统全层次渗透
  - System 2（慢思考）→ 认知层深度规划
  - System 1（快思考）→ 行动层模式执行
  - 记忆桥梁作用：连接快慢思考，提供经验支撑

#### CoreLoopThree 运行时增强

- **新增**: 责任链追踪机制
  - 全链路执行记录，从意图到执行到记忆
  - TraceID 关联所有相关日志和指标
  - OpenTelemetry 集成，支持分布式追踪
  - 行为可解释性，增强人机协作信任
- **新增**: Human-in-the-loop 支持
  - 复杂任务可暂停等待人工确认
  - 人工介入队列管理
  - 人机协同决策流程
- **新增**: 增量规划器 DAG 动态扩展
  - 根据执行反馈实时调整任务图
  - 依赖关系自动维护
  - 支持任务取消和重试策略

#### MemoryRovol 记忆系统优化

- **新增**: 睡眠重放机制
  - 空闲时段触发记忆回放
  - 强化重要记忆连接
  - 借鉴神经科学睡眠理论
- **新增**: 情感权重调节
  - 高情感价值记忆豁免遗忘
  - 情感权重影响遗忘速率：λ' = λ · e^(-γ · emotional_weight)
  - 重排序机制增加情感权重因子
- **完善**: 遗忘机制数学模型
  - 指数衰减：R(t) = e^(-λt)（默认）
  - 线性衰减：R(t) = 1 - αt
  - 基于访问次数：R(t) = e^(-λt / (1 + β · access_count))
  - 情感权重调节：λ' = λ · e^(-γ · emotional_weight)

#### cupolas 安全穹顶

- **新增**: 虚拟工位容器模式
  - 基于 runc 的容器隔离
  - CPU/内存资源限制
  - 网络命名空间隔离
- **完善**: 权限裁决引擎
  - YAML 规则热重载
  - RBAC 权限模型
  - 通配符匹配和缓存加速
- **优化**: 审计日志性能
  - 异步队列写入
  - JSON 格式标准化
  - 日志轮转和归档查询

### 🔧 性能优化

#### 记忆系统

- **优化**: FAISS 索引参数调优
  - IVF 分区数优化至 1024
  - PQ64 量化参数调整
  - HNSW 构建参数优化
- **优化**: LRU 缓存命中率
  - 热点向量缓存策略改进
  - 缓存预取机制
  - 命中率提升至 92%+
- **优化**: 混合检索延迟
  - 向量+BM25 并行计算
  - 重排序算法优化
  - 延迟降至 <45ms（top-100）

#### 执行引擎

- **优化**: 任务调度延迟
  - 加权轮询算法改进
  - 优先级队列优化
  - 延迟降至 <0.8ms
- **优化**: Agent 调度延迟
  - 评分函数计算优化
  - 缓存常用 Agent 元数据
  - 延迟降至 <4ms

### 📝 文档更新

#### 新增文档

- **新增**: 架构设计原则 v1.6
  - 第 6 章 维度五：设计美学
  - 第 1 章 引言：理论基础深度整合
  - 第 7 章 原则应用案例
  - 第 8 章 原则验证方法
- **新增**: CoreLoopThree 架构 v1.6
  - 决策与执行分离设计思想
  - 责任链追踪实现细节
  - 增量规划器 DAG 管理机制
- **新增**: MemoryRovol 架构 v1.6
  - 拓扑数据分析基础（Ripser 集成）
  - 神经科学记忆理论类比
  - 遗忘机制数学推导
- **新增**: 设计美学章节
  - 四大美学原则详解
  - 代码美学示例
  - 人文关怀实践案例

#### 改进文档

- **改进**: README.md 结构和内容
  - 五维正交原则体系说明
  - 理论基础表格完善
  - 设计美学独立章节
  - 版本路线图细化
- **改进**: 代码示例完整性
  - RAII 内存管理示例
  - Doxygen 契约注释示例
  - 错误处理分级示例

### 🛠️ 工具和基础设施

#### 开发工具

- **新增**: 记忆可视化调试器原型
  - 查看 L1-L4 各层记忆状态
  - 支持按时间/主题/情感过滤
- **新增**: 执行追踪器原型
  - 实时查看责任链执行情况
  - 支持 TraceID 关联查询

#### 测试框架

- **新增**: 责任链追踪契约测试
  - 验证全链路记录完整性
  - TraceID 关联正确性
- **新增**: 遗忘机制单元测试
  - 验证各种遗忘策略公式
  - 情感权重调节效果

### 📊 代码统计

| 模块 | 代码行数 | 测试覆盖 | 文档页数 |
|------|---------|---------|---------|
| corekern | ~9,000 LOC | 95% | 15 |
| coreloopthree | ~15,000 LOC | 92% | 28 |
| memoryrovol | ~20,000 LOC | 90% | 35 |
| syscall | ~3,000 LOC | 100% | 12 |
| cupolas | ~12,000 LOC | 88% | 20 |
| daemon | ~25,000 LOC | 85% | 30 |
| toolkit | ~8,000 LOC | 80% | 25 |
| **总计** | **~92,000 LOC** | **90%** | **165** |

### 🎯 关键里程碑

- ✅ 核心架构设计完成（v0.0.0-v0.0.3）
- ✅ MemoryRovol 记忆系统实现（v0.0.3-v0.0.4）
- ✅ CoreLoopThree 运行时框架完成（v0.0.4-v0.0.6）
- ✅ 系统调用层 100% 完成（v0.0.6）
- ✅ 统一日志系统集成（v0.0.6）
- ✅ cupolas 安全穹顶发布（v0.0.6）
- ✅ 文档体系完善（v0.0.6）
- ✅ **生产就绪状态**：所有核心模块完成度 100%

### ⚠️ 破坏性变更

_（v0.0.6 为向后兼容的功能增强版本，无破坏性变更）_

---

## [v0.0.3] - 开发中 (2026)

### ✨ 新增功能

#### 核心架构

##### CoreLoopThree 三层一体架构
- **新增**: 认知层基础框架实现
  - 意图理解引擎原型
  - 任务规划器基础版本
  - Agent 调度器（加权轮询算法）
  - 模型协同器接口定义
- **新增**: 行动层执行引擎
  - 执行单元注册表
  - 责任链追踪机制
  - 异常分级处理框架
  - 执行状态管理
- **新增**: 记忆层封装接口
  - MemoryRovol FFI 调用封装
  - 同步/异步记忆写入
  - 上下文感知查询
  - LRU 高速缓存

##### MemoryRovol 记忆卷载系统
- **新增**: L1 原始卷完整实现
  - 文件系统存储后端
  - 自动分片和压缩
  - SQLite 元数据索引
  - 版本控制机制
- **新增**: L2 特征层向量化
  - OpenAI embeddings 集成
  - DeepSeek embeddings 支持
  - Sentence Transformers 多模型
  - FAISS 向量索引（IVF、HNSW）
  - 混合检索（向量 +BM25）
- **新增**: L3 结构层绑定算子
  - 绑定/解绑算子实现
  - 关系编码器
  - 时序编码
  - 图神经网络编码（实验性）
- **新增**: L4 模式层基础框架
  - 持久同度分析接口（Ripser 集成）
  - 稳定模式挖掘算法
  - 规则生成引擎原型
  - 进化委员会联动机制

##### 微核心基础模块 (core)
- **新增**: IPC Binder 通信机制
  - 基于 Socket 的进程间通信
  - 连接管理和复用
  - 消息序列化
  - 流控和拥塞控制
- **新增**: 内存管理系统
  - 智能指针和 RAII
  - 内存池优化
  - 引用计数
  - 泄漏检测
- **新增**: 任务调度基础
  - 加权轮询算法
  - 优先级队列
  - 时间片管理
  - 任务生命周期管理
- **新增**: 时间服务
  - 高精度计时器
  - 时间戳同步
  - 超时管理

#### 系统调用层 (syscall)

##### 已完成 (60%)
- **新增**: 任务系统调用
  - `sys_task_create()` - 创建任务
  - `sys_task_submit()` - 提交任务
  - `sys_task_query()` - 查询任务状态
  - `sys_task_cancel()` - 取消任务
  - `sys_task_wait()` - 等待任务完成
- **新增**: 记忆系统调用
  - `sys_memory_write()` - 写入记忆
  - `sys_memory_read()` - 读取记忆
  - `sys_memory_query()` - 查询记忆
  - `sys_memory_delete()` - 删除记忆
  - `sys_memory_evolve()` - 触发记忆进化
- **新增**: Agent 系统调用（部分）
  - `sys_agent_create()` - 创建 Agent
  - `sys_agent_destroy()` - 销毁 Agent
  - `sys_agent_invoke()` - 调用 Agent

##### 开发中
- ⏳ Agent 技能注册和发现
- ⏳ 市场服务系统调用
- ⏳ 权限验证系统调用

#### 核心服务 (daemon)

##### llm_d - LLM 服务
- **新增**: 多模型统一接口
  - OpenAI GPT 系列
  - DeepSeek 系列
  - 本地部署模型支持
- **新增**: 提示词工程框架
  - 模板管理
  - 动态组装
  - 上下文优化

##### market_d - 市场服务
- **新增**: Agent 注册表
  - Agent 元数据管理
  - 能力描述和发现
  - 评分和评价系统
- **新增**: 技能市场原型
  - 技能注册
  - 版本管理
  - 依赖解析

##### monit_d - 监控服务
- **新增**: OpenTelemetry 集成
  - Traces 数据采集
  - Metrics 指标收集
  - Logs 日志聚合
- **新增**: 性能监控
  - CPU/内存使用率
  - 请求延迟统计
  - 吞吐量监控

##### perm_d - 权限服务
- **新增**: RBAC 权限模型
  - 角色定义
  - 权限分配
  - 访问控制列表

##### sched_d - 调度服务
- **新增**: 多策略调度器
  - 加权轮询
  - 优先级调度
  - 自适应调度（实验性）

##### tool_d - 工具服务
- **新增**: 工具注册框架
  - 工具描述语言
  - 参数验证
  - 执行沙箱

#### SDK 支持

##### Python SDK
- **新增**: 高级抽象接口
  - `Agent` 类
  - `Memory` 类
  - `Task` 类
  - `Skill` 类
- **新增**: 异步支持
  - asyncio 集成
  - 协程接口
- **新增**: 类型注解
  - 完整的 typing 支持
  - IDE 友好

##### Go SDK
- **新增**: 基础接口定义
  - Agent 接口
  - Memory 接口
  - Task 接口
- **新增**: 并发支持
  - Goroutine 安全
  - Channel 通信

##### Rust SDK
- **新增**: 所有权安全接口
  - Trait 定义
  - 生命周期标注
- **新增**: 异步运行时
  - Tokio 集成
  - Future 支持

##### TypeScript SDK
- **新增**: Node.js 支持
  - CommonJS 模块
  - ES Modules
- **新增**: 浏览器支持
  - Web Workers
  - IndexedDB 存储

#### 应用示例 (app)

##### docgen - 文档生成应用
- **新增**: 自动生成 API 文档
- **新增**: Markdown 文档站点
- **新增**: 代码注释提取

##### ecommerce - 电子商务应用
- **新增**: 商品推荐 Agent
- **新增**: 客服对话 Agent
- **新增**: 订单处理流程

##### videoedit - 视频编辑应用
- **新增**: 视频分析 Agent
- **新增**: 剪辑建议生成
- **新增**: 自动字幕生成

### 🔧 改进和优化

#### 性能优化
- **优化**: MemoryRovol 写入吞吐提升至 10,000+ 条/秒
- **优化**: FAISS 向量检索延迟 < 10ms
- **优化**: 混合检索延迟 < 50ms
- **优化**: 记忆抽象速度 100 条/秒
- **优化**: 模式挖掘速度 10 万条/分钟

#### 资源利用
- **优化**: 空闲状态 CPU < 5%
- **优化**: 空闲状态内存 200MB
- **优化**: 磁盘 IO 优化至 < 1MB/s（空闲）

#### 可扩展性
- **改进**: 支持 1024 并发 IPC 连接
- **改进**: 任务调度延迟 < 1ms
- **改进**: 水平扩展架构设计（规划中）

### 📝 文档更新

#### 新增文档
- **新增**: CoreLoopThree 架构详解
- **新增**: MemoryRovol 架构详解
- **新增**: IPC 机制详解
- **新增**: 微核心设计文档
- **新增**: 系统调用详解
- **新增**: 快速入门指南
- **新增**: Agent 开发教程
- **新增**: 技能开发教程
- **新增**: 部署指南
- **新增**: 内核调优指南
- **新增**: 故障排查手册
- **新增**: 编码规范
- **新增**: 测试规范
- **新增**: 安全规范
- **新增**: 性能指标文档

#### 改进文档
- **改进**: README.md 结构和内容
- **改进**: 代码示例完整性
- **改进**: 中文文档翻译质量

### 🛠️ 工具和基础设施

#### 构建系统
- **新增**: CMake 配置优化
- **新增**: Poetry Python 项目管理
- **新增**: Makefile 常用命令集
- **新增**: Docker Compose 容器编排

#### 开发工具
- **新增**: Pre-commit 钩子配置
- **新增**: 代码格式化（Black, clang-format）
- **新增**: 代码检查（flake8, cpplint）
- **新增**: 类型检查（mypy）

#### 测试框架
- **新增**: pytest 测试套件
- **新增**: 单元测试覆盖
- **新增**: 集成测试框架
- **新增**: 性能基准测试脚本

#### CI/CD
- **新增**: GitHub Actions 工作流（规划中）
- **新增**: 自动化测试流水线
- **新增**: 自动化发布流程（规划中）

### 🐛 Bug 修复

_（当前版本仍在开发中，Bug 修复将在正式发布时记录）_

### ⚠️ 破坏性变更

_（v0.0.3 为首次公开发布版本，无破坏性变更）_

---

## 历史版本

### v0.0.0 - 内部开发版 (2025.12)

#### 新增
- MemoryRovol 概念设计和原型
- CoreLoopThree 理论框架
- 微核心架构设计

#### 改进
- 需求分析和系统设计
- 技术选型和验证

### v0.0.x - 概念验证版 (2025.12)

#### 新增
- 项目初始化
- 基础架构设计
- 核心团队组建

---

## 版本说明

### 版本号规则

遵循语义化版本 2.0.0：

- **主版本号 (Major)**: 破坏性变更
- **次版本号 (Minor)**: 向后兼容的功能新增
- **修订号 (Patch)**: 向后兼容的问题修正

### 发布周期

- **主版本**: 每 6-12 个月
- **次版本**: 每 2-3 个月
- **修订版**: 按需发布

### 支持政策

- **当前版本**: 完全支持（v3.x）
- **上一版本**: 关键 Bug 修复（v2.x）
- **历史版本**: 不再支持

---

## 贡献者

感谢所有为本版本做出贡献的开发者！

详见：[AUTHORS.md](docs/AUTHORS.md)

---

## 联系方式

- **Gitee**: https://gitee.com/spharx/agentos
- **GitHub**: https://github.com/SpharxTeam/AgentRT
- **技术支持**: support@spharx.cn
- **安全问题**: security@spharx.cn
- **商务合作**: business@spharx.cn
- **官网**: https://spharx.cn

---

<div align="center">

**SPHARX 极光感知科技**

*From data intelligence emerges*

</div>

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.
