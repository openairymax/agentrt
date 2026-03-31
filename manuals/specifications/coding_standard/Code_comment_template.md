# AgentOS 代码注释与合规规范


**版本**: Doc V1.7  
**更新日期**: 2026-03-31  
**作者**: LirenWang  
**适用范围**: AgentOS 所有编程语言的代码注释  
**理论基础**: 工程两论、五维正交系统、双系统认知理论、代码合规框架  
**原则映射**: A-1至A-4（设计美学）、C-1至C-4（认知工程）、E-1至E-8（工程基础设施）、D-1至D-4（安全工程）、合规性原则

---

## 一、概述

### 1.1 编制目的

代码注释是软件工程中不可或缺的组成部分，是代码可读性、可维护性、合规性和协作效率的关键保障。本文档为 AgentOS 项目提供统一的代码注释与合规规范，旨在：

1. **统一注释风格**：确保整个项目注释格式一致，降低认知负担
2. **保障代码合规**：规范版权声明、许可证标识、第三方代码引用，确保法律合规性
3. **提升代码质量**：通过规范化的注释促进开发者深入思考代码设计
4. **加速团队协作**：清晰的注释使代码意图一目了然，减少沟通成本
5. **支持自动化工具**：规范的注释可被 Doxygen、Sphinx、TypeDoc 等工具解析生成文档
6. **确保开源合规**：遵循 SPDX 标准，保障项目开源过程的合法性与透明性

### 1.2 核心理念与架构映射

AgentOS 的代码注释规范建立在"工程两论"、五维正交系统和现代合规框架的基础之上，体现了以下核心设计理念：

#### 1.2.1 基于工程控制论的注释理念
- **反馈闭环设计**：注释构成代码的"自解释系统"，为开发者提供实时的设计意图反馈，形成"编写-注释-理解"的认知闭环
- **稳定性保障**：公共 API 注释作为系统接口的"控制契约"，确保模块间交互的稳定性和可预测性（映射原则：S-3接口稳定性）
- **前馈预测性设计**：基于历史注释模式的趋势预测，提前调整注释策略和内容结构

#### 1.2.2 基于系统工程的注释理念  
- **层次分解方法**：注释体系遵循五维正交系统，从系统观（架构意图）到工程观（实现细节）提供不同层次的解释
- **模块化文档**：注释与代码结构严格对应，支持"按需加载"的文档认知模式（映射原则：S-2模块化设计）
- **总体设计部**：注释作为代码的总体设计部，只做解释说明、不做具体实现

#### 1.2.3 基于双系统认知理论的注释理念
- **System 1 注释**：简单接口的注释应直观易懂，支持快速认知（快速路径），如简单的 getter/setter
- **System 2 注释**：复杂算法的注释需提供深度原理说明，支持深度思考（慢速路径），如核心算法实现

#### 1.2.4 基于设计美学的注释理念
- **极简主义**（A-1原则）：注释应简洁精炼，避免冗余信息污染代码空间
- **细节关注**（A-2原则）：关键设计决策必须在注释中明确记录，传承工程智慧
- **人文关怀**（A-3原则）：注释应设身处地为读者着想，提供必要的上下文和背景
- **完美主义**（A-4原则）：注释质量应与代码质量同等要求，追求文档的完美无瑕

#### 1.2.5 基于合规框架的注释理念
- **版权清晰**：每个源文件必须明确版权归属，遵循 SPDX 标准
- **许可证透明**：许可证标识必须准确、完整，避免兼容性问题
- **第三方合规**：引入第三方代码必须保留原始声明，明确修改记录
- **风险控制**：安全敏感代码必须标注安全等级和风险提示

#### 1.2.6 注释的多重角色
- **注释即认知桥梁**：连接代码实现与设计意图，降低认知负荷
- **注释即知识传承**：记录技术决策背后的思考过程，避免知识丢失
- **注释即质量指标**：注释完整度直接反映代码的工程成熟度
- **注释即协作协议**：为团队协作提供统一的沟通语言和标准
- **注释即合规凭证**：证明代码的合法来源和授权状态

### 1.3 适用范围

| 语言 | 注释风格 | 文档生成工具 | 合规检查工具 |
|------|----------|--------------|--------------|
| C/C++ | Doxygen + SPDX | Doxygen | clang-tidy, REUSE |
| Python | Google/NumPy Docstring + SPDX | Sphinx | pydocstyle, licensecheck |
| JavaScript/TypeScript | JSDoc/TSDoc + SPDX | TypeDoc | ESLint, license-checker |
| Go | GoDoc + SPDX | go doc | go-licenses, govulncheck |
| Rust | Rustdoc + SPDX | cargo doc | cargo-audit, cargo-deny |
| 配置/数据文件 | 语言特定格式 | - | 相应格式检查工具 |

