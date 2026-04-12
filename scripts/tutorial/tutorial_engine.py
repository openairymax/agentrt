#!/usr/bin/env python3
"""
AgentOS 交互式教程引擎

提供命令行和Web两种交互式学习方式，支持渐进式学习路径和实时验证。

Usage:
    python tutorial_engine.py start --role new-contributor
    python tutorial_engine.py next
    python tutorial_engine.py serve --port 8080
"""

import argparse
import json
import os
import sys
import webbrowser
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union
from http.server import HTTPServer, SimpleHTTPRequestHandler
import threading


class TutorialRole(Enum):
    """教程角色"""
    NEW_CONTRIBUTOR = "new-contributor"      # 新贡献者
    MODULE_DEVELOPER = "module-developer"    # 模块开发者
    SYSTEM_INTEGRATOR = "system-integrator"  # 系统集成者


class TutorialStepType(Enum):
    """教程步骤类型"""
    THEORY = "theory"        # 理论学习
    PRACTICE = "practice"    # 实践操作
    EXERCISE = "exercise"    # 练习验证
    QUIZ = "quiz"            # 知识测验
    REVIEW = "review"        # 回顾总结


class StepStatus(Enum):
    """步骤状态"""
    PENDING = "pending"      # 未开始
    IN_PROGRESS = "in_progress"  # 进行中
    COMPLETED = "completed"  # 已完成
    SKIPPED = "skipped"      # 已跳过


@dataclass
class TutorialStep:
    """教程步骤"""
    id: str
    title: str
    description: str
    step_type: TutorialStepType
    content: str  # Markdown格式内容
    duration_minutes: int = 10
    prerequisites: List[str] = field(default_factory=list)
    commands: List[str] = field(default_factory=list)  # 需要运行的命令
    expected_output: str = ""  # 预期输出（用于验证）
    validation_type: str = ""  # 验证类型：command, file, manual
    validation_command: str = ""  # 验证命令
    validation_pattern: str = ""  # 验证模式（正则表达式）
    hints: List[str] = field(default_factory=list)
    resources: List[str] = field(default_factory=list)


@dataclass
class TutorialPath:
    """教程路径"""
    id: str
    title: str
    description: str
    role: TutorialRole
    estimated_hours: int
    steps: List[TutorialStep]
    prerequisites: List[str] = field(default_factory=list)


@dataclass
class UserProgress:
    """用户进度"""
    user_id: str = "default"
    current_path: str = ""
    current_step: int = 0
    step_status: Dict[str, StepStatus] = field(default_factory=dict)
    start_time: str = ""
    last_update: str = ""
    completed_steps: List[str] = field(default_factory=list)
    scores: Dict[str, float] = field(default_factory=dict)