---

## 二、代码合规注释规范

基于《代码合规建议.md》（总线审查版）的核心要求，本章节系统规定代码注释中必须包含的合规信息，确保项目符合开源合规、安全合规等法律法规要求。

### 2.1 文件头部合规注释

所有自研源代码文件必须在文件头部添加版权和许可证声明，遵循 SPDX（Software Package Data Exchange）标准，这是开源项目合规性的基础要求。

#### 2.1.1 自研代码文件头部声明格式

**C/C++ 文件示例**：
```c
/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file memory_manager.h
 * @brief AgentOS 内存管理器接口
 * 
 * 提供内核级内存分配、释放、监控功能。采用 slab 分配器
 * 优化小对象分配性能，支持 NUMA 感知内存布局。
 * 
 * @author AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see atoms/corekern/memory/
 */

#ifndef AGENTOS_MEMORY_MANAGER_H
#define AGENTOS_MEMORY_MANAGER_H

#include "agentos_types.h"

#ifdef __cplusplus
extern "C" {
#endif
```

**Python 文件示例**：
```python
"""
Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
SPDX-License-Identifier: Apache-2.0

AgentOS 任务调度模块

本模块提供任务调度核心功能，包括任务提交、状态管理、
优先级队列和依赖解析。调度器采用双系统架构：
- System 1：快速路径，处理简单任务
- System 2：深度路径，处理复杂任务

典型用法:
    from agentos.scheduler import TaskScheduler
    
    scheduler = TaskScheduler()
    task_id = scheduler.submit(task_plan)
    result = scheduler.wait(task_id)

版本: 2.0.0
作者: AgentOS Team
日期: 2026-03-30
"""

from typing import Optional, List, Dict, Any
```

**TypeScript/JavaScript 文件示例**：
```typescript
/**
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @fileoverview AgentOS 任务调度模块
 * 
 * 提供任务调度核心功能，包括任务提交、状态管理、
 * 优先级队列和依赖解析。
 * 
 * @module agentos/scheduler
 * @author AgentOS Team
 * @version 2.0.0
 * @date 2026-03-30
 */

import { TaskPlan, TaskResult, TaskStatus } from './types';
```

#### 2.1.2 声明格式规范

1. **版权声明格式**：
   - **标准格式**：`Copyright (C) [第一次发布年份]-[末次修改年份] [版权所有者名称]`
   - **SPDX格式**：`SPDX-FileCopyrightText: [第一次发布年份]-[末次修改年份] [版权所有者名称]`
   - **示例**：`SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.`

2. **许可证标识格式**：
   - **标准格式**：`SPDX-License-Identifier: [许可证标识符]`
   - **常见标识符**：`Apache-2.0`、`MIT`、`GPL-3.0-or-later`、`BSD-3-Clause`
   - **多许可证**：`SPDX-License-Identifier: (Apache-2.0 OR MIT)`

3. **声明位置要求**：
   - 必须位于文件头部，在任何其他内容之前
   - 对于支持多行注释的语言，使用多行注释格式
   - 对于不支持多行注释的文件格式（如JSON），在单行注释中添加

#### 2.1.3 特殊文件处理

1. **配置文件（JSON/YAML/XML）**：
   ```json
   {
     "_comment": "SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.",
     "_license": "SPDX-License-Identifier: Apache-2.0",
     "name": "agentos-config",
     "version": "2.0.0"
   }
   ```

2. **脚本文件（Shell/Batch）**：
   ```bash
   #!/bin/bash
   # SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
   # SPDX-License-Identifier: Apache-2.0
   #
   # AgentOS 系统初始化脚本
   # 版本: 2.0.0
   # 日期: 2026-03-30
   ```

### 2.2 第三方代码合规注释

引入第三方开源代码时，必须严格遵守合规要求，保留原始版权和许可证声明。

#### 2.2.1 原样引入第三方代码

```c
/*
 * 以下代码来自第三方开源项目 [项目名称]，版本 [版本号]
 * 原版权声明保留如下：
 * 
 * Copyright (C) 2010-2024 [原作者/组织]
 * SPDX-FileCopyrightText: 2010-2024 [原作者/组织]
 * SPDX-License-Identifier: [原许可证标识]
 * 
 * 此部分代码按原样引入，未作任何修改。
 * 原代码仓库: [仓库URL]
 * 引入日期: 2026-03-30
 * 引入原因: [简要说明原因]
 */
```

#### 2.2.2 修改后引入第三方代码

```c
/*
 * 以下代码基于第三方开源项目 [项目名称] 修改而来
 * 原版权声明保留如下：
 * 
 * Copyright (C) 2010-2024 [原作者/组织]
 * SPDX-FileCopyrightText: 2010-2024 [原作者/组织]
 * SPDX-License-Identifier: [原许可证标识]
 * 
 * 修改声明:
 * This file was modified by SPHARX Ltd. on 2026-03-30.
 * 主要修改内容:
 * 1. 添加了 [功能A] 支持
 * 2. 优化了 [性能B]
 * 3. 修复了 [问题C]
 * 
 * 原代码仓库: [仓库URL]
 * 修改依据: [需求文档/Issue链接]
 */
```

#### 2.2.3 第三方代码合规检查清单

| 检查项 | 要求 | 验证方法 |
|--------|------|----------|
| 版权声明 | 必须原样保留 | 比对原始文件 |
| 许可证标识 | 必须原样保留 | 检查SPDX标识 |
| 修改声明 | 如有修改必须添加 | 检查修改记录 |
| 许可证兼容性 | 与项目许可证兼容 | 使用许可证兼容性矩阵 |
| 引入记录 | 记录引入原因和日期 | 检查版本控制日志 |

### 2.3 许可证兼容性注释

对于涉及多个许可证的代码，必须在注释中明确说明兼容性，确保法律合规。

```c
/*
 * 许可证兼容性说明：
 * 
 * 本文件包含以下许可证代码：
 * 1. 主要部分: Apache-2.0 (AgentOS 自研代码)
 * 2. 算法库部分: BSD-3-Clause (第三方库 [库名])
 * 3. 工具函数: MIT (第三方库 [库名])
 * 
 * 兼容性分析：
 * - Apache-2.0 与 BSD-3-Clause 兼容 ✅
 * - Apache-2.0 与 MIT 兼容 ✅
 * - 组合许可证: Apache-2.0 AND BSD-3-Clause AND MIT
 * 
 * 使用限制：
 * - 必须保留所有版权声明
 * - 必须提供原始许可证文本副本
 * - 修改必须标注修改记录
 */
```

### 2.4 安全合规注释

安全敏感代码必须添加安全合规注释，明确安全等级、威胁模型和防护措施。

#### 2.4.1 密码学相关代码

```c
/**
 * @brief AES-256-GCM 加密函数
 * 
 * 使用硬件加速的 AES-GCM 模式进行数据加密。
 * 符合 NIST SP 800-38D 标准，提供认证加密。
 * 
 * @security 安全等级: CRITICAL
 * @security 密钥管理: 使用内核安全密钥环存储
 * @security 随机数生成: 使用硬件 RDRAND 指令
 * @security 侧信道防护: 恒定时间实现，抗时序攻击
 * 
 * @compliance 符合标准:
 *   - FIPS 140-3 Level 2
 *   - PCI DSS v4.0
 *   - GDPR 第32条（安全处理）
 * 
 * @audit 审计记录:
 *   - 2026-03-15: 第三方安全审计通过
 *   - 2026-03-20: 模糊测试覆盖率达到100%
 * 
 * @see 安全设计指南: Security_design_guide.md
 */
```

#### 2.4.2 数据隐私相关代码

```python
def process_user_data(user_data: Dict[str, Any]) -> Dict[str, Any]:
    """处理用户数据，确保符合隐私法规。
    
    对用户数据进行匿名化处理，移除直接标识符，
    应用差分隐私保护敏感信息。
    
    Args:
        user_data: 原始用户数据字典
        
    Returns:
        处理后的匿名化数据
        
    Privacy:
        - 数据分类: PII（个人身份信息）
        - 处理依据: 用户明示同意
        - 保留期限: 30天（符合数据最小化原则）
        - 跨境传输: 使用标准合同条款（SCCs）
        
    Compliance:
        - GDPR: 第5条（合法性、公平性、透明性）
        - CCPA: 第1798.100条（消费者隐私权）
        - PIPL: 第22条（个人信息处理规则）
        
    Security:
        - 加密存储: AES-256 加密
        - 访问控制: RBAC 基于角色的访问控制
        - 审计日志: 完整的数据访问记录
        
    Example:
        >>> processed = process_user_data({"name": "张三", "email": "zhangsan@example.com"})
        >>> print(processed["anonymous_id"])
        "user_9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
    """
```

### 2.5 专利合规注释

涉及第三方专利技术的代码必须添加专利合规注释，明确专利使用状态和合规措施。

#### 2.5.1 专利技术使用声明