class TutorialEngine:
    """教程引擎"""
    
    def __init__(self, tutorials_dir: Path):
        self.tutorials_dir = tutorials_dir
        self.tutorials: Dict[str, TutorialPath] = {}
        self.progress: Optional[UserProgress] = None
        self._load_tutorials()
    
    def _load_tutorials(self):
        """加载教程"""
        tutorial_files = list(self.tutorials_dir.glob("*.json"))
        
        if not tutorial_files:
            # 创建示例教程
            self._create_sample_tutorials()
            return
        
        for file_path in tutorial_files:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    tutorial = TutorialPath(**data)
                    self.tutorials[tutorial.id] = tutorial
            except Exception as e:
                print(f"⚠️  加载教程失败 {file_path}: {e}")
    
    def _create_sample_tutorials(self):
        """创建示例教程（如果不存在）"""
        print("📝 创建示例教程...")
        
        # 新贡献者教程
        new_contributor_path = TutorialPath(
            id="new-contributor",
            title="新贡献者入门指南",
            description="面向首次接触AgentOS的贡献者，涵盖环境配置、代码结构和第一个PR提交",
            role=TutorialRole.NEW_CONTRIBUTOR,
            estimated_hours=4,
            steps=[
                TutorialStep(
                    id="welcome",
                    title="欢迎来到AgentOS",
                    description="了解AgentOS的基本概念和项目愿景",
                    step_type=TutorialStepType.THEORY,
                    content="""
# 欢迎来到AgentOS！

**AgentOS** 是一个基于**体系并行论 (MCIS)** 理论构建的智能体操作系统。

## 核心概念
- **智能体 (Agent)**: 具有自主决策和执行能力的软件实体
- **技能 (Skill)**: 智能体执行特定任务的能力
- **记忆 (Memory)**: 智能体长期和短期的经验存储
- **核心三层循环 (CoreLoopThree)**: 认知、执行、记忆的协同工作流

## 项目愿景
构建一个**安全、高效、可扩展**的智能体操作系统，支持大规模智能体应用开发。

## 学习目标
完成本教程后，你将能够：
1. 搭建AgentOS开发环境
2. 理解项目代码结构
3. 提交你的第一个Pull Request
                    """,
                    duration_minutes=10,
                    commands=[],
                    validation_type="manual"
                ),
                TutorialStep(
                    id="env-setup",
                    title="环境配置",
                    description="配置开发环境，安装必要工具",
                    step_type=TutorialStepType.PRACTICE,
                    content="""
# 环境配置

## 系统要求
- **操作系统**: Linux/macOS/Windows (WSL2)
- **内存**: 最低8GB，推荐16GB
- **存储**: 最少20GB可用空间

## 安装必要工具

### 1. Git
```bash
# 检查Git是否安装
git --version
```

### 2. Python 3.8+
```bash
python3 --version
pip3 --version
```

### 3. CMake 3.20+
```bash
cmake --version
```

### 4. C/C++ 编译器
```bash
# GCC
gcc --version

# 或 Clang
clang --version
```

## 实践任务
运行以下命令检查你的环境：
```bash
git --version && \
python3 --version && \
cmake --version && \
gcc --version
```

所有命令都应成功执行，显示版本信息。
                    """,
                    duration_minutes=20,
                    commands=["git --version", "python3 --version", "cmake --version", "gcc --version"],
                    validation_type="command",
                    validation_command="git --version && python3 --version && cmake --version && gcc --version",
                    validation_pattern=r"git version.*\npython 3\.[8-9]\..*\ncmake version 3\.2[0-9]\..*\ngcc.*",
                    hints=[
                        "如果缺少某个工具，请参考官方文档安装",
                        "Windows用户建议使用WSL2"
                    ]
                ),
                TutorialStep(
                    id="project-structure",
                    title="项目结构分析",
                    description="了解AgentOS的代码组织结构",
                    step_type=TutorialStepType.THEORY,
                    content="""
# AgentOS 项目结构

## 主要目录
```
agentos/
├── atoms/                    # 核心原子模块
│   ├── coreloopthree/       # 认知三层循环
│   ├── memoryrovol/         # 记忆系统
│   └── syscall/             # 系统调用接口
├── daemon/                  # 守护进程服务
├── include/                 # 公共头文件
├── scripts/                 # 开发运维工具
└── docs/                    # 文档
```

## 关键文件
- **CMakeLists.txt**: 构建系统配置
- **ARCHITECTURAL_PRINCIPLES.md**: 架构设计原则
- **Capital_Specifications/**: 项目规范文档

## 实践任务
浏览项目目录，熟悉关键文件：
```bash
ls -la agentos/
find agentos -name "*.md" | head -10
```

了解项目结构有助于你快速定位代码和文档。
                    """,
                    duration_minutes=15,
                    commands=["ls -la", "find . -name '*.md' | head -10"],
                    validation_type="manual"
                ),
                TutorialStep(
                    id="first-build",
                    title="首次构建",
                    description="构建AgentOS项目，验证环境配置",
                    step_type=TutorialStepType.PRACTICE,
                    content="""
# 首次构建AgentOS

## 构建步骤

### 1. 进入项目目录
```bash
cd agentos
```

### 2. 创建构建目录
```bash
mkdir -p build && cd build
```

### 3. 配置CMake
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### 4. 编译项目
```bash
make -j$(nproc)  # Linux/macOS
# 或
make -j%NUMBER_OF_PROCESSORS%  # Windows
```

## 验证构建
构建成功后，你会看到类似输出：
```
[100%] Built target agentos
```

## 实践任务
按照上述步骤构建AgentOS项目。

**注意**: 首次构建可能需要较长时间（10-30分钟），具体取决于你的系统性能。
                    """,
                    duration_minutes=30,
                    commands=["mkdir -p build", "cd build", "cmake .. -DCMAKE_BUILD_TYPE=Debug", "make -j4"],
                    validation_type="file",
                    validation_pattern=r"build/.*\.(so|dll|a|lib)",
                    hints=[
                        "如果构建失败，检查错误信息并修复环境问题",
                        "可以尝试减少并行编译任务数：make -j2"
                    ]
                ),
                TutorialStep(
                    id="first-pr",
                    title="第一个Pull Request",
                    description="学习如何提交你的第一个贡献",
                    step_type=TutorialStepType.EXERCISE,
                    content="""
# 提交第一个Pull Request

## 贡献流程

### 1. Fork 项目
- 访问 https://github.com/SpharxTeam/AgentOS
- 点击右上角的 "Fork" 按钮

### 2. 克隆你的分支
```bash
git clone https://github.com/YOUR_USERNAME/AgentOS.git
cd AgentOS
```

### 3. 创建特性分支
```bash
git checkout -b fix-typo-docs
```

### 4. 进行修改
例如，修复文档中的拼写错误：
```bash
# 查找可能的拼写错误
grep -r "teh" docs/ || true
grep -r "adn" docs/ || true
```

### 5. 提交更改
```bash
git add .
git commit -m "docs: fix typo in getting_started.md"
```

### 6. 推送分支
```bash
git push origin fix-typo-docs
```

### 7. 创建Pull Request
- 访问你的GitHub仓库页面
- 点击 "Compare & pull request"
- 填写PR描述，引用相关Issue

## 实践任务
提交一个简单的文档修复PR。

**注意**: 对于首次贡献，建议从文档修复开始。
                    """,
                    duration_minutes=30,
                    commands=["git clone", "git checkout -b fix-typo-docs"],
                    validation_type="manual",
                    hints=[
                        "从小处着手，修复简单的文档问题",
                        "阅读CONTRIBUTING.md了解详细指南"
                    ]
                ),
                TutorialStep(
                    id="review",
                    title="回顾与总结",
                    description="回顾学习成果，规划下一步",
                    step_type=TutorialStepType.REVIEW,
                    content="""
# 回顾与总结

恭喜你完成了新贡献者入门教程！

## 学习成果
✅ 配置了AgentOS开发环境  
✅ 了解了项目结构和架构设计  
✅ 成功构建了AgentOS项目  
✅ 学习了贡献流程和PR提交  

## 下一步建议

### 1. 深入阅读文档
- [架构设计原则](../docs/ARCHITECTURAL_PRINCIPLES.md)
- [代码规范](../docs/Capital_Specifications/coding_standard/)
- [API文档](../docs/Capital_API/)

### 2. 参与社区讨论
- 加入GitHub Discussions
- 参与Issue讨论
- 审查他人的PR

### 3. 选择下一个教程
- **模块开发者**: 学习如何开发AgentOS模块
- **系统集成者**: 学习如何部署和集成AgentOS

## 持续学习
AgentOS是一个不断发展的项目，持续学习和参与是成为核心贡献者的关键。

感谢你的贡献！🎉
                    """,
                    duration_minutes=10,
                    commands=[],
                    validation_type="manual"
                )
            ]
        )
        
        self.tutorials["new-contributor"] = new_contributor_path
        
        # 保存教程文件
        self._save_tutorial(new_contributor_path)
        
        print("✅ 示例教程创建完成")
    
    def _save_tutorial(self, tutorial: TutorialPath):
        """保存教程到文件"""
        file_path = self.tutorials_dir / f"{tutorial.id}.json"
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(asdict(tutorial), f, indent=2, ensure_ascii=False, default=str)
    
    def start_tutorial(self, tutorial_id: str, user_id: str = "default") -> bool:
        """开始教程"""
        if tutorial_id not in self.tutorials:
            print(f"❌ 教程不存在: {tutorial_id}")
            return False
        
        self.progress = UserProgress(
            user_id=user_id,
            current_path=tutorial_id,
            current_step=0,
            start_time=datetime.now().isoformat(),
            last_update=datetime.now().isoformat()
        )
        
        # 保存进度
        self._save_progress()
        
        print(f"🎯 开始教程: {self.tutorials[tutorial_id].title}")
        self._show_current_step()
        
        return True
    
    def next_step(self) -> bool:
        """进入下一步"""
        if not self.progress:
            print("❌ 没有正在进行的教程")
            return False
        
        tutorial = self.tutorials[self.progress.current_path]
        
        # 标记当前步骤为完成
        if self.progress.current_step < len(tutorial.steps):
            current_step_id = tutorial.steps[self.progress.current_step].id
            self.progress.step_status[current_step_id] = StepStatus.COMPLETED
            self.progress.completed_steps.append(current_step_id)
        
        # 前进到下一步
        self.progress.current_step += 1
        
        if self.progress.current_step >= len(tutorial.steps):
            print("🎉 教程完成！")
            self._show_completion_summary()
            return False
        
        self.progress.last_update = datetime.now().isoformat()
        self._save_progress()
        self._show_current_step()
        
        return True
    
    def previous_step(self) -> bool:
        """返回上一步"""
        if not self.progress:
            print("❌ 没有正在进行的教程")
            return False
        
        if self.progress.current_step == 0:
            print("⚠️  已经是第一步")
            return False
        
        self.progress.current_step -= 1
        self.progress.last_update = datetime.now().isoformat()
        self._save_progress()
        self._show_current_step()
        
        return True
    
    def _show_current_step(self):
        """显示当前步骤"""
        if not self.progress:
            return
        
        tutorial = self.tutorials[self.progress.current_path]
        step = tutorial.steps[self.progress.current_step]
        
        print("\n" + "="*70)
        print(f"📚 步骤 {self.progress.current_step + 1}/{len(tutorial.steps)}: {step.title}")
        print("="*70)
        print(f"📖 {step.description}")
        print(f"⏱️  预计时间: {step.duration_minutes} 分钟")
        print(f"📝 类型: {step.step_type.value}")
        print("\n" + "-"*70)
        print(step.content)
        print("-"*70)
        
        if step.commands:
            print("\n💻 需要执行的命令:")
            for cmd in step.commands:
                print(f"  $ {cmd}")
        
        if step.hints:
            print("\n💡 提示:")
            for hint in step.hints:
                print(f"  • {hint}")
        
        if step.validation_type == "command":
            print("\n✅ 验证方式: 运行验证命令")
            print(f"  $ {step.validation_command}")
        elif step.validation_type == "file":
            print("\n✅ 验证方式: 检查文件是否存在")
            print(f"  文件模式: {step.validation_pattern}")
        elif step.validation_type == "manual":
            print("\n✅ 验证方式: 手动确认完成")
            print("  完成后输入 'done' 继续")
        
        print("\n" + "="*70)
        print("命令: next (下一步), prev (上一步), status (状态), quit (退出)")
        print("="*70)
    
    def _show_completion_summary(self):
        """显示完成总结"""
        if not self.progress:
            return
        
        tutorial = self.tutorials[self.progress.current_path]
        
        print("\n" + "="*70)
        print("🎉 教程完成总结")
        print("="*70)
        print(f"📚 教程: {tutorial.title}")
        print(f"👤 用户: {self.progress.user_id}")
        print(f"⏱️  开始时间: {self.progress.start_time}")
        print(f"📅 完成时间: {datetime.now().isoformat()}")
        print(f"✅ 完成步骤: {len(self.progress.completed_steps)}/{len(tutorial.steps)}")
        print("\n📈 学习成果:")
        
        for i, step in enumerate(tutorial.steps):
            status = self.progress.step_status.get(step.id, StepStatus.PENDING)
            status_icon = {
                StepStatus.COMPLETED: "✅",
                StepStatus.IN_PROGRESS: "🔄",
                StepStatus.PENDING: "⏳",
                StepStatus.SKIPPED: "⏭️"
            }.get(status, "❓")
            
            print(f"  {status_icon} {i+1}. {step.title}")
        
        print("\n🚀 下一步建议:")
        print("  1. 参与实际Issue的解决")
        print("  2. 审查其他贡献者的PR")
        print("  3. 选择更高级的教程继续学习")
        print("\n感谢你的学习与贡献！🌟")
        print("="*70)
    
    def validate_step(self, step_id: str, input_data: str = "") -> Tuple[bool, str]:
        """验证步骤完成情况"""
        if not self.progress:
            return False, "没有正在进行的教程"
        
        tutorial = self.tutorials[self.progress.current_path]
        
        # 查找当前步骤
        current_step = None
        for step in tutorial.steps:
            if step.id == step_id:
                current_step = step
                break
        
        if not current_step:
            return False, f"步骤不存在: {step_id}"
        
        if current_step.validation_type == "manual":
            if input_data.lower() == "done":
                return True, "步骤完成确认"
            else:
                return False, "请输入 'done' 确认完成"
        
        elif current_step.validation_type == "command":
            try:
                import subprocess
                result = subprocess.run(
                    current_step.validation_command,
                    shell=True,
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                
                if result.returncode == 0:
                    return True, "命令执行成功"
                else:
                    return False, f"命令执行失败: {result.stderr}"
            except Exception as e:
                return False, f"验证失败: {e}"
        
        elif current_step.validation_type == "file":
            import glob
            matches = list(Path(".").glob(current_step.validation_pattern))
            if matches:
                return True, f"找到文件: {len(matches)} 个匹配"
            else:
                return False, f"未找到匹配文件: {current_step.validation_pattern}"
        
        return False, "未知验证类型"
    
    def get_status(self) -> Dict[str, Any]:
        """获取当前状态"""
        if not self.progress:
            return {"status": "no_active_tutorial"}
        
        tutorial = self.tutorials[self.progress.current_path]
        current_step = tutorial.steps[self.progress.current_step]
        
        return {
            "status": "in_progress",
            "tutorial": self.progress.current_path,
            "current_step": self.progress.current_step + 1,
            "total_steps": len(tutorial.steps),
            "current_step_title": current_step.title,
            "completed_steps": len(self.progress.completed_steps),
            "start_time": self.progress.start_time,
            "last_update": self.progress.last_update
        }
    
    def _save_progress(self):
        """保存进度"""
        if not self.progress:
            return
        
        progress_file = self.tutorials_dir / f"progress_{self.progress.user_id}.json"
        with open(progress_file, 'w', encoding='utf-8') as f:
            json.dump(asdict(self.progress), f, indent=2, ensure_ascii=False, default=str)
    
    def load_progress(self, user_id: str = "default") -> bool:
        """加载进度"""
        progress_file = self.tutorials_dir / f"progress_{user_id}.json"
        
        if not progress_file.exists():
            return False
        
        try:
            with open(progress_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
                self.progress = UserProgress(**data)
            
            # 检查教程是否存在
            if self.progress.current_path not in self.tutorials:
                print(f"⚠️  教程不存在: {self.progress.current_path}")
                return False
            
            print(f"📚 恢复进度: {self.progress.current_path} (步骤 {self.progress.current_step + 1})")
            return True
        except Exception as e:
            print(f"❌ 加载进度失败: {e}")
            return False
    
    def list_tutorials(self):
        """列出所有教程"""
        print("\n" + "="*70)
        print("📚 可用教程")
        print("="*70)
        
        for tutorial_id, tutorial in self.tutorials.items():
            print(f"\n🎯 {tutorial.title}")
            print(f"   ID: {tutorial_id}")
            print(f"   描述: {tutorial.description}")
            print(f"   适用角色: {tutorial.role.value}")
            print(f"   预计时长: {tutorial.estimated_hours} 小时")
            print(f"   步骤数: {len(tutorial.steps)}")
        
        print("\n" + "="*70)
        print("使用方法: python tutorial_engine.py start --tutorial <教程ID>")
        print("="*70)


class TutorialWebServer:
    """教程Web服务器"""
    
    def __init__(self, engine: TutorialEngine, port: int = 8080):
        self.engine = engine
        self.port = port
    
    def start(self):
        """启动Web服务器"""
        print(f"🌐 启动教程Web服务器: http://localhost:{self.port}")
        
        # 创建临时目录用于Web文件
        web_dir = self.engine.tutorials_dir / "web"
        web_dir.mkdir(exist_ok=True)
        
        # 生成首页
        self._generate_index_html(web_dir)
        
        # 切换工作目录
        os.chdir(web_dir)
        
        # 启动服务器
        server = HTTPServer(('localhost', self.port), SimpleHTTPRequestHandler)
        
        print(f"📄 访问教程页面: http://localhost:{self.port}")
        print("🛑 按 Ctrl+C 停止服务器")
        
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\n🛑 服务器已停止")
    
    def _generate_index_html(self, web_dir: Path):
        """生成HTML首页"""
        html_content = """
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AgentOS 交互式教程</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            line-height: 1.6;
            color: #333;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 2rem;
        }
        
        header {
            text-align: center;
            margin-bottom: 3rem;
            color: white;
        }
        
        header h1 {
            font-size: 3rem;
            margin-bottom: 1rem;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        header p {
            font-size: 1.2rem;
            opacity: 0.9;
        }
        
        .card-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 2rem;
            margin-bottom: 3rem;
        }
        
        .card {
            background: white;
            border-radius: 15px;
            padding: 2rem;
            box-shadow: 0 20px 40px rgba(0,0,0,0.1);
            transition: transform 0.3s ease, box-shadow 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 25px 50px rgba(0,0,0,0.15);
        }
        
        .card h2 {
            color: #4a5568;
            margin-bottom: 1rem;
            font-size: 1.8rem;
        }
        
        .card p {
            color: #718096;
            margin-bottom: 1.5rem;
        }
        
        .meta {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1.5rem;
            padding-bottom: 1rem;
            border-bottom: 1px solid #e2e8f0;
        }
        
        .badge {
            background: #4299e1;
            color: white;
            padding: 0.25rem 0.75rem;
            border-radius: 20px;
            font-size: 0.875rem;
            font-weight: 600;
        }
        
        .time {
            color: #718096;
            font-size: 0.875rem;
        }
        
        .btn {
            display: inline-block;
            background: #667eea;
            color: white;
            padding: 0.75rem 1.5rem;
            border-radius: 8px;
            text-decoration: none;
            font-weight: 600;
            transition: background 0.3s ease;
            border: none;
            cursor: pointer;
            font-size: 1rem;
        }
        
        .btn:hover {
            background: #5a67d8;
        }
        
        .btn-secondary {
            background: #48bb78;
        }
        
        .btn-secondary:hover {
            background: #38a169;
        }
        
        .steps {
            margin-top: 1rem;
        }
        
        .step-item {
            display: flex;
            align-items: center;
            padding: 0.75rem;
            border-radius: 8px;
            background: #f7fafc;
            margin-bottom: 0.5rem;
        }
        
        .step-number {
            background: #cbd5e0;
            color: #4a5568;
            width: 32px;
            height: 32px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-right: 1rem;
            font-weight: 600;
        }
        
        footer {
            text-align: center;
            color: white;
            margin-top: 3rem;
            padding-top: 2rem;
            border-top: 1px solid rgba(255,255,255,0.2);
        }
        
        .command-line {
            background: #2d3748;
            color: #e2e8f0;
            padding: 1rem;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            margin: 2rem 0;
        }
        
        .cli-instruction {
            text-align: center;
            margin-top: 2rem;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>AgentOS 交互式教程</h1>
            <p>通过渐进式学习路径，快速掌握 AgentOS 开发与贡献</p>
        </header>
        
        <div class="card-grid">
            <div class="card">
                <h2>👋 新贡献者入门</h2>
                <div class="meta">
                    <span class="badge">初学者</span>
                    <span class="time">⏱️ 4 小时</span>
                </div>
                <p>面向首次接触 AgentOS 的贡献者，涵盖环境配置、代码结构和第一个 PR 提交。</p>
                <div class="steps">
                    <div class="step-item">
                        <span class="step-number">1</span>
                        <span>欢迎与理论介绍</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">2</span>
                        <span>环境配置与验证</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">3</span>
                        <span>项目结构分析</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">4</span>
                        <span>首次构建项目</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">5</span>
                        <span>提交第一个 PR</span>
                    </div>
                </div>
                <button class="btn" onclick="startTutorial('new-contributor')">开始学习</button>
            </div>
            
            <div class="card">
                <h2>🛠️ 模块开发者指南</h2>
                <div class="meta">
                    <span class="badge">中级</span>
                    <span class="time">⏱️ 8 小时</span>
                </div>
                <p>学习如何开发 AgentOS 模块，理解模块架构、API 设计和测试编写。</p>
                <div class="steps">
                    <div class="step-item">
                        <span class="step-number">1</span>
                        <span>模块系统架构</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">2</span>
                        <span>API 设计与契约</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">3</span>
                        <span>模块实现模式</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">4</span>
                        <span>测试框架使用</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">5</span>
                        <span>性能优化技巧</span>
                    </div>
                </div>
                <button class="btn" onclick="alert('即将推出！')">即将推出</button>
            </div>
            
            <div class="card">
                <h2>🔧 系统集成者指南</h2>
                <div class="meta">
                    <span class="badge">高级</span>
                    <span class="time">⏱️ 12 小时</span>
                </div>
                <p>学习如何部署和集成 AgentOS，掌握系统配置、监控调试和故障排查。</p>
                <div class="steps">
                    <div class="step-item">
                        <span class="step-number">1</span>
                        <span>系统架构概述</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">2</span>
                        <span>部署配置实践</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">3</span>
                        <span>监控与告警</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">4</span>
                        <span>故障排查技巧</span>
                    </div>
                    <div class="step-item">
                        <span class="step-number">5</span>
                        <span>性能调优实战</span>
                    </div>
                </div>
                <button class="btn" onclick="alert('即将推出！')">即将推出</button>
            </div>
        </div>
        
        <div class="command-line">
            <h3 style="color: #68d391; margin-bottom: 1rem;">💻 命令行版本</h3>
            <p>如果你更喜欢命令行界面，可以使用以下命令启动交互式教程：</p>
            <pre style="background: #1a202c; padding: 1rem; border-radius: 4px; margin: 1rem 0;">
cd scripts/tutorial
python tutorial_engine.py start --tutorial new-contributor</pre>
            <p>支持的命令：next (下一步), prev (上一步), status (状态), validate (验证), quit (退出)</p>
        </div>
        
        <div class="cli-instruction">
            <h3>🎯 立即开始学习</h3>
            <p>打开终端，运行以下命令开始你的 AgentOS 学习之旅：</p>
            <pre style="display: inline-block; background: rgba(0,0,0,0.2); color: white; padding: 0.5rem 1rem; border-radius: 4px; margin-top: 1rem;">
python tutorial_engine.py list</pre>
        </div>
    </div>
    
    <footer>
        <p>© 2026 SPHARX Ltd. | AgentOS 交互式教程系统 v1.0</p>
        <p style="margin-top: 0.5rem; font-size: 0.875rem; opacity: 0.8;">"From data intelligence emerges."</p>
    </footer>
    
    <script>
        function startTutorial(tutorialId) {
            if (tutorialId === 'new-contributor') {
                alert('请在命令行中运行：\\n\\ncd scripts/tutorial\\npython tutorial_engine.py start --tutorial new-contributor');
            } else {
                alert('该教程即将推出，敬请期待！');
            }
        }
    </script>
</body>
</html>
        """
        
        index_file = web_dir / "index.html"
        index_file.write_text(html_content, encoding='utf-8')


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="AgentOS 交互式教程引擎",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 列出所有教程
  python tutorial_engine.py list
  
  # 开始新贡献者教程
  python tutorial_engine.py start --tutorial new-contributor
  
  # 继续当前教程
  python tutorial_engine.py next
  python tutorial_engine.py prev
  
  # 查看当前状态
  python tutorial_engine.py status
  
  # 验证当前步骤
  python tutorial_engine.py validate --input "done"
  
  # 启动Web服务器
  python tutorial_engine.py serve --port 8080
        """
    )
    
    subparsers = parser.add_subparsers(dest="command", help="命令")
    
    # list 命令
    list_parser = subparsers.add_parser("list", help="列出所有教程")
    
    # start 命令
    start_parser = subparsers.add_parser("start", help="开始教程")
    start_parser.add_argument("--tutorial", "-t", required=True, help="教程ID")
    start_parser.add_argument("--user", "-u", default="default", help="用户ID")
    
    # next 命令
    subparsers.add_parser("next", help="下一步")
    
    # prev 命令
    subparsers.add_parser("prev", help="上一步")
    
    # status 命令
    subparsers.add_parser("status", help="查看当前状态")
    
    # validate 命令
    validate_parser = subparsers.add_parser("validate", help="验证当前步骤")
    validate_parser.add_argument("--input", "-i", default="", help="验证输入")
    
    # serve 命令
    serve_parser = subparsers.add_parser("serve", help="启动Web服务器")
    serve_parser.add_argument("--port", "-p", type=int, default=8080, help="端口号")
    
    args = parser.parse_args()
    
    # 初始化教程引擎
    tutorials_dir = Path(__file__).parent
    engine = TutorialEngine(tutorials_dir)
    
    if args.command == "list":
        engine.list_tutorials()
    
    elif args.command == "start":
        # 尝试加载现有进度
        if not engine.load_progress(args.user):
            # 开始新教程
            engine.start_tutorial(args.tutorial, args.user)
    
    elif args.command == "next":
        if not engine.load_progress():
            print("❌ 没有正在进行的教程，请先使用 'start' 命令")
            return
        engine.next_step()
    
    elif args.command == "prev":
        if not engine.load_progress():
            print("❌ 没有正在进行的教程，请先使用 'start' 命令")
            return
        engine.previous_step()
    
    elif args.command == "status":
        if not engine.load_progress():
            print("❌ 没有正在进行的教程")
            return
        
        status = engine.get_status()
        print("\n📊 当前状态:")
        print(f"   教程: {status['tutorial']}")
        print(f"   步骤: {status['current_step']}/{status['total_steps']}")
        print(f"   当前步骤: {status['current_step_title']}")
        print(f"   完成步骤: {status['completed_steps']}")
        print(f"   开始时间: {status['start_time']}")
        print(f"   最后更新: {status['last_update']}")
    
    elif args.command == "validate":
        if not engine.load_progress():
            print("❌ 没有正在进行的教程，请先使用 'start' 命令")
            return
        
        status = engine.get_status()
        tutorial = engine.tutorials[status['tutorial']]
        current_step_id = tutorial.steps[status['current_step'] - 1].id
        
        success, message = engine.validate_step(current_step_id, args.input)
        if success:
            print(f"✅ {message}")
        else:
            print(f"❌ {message}")
    
    elif args.command == "serve":
        # 启动Web服务器
        server = TutorialWebServer(engine, args.port)
        server.start()
    
    else:
        parser.print_help()


if __name__ == "__main__":
    main()