```c
/**
 * @brief 基于 LZ4 压缩算法优化的数据压缩函数
 * 
 * 使用改进的 LZ4 压缩算法，针对特定数据类型优化哈希表配置和匹配策略。
 * 相比标准 LZ4，压缩率提升 15-20%，解压速度保持相近水平。
 * 
 * @patent 专利声明:
 *   - 基础算法: LZ4 压缩算法（专利已过期）
 *   - 优化技术: 自研优化，不涉及第三方专利
 *   - 专利检查: 已进行专利前置审查，未发现侵权风险
 *   
 * @patent 第三方专利排查:
 *   1. US Patent 5,838,919 (数据压缩方法) - 已过期 ✅
 *   2. US Patent 6,009,177 (LZ派生算法) - 已过期 ✅  
 *   3. CN Patent ZL201110123456.7 (中文优化) - 不相关 ❌
 *   4. EP Patent 1234567 (哈希优化) - 已规避 ✅
 *   
 * @patent 合规措施:
 *   1. 使用已进入公共领域的算法基础
 *   2. 对核心优化技术进行专利检索
 *   3. 建立专利监控机制，定期更新专利状态
 *   4. 提供专利替代方案（如 GZIP 回退）
 *   
 * @warning 专利风险提示:
 *   - 禁止在专利保护期内国家/地区商业使用（如有相关专利）
 *   - 定期检查专利状态变化
 *   - 发现潜在专利冲突时立即通知法务部门
 *   
 * @see 专利合规指南: patent_compliance.md
 * @see LZ4 开源项目: https://github.com/lz4/lz4
 */
int lz4_optimized_compress(const uint8_t* src, uint8_t* dst, int src_size);
```

### 2.6 商业秘密合规注释

涉及商业秘密或保密技术的代码必须添加商业秘密合规注释，明确保密要求和披露限制。

#### 2.6.1 商业秘密保护声明

```c
/**
 * @brief 自研高性能数据压缩算法
 * 
 * 实现基于改进LZ77算法的高性能数据压缩，采用专有的哈希匹配策略
 * 和滑动窗口优化技术。相比标准DEFLATE算法，压缩率提升15-25%，
 * 解压速度保持相同水平。
 * 
 * @trade_secret 商业秘密声明:
 *   
 *   **核心商业秘密**:
 *   1. 哈希匹配策略: 专利待申请，目前作为商业秘密保护
 *   2. 滑动窗口优化: 专有的内存访问模式和预取算法
 *   3. 编码优化技巧: 熵编码参数动态调整技术
 *   4. 硬件适配方案: 针对不同CPU架构的SIMD指令优化
 *   
 *   **保密等级**: CONFIDENTIAL
 *   **保密期限**: 永久（除非通过专利公开）
 *   **知悉范围**: 核心算法团队、授权合作伙伴
 *   
 * @trade_secret 保护措施:
 *   1. **访问控制**: 仅授权人员可访问源代码
 *   2. **代码混淆**: 关键算法部分使用代码混淆
 *   3. **水印技术**: 在输出中添加数字水印追踪泄露
 *   4. **审计日志**: 完整的代码访问和修改记录
 *   5. **员工协议**: 所有接触人员签署保密协议
 *   
 * @trade_secret 披露限制:
 *   1. 禁止开源分发商业秘密代码
 *   2. 禁止向未授权第三方透露算法细节
 *   3. 技术交流需脱敏处理（移除关键参数）
 *   4. 学术发表需内部技术评审委员会批准
 *   
 * @warning 法律责任:
 *   - 违反保密协议可能面临刑事指控
 *   - 商业秘密泄露可能导致重大经济损失
 *   - 侵权方可能被要求支付惩罚性赔偿
 *   
 * @see 商业秘密保护政策: trade_secret_policy.md
 * @see 员工保密协议范本: templates/nda.md
 */
void high_performance_compress(const uint8_t* input, uint8_t* output, int input_size, int* output_size);
```

---

## 三、通用注释原则

### 3.1 注释的必要性判断

基于工程控制论的反馈闭环原理，注释应与代码复杂度成正比，形成自适应的注释策略。

**必须注释的场景**：
- **公共 API 接口**：函数、类、模块的对外接口（映射原则：S-3接口稳定性）
- **复杂算法**：非直观逻辑、数学推导、优化技巧（映射原则：C-4认知辅助）
- **性能关键代码**：性能优化、内存管理、并发控制（映射原则：E-6性能工程）
- **安全敏感代码**：身份验证、数据加密、权限检查（映射原则：D-2安全隔离）
- **平台特定代码**：操作系统、硬件架构、编译器相关（映射原则：E-7跨平台）
- **临时解决方案**：TODO、FIXME、HACK等临时代码（映射原则：E-8可测试性）
- **已废弃接口**：DEPRECATED标记的旧接口（映射原则：S-4演化适应）
- **合规相关代码**：版权、许可证等（合规要求）

**无需注释的场景**：
- **简单的 getter/setter**：属性访问方法，代码意图明显
- **标准库调用包装**：对标准库函数的简单包装
- **代码本身已足够清晰**：命名规范、结构清晰的代码
- **自动生成的代码**：由工具生成的代码（需标注生成来源）
- **重复模式代码**：项目中已建立模式的重复代码

### 3.2 注释内容要素

完整的函数/方法注释应包含以下要素，形成系统化的文档结构：

| 要素 | 必要性 | 说明 | 示例 |
|------|--------|------|------|
| 简要描述 | 必须 | 一句话说明功能，System 1认知 | `分配指定大小的内存块` |
| 详细描述 | 推荐 | 解释设计意图、算法原理，System 2认知 | `从内核内存池中分配连续内存...` |
| 参数说明 | 必须 | 每个参数的含义、约束、单位 | `@param size 字节数，必须大于0` |
| 返回值说明 | 必须 | 返回值含义、成功/失败条件 | `@return 成功返回指针，失败返回NULL` |
| 异常说明 | 必须 | 可能抛出的异常及触发条件 | `@throws ValueError 当参数无效时` |
| 线程安全 | 推荐 | 是否线程安全、并发使用注意事项 | `@note 线程安全：可多线程调用` |
| 使用示例 | 推荐 | 典型用法代码示例，降低使用门槛 | `@example void* ptr = alloc(1024);` |
| 合规信息 | 必须 | 版权、许可证、安全等级等 | `@compliance 安全等级: HIGH` |
| 注意事项 | 可选 | 边界条件、性能考量、资源管理 | `@note 调用者负责释放内存` |
| 相关接口 | 可选 | 相关函数、参考文档链接 | `@see agentos_mem_free()` |
| 版本历史 | 可选 | 重大修改记录、版本变更 | `@since 2.0.0` |
| 作者信息 | 可选 | 创建者、维护者、联系方式 | `@author AgentOS Team` |

### 3.3 注释语言规范

- **中文为主**：注释使用规范中文，技术术语保留英文原词
- **术语一致**：使用项目统一术语表中的术语，避免歧义
- **语法规范**：符合现代汉语语法，避免口语化、网络用语
- **避免冗余**：不重复代码已表达的信息
- **合规准确**：版权年份、许可证标识必须准确无误

---

## 四、C/C++ 注释规范（Doxygen + SPDX 风格）

### 4.1 文件头注释模板

```c
/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file memory_manager.h
 * @brief AgentOS 内存管理器接口
 * 
 * 提供内核级内存分配、释放、监控功能。采用 slab 分配器
 * 优化小对象分配性能，支持 NUMA 感知内存布局。
 * 
 * @author AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see atoms/corekern/memory/
 */

#ifndef AGENTOS_MEMORY_MANAGER_H
#define AGENTOS_MEMORY_MANAGER_H

#include "agentos_types.h"

#ifdef __cplusplus
extern "C" {
#endif
```

### 4.2 函数注释模板

```c
/**
 * @brief 分配指定大小的内存块
 * 
 * 从内核内存池中分配一块连续内存。分配器会根据请求大小
 * 自动选择最优的分配策略：
 * - 小于 256 字节：使用 slab 分配器
 * - 256 字节 ~ 4KB：使用 buddy 分配器
 * - 大于 4KB：直接向操作系统申请
 * 
 * @param size 请求分配的字节数，必须大于 0
 * @param flags 分配标志位，见 agentos_mem_flags_t
 * @return 成功返回内存块指针，失败返回 NULL
 * 
 * @compliance 安全等级: HIGH
 * @compliance 许可证: Apache-2.0
 * 
 * @note 调用者负责释放内存（调用 agentos_mem_free）
 * @note 返回的内存块已按 16 字节对齐
 * 
 * @warning 不要在信号处理函数中调用此函数
 * 
 * @see agentos_mem_free()
 * @see agentos_mem_realloc()
 * 
 * @example
 * // 基本用法
 * void* buffer = agentos_mem_alloc(1024, AGENTOS_MEM_FLAG_ZERO);
 * if (buffer == NULL) {
 *     log_error("Memory allocation failed");
 *     return AGENTOS_ERR_NO_MEMORY;
 * }
 * // 使用 buffer...
 * agentos_mem_free(buffer);
 */
void* agentos_mem_alloc(size_t size, uint32_t flags);
```

### 4.3 结构体注释模板

```c
/**
 * @brief 任务执行上下文结构体
 * 
 * 封装任务执行所需的全部上下文信息，包括输入数据、
 * 执行配置和输出缓冲区。此结构体为不透明类型，
 * 外部代码应通过访问函数操作。
 * 
 * @compliance 数据分类: 内部数据结构，不包含用户敏感信息
 */
typedef struct agentos_task_context {
    uint64_t task_id;           /**< 任务唯一标识符 */
    char* input_data;           /**< 输入数据指针，内部管理 */
    size_t input_size;          /**< 输入数据大小（字节） */
    char* output_buffer;        /**< 输出缓冲区指针 */
    size_t output_capacity;     /**< 输出缓冲区容量 */
    size_t output_size;         /**< 实际输出大小 */
    uint32_t flags;             /**< 执行标志位 */
    int32_t error_code;         /**< 错误码，0 表示成功 */
    struct timespec start_time; /**< 任务开始时间 */
    struct timespec end_time;   /**< 任务结束时间 */
} agentos_task_context_t;
```

---

## 五、Python 注释规范（Google Docstring + SPDX 风格）

### 5.1 模块注释模板

```python
"""
Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
SPDX-License-Identifier: Apache-2.0

AgentOS 任务调度模块

本模块提供任务调度核心功能，包括任务提交、状态管理、
优先级队列和依赖解析。调度器采用双系统架构：
- System 1：快速路径，处理简单任务
- System 2：深度路径，处理复杂任务

典型用法:
    from agentos.scheduler import TaskScheduler
    
    scheduler = TaskScheduler()
    task_id = scheduler.submit(task_plan)
    result = scheduler.wait(task_id)

属性:
    MAX_PRIORITY (int): 最大优先级值，默认为 10
    DEFAULT_TIMEOUT (float): 默认超时时间（秒），默认为 30.0

作者:
    AgentOS Team

版本:
    2.0.0
日期:
    2026-03-30
"""

from typing import Optional, List, Dict, Any
from dataclasses import dataclass
import logging

MAX_PRIORITY: int = 10
DEFAULT_TIMEOUT: float = 30.0
```

### 5.2 类注释模板

```python
class TaskScheduler:
    """任务调度器，管理任务的生命周期和执行。
    
    调度器负责接收任务请求、解析依赖关系、分配执行资源，
    并跟踪任务状态。支持优先级队列、超时控制和错误重试。
    
    调度器是线程安全的，可在多线程环境中使用。
    
    合规信息:
        版权: SPHARX Ltd. (2025-2026)
        许可证: Apache-2.0
        安全等级: MEDIUM
    
    属性:
        name (str): 调度器名称，用于日志标识
        max_workers (int): 最大工作线程数
        queue_size (int): 当前队列长度
        
    示例:
        >>> scheduler = TaskScheduler(name="main", max_workers=4)
        >>> task_id = scheduler.submit(plan)
        >>> result = scheduler.wait(task_id, timeout=60)
        >>> print(result.status)
        'completed'
    
    注意:
        使用完毕后应调用 shutdown() 方法释放资源，
        或使用上下文管理器自动管理生命周期。
    """
    
    def __init__(self, name: str = "default", max_workers: int = 4) -> None:
        """初始化任务调度器。
        
        Args:
            name: 调度器名称，用于日志和监控标识
            max_workers: 最大工作线程数，必须为正整数
            
        Raises:
            ValueError: 当 max_workers <= 0 时抛出
            
        合规:
            数据保护: 不收集用户个人信息
        """
        pass
```

---

## 六、JavaScript/TypeScript 注释规范（JSDoc/TSDoc + SPDX 风格）

### 6.1 文件头注释模板

```typescript
/**
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @fileoverview AgentOS 任务调度模块
 * 
 * 提供任务调度核心功能，包括任务提交、状态管理、
 * 优先级队列和依赖解析。
 * 
 * @module agentos/scheduler
 * @author AgentOS Team
 * @version 2.0.0
 * @date 2026-03-30
 * 
 * @example
 * import { TaskScheduler } from 'agentos/scheduler';
 * 
 * const scheduler = new TaskScheduler({ maxWorkers: 4 });
 * const taskId = await scheduler.submit(plan);
 * const result = await scheduler.wait(taskId);
 */

import { TaskPlan, TaskResult, TaskStatus } from './types';
```

### 6.2 类注释模板

```typescript
/**
 * 任务调度器，管理任务的生命周期和执行。
 * 
 * 调度器负责接收任务请求、解析依赖关系、分配执行资源，
 * 并跟踪任务状态。支持优先级队列、超时控制和错误重试。
 * 
 * 合规信息:
 *   - 版权: SPHARX Ltd. (2025-2026)
 *   - 许可证: Apache-2.0
 *   - 安全等级: MEDIUM
 *   - 数据保护: GDPR 合规
 * 
 * @template T - 任务输入数据类型
 * @template R - 任务输出数据类型
 * 
 * @example
 * const scheduler = new TaskScheduler<TaskInput, TaskOutput>({
 *   name: 'main',
 *   maxWorkers: 4
 * });
 * 
 * const taskId = await scheduler.submit(plan);
 * const result = await scheduler.wait(taskId);
 * console.log(result.status); // 'completed'
 */
export class TaskScheduler<T = unknown, R = unknown> {
    /**
     * 创建任务调度器实例。
     * 
     * @param manager - 调度器配置选项
     * @param manager.name - 调度器名称，用于日志标识
     * @param manager.maxWorkers - 最大工作线程数
     * @param manager.defaultTimeout - 默认超时时间（毫秒）
     * 
     * @throws {Error} 当 maxWorkers <= 0 时抛出
     * 
     * @compliance 数据最小化: 仅收集必要配置信息
     */
    constructor(manager: SchedulerConfig) {
        // 实现...
    }
}
```

---

## 七、特殊场景注释

### 7.1 TODO 注释

```c
// TODO(zhangsan): 添加任务取消功能 [优先级: P1] [预计完成: 2026-04-15]
// 关联 Issue: #1234
// 技术方案: 使用原子操作实现无锁取消
// 测试计划: 添加并发取消测试用例
```

```python
# TODO(lisi): 实现任务重试逻辑 [优先级: P2]
# 关联 PR: #567
# 实现思路: 指数退避 + 抖动算法
# 依赖模块: retry.py (需要先重构)
```

### 7.2 FIXME 注释

```c
// FIXME(wangwu): 并发访问时存在竞态条件 [严重程度: Critical]
// 问题描述: task_id 生成器在多线程环境下可能产生重复ID
// 临时解决方案: 使用线程本地存储
// 永久解决方案: 实现分布式ID生成器
// 影响范围: 所有任务调度相关功能
```

### 7.3 DEPRECATED 注释

```typescript
/**
 * @deprecated 自版本 2.0.0 起废弃
 * 
 * 请使用 `TaskScheduler.submit()` 代替。
 * 此方法将在 3.0.0 版本中移除。
 * 
 * @deprecation_reason 新的提交接口支持更丰富的选项配置
 * @replacement TaskScheduler.submit()
 * @removal_version 3.0.0
 * 
 * 迁移指南:
 * 旧代码: submitTask(plan)
 * 新代码: scheduler.submit(plan, options)
 * 
 * @see TaskScheduler.submit
 */
submitTask(plan: TaskPlan): string {
    console.warn('submitTask is deprecated, use scheduler.submit()');
    return this.submit(plan);
}
```

### 7.4 HACK 注释

```c
// HACK(zhaoliu): 临时绕过第三方库的内存泄漏问题
// 第三方库: libevent 2.1.12 (已知bug: #789)
// 问题链接: https://github.com/libevent/libevent/issues/789
// 临时方案: 每1000次调用后重启连接
// 永久方案: 升级到 libevent 2.1.13+ (修复版本)
// 监控指标: 内存使用量、连接重启次数
```

### 7.5 SECURITY 注释

```python
# SECURITY: 此函数处理用户输入，必须进行严格的参数验证
# 威胁模型: 攻击者可能注入恶意输入
# 防护措施:
#   1. 输入验证: 白名单过滤
#   2. 输出编码: HTML/URL 编码
#   3. 权限检查: 最小权限原则
# 参考文档: OWASP Input Validation Cheat Sheet
# 测试用例: 包含 SQL 注入、XSS 攻击测试
```

---

## 八、注释质量检查与合规验证

### 8.1 自动化检查工具

| 语言 | 文档生成工具 | 合规检查工具 | 代码质量工具 |
|------|--------------|--------------|--------------|
| C/C++ | Doxygen | REUSE, FOSSA | clang-tidy, cppcheck |
| Python | Sphinx | licensecheck, pip-licenses | pylint, mypy, bandit |
| JavaScript/TypeScript | TypeDoc | license-checker, npm-license | ESLint, TSC, SonarJS |
| Go | go doc | go-licenses, govulncheck | golangci-lint, staticcheck |
| Rust | cargo doc | cargo-deny, cargo-audit | clippy, rustfmt |
| 通用 | - | ScanCode, ORT | - |

### 8.2 检查规则

**必须检查项**：
- 文件头部是否包含SPDX版权和许可证声明
- 公共 API 是否有完整注释
- 参数说明是否完整
- 返回值说明是否准确
- 异常说明是否完整
- 第三方代码是否保留原始声明
- 许可证兼容性是否符合要求

**建议检查项**：
- 注释是否解释"为什么"（设计意图）
- 注释是否与代码同步
- 注释语言是否一致
- 注释格式是否规范
- 安全注释是否完整
- 合规注释是否准确

### 8.3 人工审查清单

| 审查维度 | 审查要点 | 通过标准 |
|----------|----------|----------|
| 合规性 | SPDX声明完整性 | 所有文件包含正确的版权和许可证标识 |
| 准确性 | 注释与代码一致性 | 注释准确描述代码功能和行为 |
| 完整性 | 必需注释要素 | 公共API包含所有必需注释要素 |
| 安全性 | 安全注释完整性 | 安全敏感代码包含完整的安全注释 |
| 可读性 | 注释语言质量 | 中文语法正确，表达清晰 |
| 维护性 | 注释时效性 | 注释与代码同步更新，无过时信息 |

---

## 九、最佳实践

### 9.1 注释编写原则

1. **及时更新**：代码修改时同步更新注释
2. **避免冗余**：不重复代码已表达的信息
3. **保持简洁**：注释应精炼，避免过度描述
4. **使用示例**：复杂接口必须提供使用示例
5. **说明边界**：明确说明参数边界、返回值边界
6. **记录决策**：关键设计决策必须在注释中记录
7. **合规优先**：始终优先考虑合规要求

### 9.2 合规最佳实践

1. **SPDX标准化**：始终使用SPDX标准格式
2. **版权年份更新**：每年更新版权年份范围
3. **许可证兼容性检查**：引入新代码前检查许可证兼容性
4. **第三方代码记录**：详细记录第三方代码来源和修改
5. **安全注释完整**：安全敏感代码必须有完整的安全注释
6. **定期合规审计**：定期进行代码合规审计

### 9.3 常见错误与修正

| 错误类型 | 错误示例 | 正确做法 |
|----------|----------|----------|
| 缺少合规声明 | 无SPDX声明 | 添加完整的SPDX版权和许可证声明 |
| 过时版权年份 | Copyright 2020-2022 | 更新为 Copyright 2025-2026 |
| 错误许可证标识 | SPDX-License-Identifier: MIT | 使用正确的许可证标识 |
| 第三方代码声明缺失 | 直接使用第三方代码 | 添加第三方代码引入声明 |
| 安全注释不完整 | 仅简单标注安全 | 提供完整的安全等级、威胁模型、防护措施 |
| 合规信息不准确 | 错误的专利分类 | 咨询法务部门确定正确的分类 |
| 注释与代码不同步 | 注释描述旧实现 | 更新注释以匹配当前代码 |
| 冗余注释 | `i++; // i 加 1` | 删除无用注释 |
| 误导性注释 | 注释与实际行为矛盾 | 修正注释或代码 |
| 过长注释 | 注释比代码还长 | 重构代码或提取为独立文档 |

---

## 十、参考资料

1. **SPDX 规范**: https://spdx.dev/specifications/
2. **REUSE 合规指南**: https://reuse.software/
3. **Doxygen 官方文档**: https://www.doxygen.nl/manual/
4. **Google Python Style Guide**: https://google.github.io/styleguide/pyguide.html
5. **TypeScript Documentation**: https://www.typescriptlang.org/docs/
6. **Effective Go**: https://golang.org/doc/effective_go
7. **Rust API Guidelines**: https://rust-lang.github.io/api-guidelines/
8. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
9. **AgentOS 统一术语表**: [TERMINOLOGY.md](../TERMINOLOGY.md)
10. **AgentOS 安全设计指南**: [Security_design_guide.md](../Security_design_guide.md)

---

## 更新记录

| 版本 | 日期 | 修改内容 | 修改人 |
|------|------|----------|--------|
| 1.7 | 2026-03-31 | 重新整理，删除AI和出口管制相关内容，精简文档结构 | LirenWang |
| 1.6 | 2026-03-25 | 初始版本，基于工程两论和五维正交系统设计 | AgentOS文档组 |

---

© 2025-2026 SPHARX Ltd. All Rights Reserved.  
SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.  
SPDX-License-Identifier: Apache-2.0  

"From data intelligence emerges."