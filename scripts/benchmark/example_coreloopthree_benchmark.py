#!/usr/bin/env python3
"""
AgentOS CoreLoopThree 性能基准测试示例

本示例展示如何使用性能基准测试框架测试 CoreLoopThree 模块。
由于 CoreLoopThree 是 C 语言实现的系统核心模块，本示例使用模拟接口
演示完整的基准测试流程，包括测试定义、执行、监控和结果分析。

设计原则：
1. 真实性 - 模拟真实 CoreLoopThree 工作负载
2. 可重复性 - 确保每次测试条件一致
3. 可扩展性 - 可轻松扩展到实际 C 模块测试

@version v1.0.0
@date 2026-04-11
@copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import asyncio
import sys
import time
from pathlib import Path

# 添加父目录到路径，以便导入 benchmark 模块
sys.path.insert(0, str(Path(__file__).parent))

from benchmark_core import (
    BenchmarkBase, BenchmarkContext, BenchmarkPhase,
    BenchmarkRegistry, BenchmarkRunner, ThroughputBenchmark,
    ResourceMonitor, BenchmarkResult
)
from statistics_engine import StatisticsEngine
from report_generator import ReportGenerator, ReportFormat


class CoreLoopThreeCognitionBenchmark(BenchmarkBase):
    """CoreLoopThree 认知层性能基准测试
    
    模拟认知层的核心操作：
    1. 意图理解（Intent Understanding）
    2. 任务规划（Task Planning）
    3. 模型协同（Model Coordination）
    4. Agent调度（Agent Dispatching）
    """
    
    async def setup(self, context: BenchmarkContext) -> None:
        """测试初始化"""
        context.logger.info("初始化 CoreLoopThree 认知层基准测试环境")
        
        # 模拟认知层初始化
        self.cognition_engine = {
            "initialized": True,
            "models_loaded": 3,  # 3个模型：理解、规划、调度
            "memory_size": 1024 * 1024,  # 1MB 内存缓存
            "config": {
                "max_concurrent_tasks": 10,
                "planning_timeout_ms": 1000,
                "coordination_mode": "adaptive"
            }
        }
        
        # 准备测试数据
        self.test_intents = [
            "分析用户请求并制定执行计划",
            "理解自然语言指令并转换为任务",
            "协调多个模型共同解决复杂问题",
            "调度智能体执行分布式任务",
            "评估任务执行结果并调整策略"
        ] * 20  # 100个测试意图
        
        context.metadata.update({
            "module": "coreloopthree/cognition",
            "version": "1.0.0.7",
            "test_type": "throughput",
            "workload": "mixed_intent_processing"
        })
    
    async def run(self, context: BenchmarkContext) -> None:
        """执行基准测试"""
        context.logger.info(f"开始 CoreLoopThree 认知层基准测试，共 {len(self.test_intents)} 个意图")
        
        processed_count = 0
        start_time = time.perf_counter()
        
        for i, intent in enumerate(self.test_intents):
            # 模拟意图理解过程
            await asyncio.sleep(0.001)  # 模拟 1ms 处理时间
            
            # 模拟任务规划
            plan = self._simulate_planning(intent)
            
            # 模拟模型协同
            coordination_result = self._simulate_coordination(plan)
            
            # 模拟 Agent 调度
            dispatch_result = self._simulate_dispatch(coordination_result)
            
            processed_count += 1
            
            # 每处理10个意图报告一次进度
            if (i + 1) % 10 == 0:
                elapsed = time.perf_counter() - start_time
                throughput = (i + 1) / elapsed
                context.logger.info(
                    f"进度: {i + 1}/{len(self.test_intents)}, "
                    f"吞吐量: {throughput:.2f} 意图/秒"
                )
        
        end_time = time.perf_counter()
        total_time = end_time - start_time
        throughput = len(self.test_intents) / total_time
        
        # 记录结果
        context.results.append(BenchmarkResult(
            test_name="cognition_throughput",
            metric_name="intents_per_second",
            value=throughput,
            unit="intents/sec",
            metadata={
                "total_intents": len(self.test_intents),
                "total_time_sec": total_time,
                "avg_latency_ms": (total_time / len(self.test_intents)) * 1000
            }
        ))
        
        context.logger.info(
            f"CoreLoopThree 认知层基准测试完成: "
            f"吞吐量 = {throughput:.2f} 意图/秒, "
            f"平均延迟 = {(total_time / len(self.test_intents)) * 1000:.2f} ms"
        )
    
    async def cleanup(self, context: BenchmarkContext) -> None:
        """清理测试环境"""
        context.logger.info("清理 CoreLoopThree 认知层基准测试环境")
        self.cognition_engine = None
        self.test_intents = None
    
    def _simulate_planning(self, intent: str) -> dict:
        """模拟任务规划过程"""
        # 模拟不同复杂度意图的处理时间差异
        complexity = min(len(intent) / 100, 1.0)  # 基于意图长度估算复杂度
        plan_size = int(10 + complexity * 20)  # 10-30个步骤
        
        return {
            "intent": intent,
            "steps": plan_size,
            "complexity": complexity,
            "resources": ["cpu", "memory", "io"],
            "estimated_duration": 100 + complexity * 900  # 100-1000ms
        }
    
    def _simulate_coordination(self, plan: dict) -> dict:
        """模拟模型协同过程"""
        coordination_score = 0.7 + (plan["complexity"] * 0.3)  # 0.7-1.0
        models_involved = max(1, int(plan["complexity"] * 3))  # 1-3个模型
        
        return {
            "plan": plan,
            "coordination_score": coordination_score,
            "models_involved": models_involved,
            "consensus_achieved": coordination_score > 0.8,
            "communication_overhead": plan["steps"] * 0.1
        }
    
    def _simulate_dispatch(self, coordination_result: dict) -> dict:
        """模拟 Agent 调度过程"""
        success_rate = 0.85 + (coordination_result["coordination_score"] * 0.15)
        agents_required = max(1, int(coordination_result["plan"]["steps"] / 5))
        
        return {
            "coordination": coordination_result,
            "success_rate": success_rate,
            "agents_required": agents_required,
            "estimated_completion_time": coordination_result["plan"]["estimated_duration"] * 1.2,
            "load_balanced": agents_required > 1
        }


class CoreLoopThreeMemoryBenchmark(BenchmarkBase):
    """CoreLoopThree 记忆层性能基准测试
    
    模拟记忆层的核心操作：
    1. 记忆写入（Memory Write）
    2. 记忆查询（Memory Query）
    3. 记忆融合（Memory Fusion）
    4. 记忆检索（Memory Retrieval）
    """
    
    async def setup(self, context: BenchmarkContext) -> None:
        """测试初始化"""
        context.logger.info("初始化 CoreLoopThree 记忆层基准测试环境")
        
        # 模拟记忆层初始化
        self.memory_engine = {
            "initialized": True,
            "storage_backend": "memoryrovol",
            "cache_size": 512 * 1024,  # 512KB 缓存
            "index_type": "hierarchical",
            "retrieval_strategy": "semantic"
        }
        
        # 准备测试记忆数据
        self.memory_entries = []
        for i in range(100):  # 100个记忆条目
            self.memory_entries.append({
                "id": f"memory_{i:03d}",
                "content": f"这是第{i}个记忆条目，包含重要的智能体经验数据",
                "timestamp": time.time() - (i * 3600),  # 时间分布
                "importance": 0.5 + (i % 10) * 0.05,  # 0.5-1.0
                "tags": [f"tag_{j}" for j in range(i % 5)],
                "embeddings": [float(j) for j in range(128)]  # 128维向量
            })
        
        # 准备查询
        self.queries = [
            "查找关于任务规划的经验",
            "搜索用户偏好数据",
            "检索错误处理模式",
            "查找性能优化建议",
            "搜索协作策略"
        ] * 10  # 50个查询
        
        context.metadata.update({
            "module": "coreloopthree/memory",
            "version": "1.0.0.7",
            "test_type": "latency",
            "workload": "mixed_memory_operations"
        })
    
    async def run(self, context: BenchmarkContext) -> None:
        """执行基准测试"""
        context.logger.info(
            f"开始 CoreLoopThree 记忆层基准测试: "
            f"{len(self.memory_entries)} 个记忆条目, "
            f"{len(self.queries)} 个查询"
        )
        
        # 测试记忆写入性能
        write_times = []
        for entry in self.memory_entries:
            start = time.perf_counter()
            await self._simulate_write(entry)
            write_times.append(time.perf_counter() - start)
        
        # 测试记忆查询性能
        query_times = []
        for query in self.queries:
            start = time.perf_counter()
            results = await self._simulate_query(query)
            query_times.append(time.perf_counter() - start)
        
        # 测试记忆融合性能
        fusion_times = []
        for i in range(0, len(self.memory_entries), 5):
            if i + 5 <= len(self.memory_entries):
                batch = self.memory_entries[i:i+5]
                start = time.perf_counter()
                await self._simulate_fusion(batch)
                fusion_times.append(time.perf_counter() - start)
        
        # 记录结果
        if write_times:
            avg_write_time = sum(write_times) / len(write_times)
            context.results.append(BenchmarkResult(
                test_name="memory_write_latency",
                metric_name="write_latency",
                value=avg_write_time * 1000,  # 转换为毫秒
                unit="ms",
                metadata={
                    "operations": len(write_times),
                    "total_time_sec": sum(write_times),
                    "throughput": len(write_times) / sum(write_times)
                }
            ))
        
        if query_times:
            avg_query_time = sum(query_times) / len(query_times)
            context.results.append(BenchmarkResult(
                test_name="memory_query_latency",
                metric_name="query_latency",
                value=avg_query_time * 1000,  # 转换为毫秒
                unit="ms",
                metadata={
                    "operations": len(query_times),
                    "total_time_sec": sum(query_times),
                    "throughput": len(query_times) / sum(query_times)
                }
            ))
        
        if fusion_times:
            avg_fusion_time = sum(fusion_times) / len(fusion_times)
            context.results.append(BenchmarkResult(
                test_name="memory_fusion_latency",
                metric_name="fusion_latency",
                value=avg_fusion_time * 1000,  # 转换为毫秒
                unit="ms",
                metadata={
                    "operations": len(fusion_times),
                    "batch_size": 5,
                    "total_time_sec": sum(fusion_times)
                }
            ))
        
        context.logger.info(
            f"CoreLoopThree 记忆层基准测试完成: "
            f"写入延迟 = {avg_write_time*1000:.2f} ms, "
            f"查询延迟 = {avg_query_time*1000:.2f} ms, "
            f"融合延迟 = {avg_fusion_time*1000:.2f} ms"
        )
    
    async def cleanup(self, context: BenchmarkContext) -> None:
        """清理测试环境"""
        context.logger.info("清理 CoreLoopThree 记忆层基准测试环境")
        self.memory_engine = None
        self.memory_entries = None
        self.queries = None
    
    async def _simulate_write(self, entry: dict) -> bool:
        """模拟记忆写入"""
        await asyncio.sleep(0.002)  # 模拟 2ms 写入时间
        return True
    
    async def _simulate_query(self, query: str) -> list:
        """模拟记忆查询"""
        await asyncio.sleep(0.005)  # 模拟 5ms 查询时间
        
        # 模拟语义搜索返回结果
        results = []
        for entry in self.memory_entries:
            similarity = 0.3 + (hash(query) % 100) / 200  # 模拟相似度 0.3-0.8
            if similarity > 0.5:
                results.append({
                    "entry": entry,
                    "similarity": similarity,
                    "relevance": similarity * entry["importance"]
                })
        
        # 返回 top-3 结果
        results.sort(key=lambda x: x["relevance"], reverse=True)
        return results[:3]
    
    async def _simulate_fusion(self, batch: list) -> dict:
        """模拟记忆融合"""
        await asyncio.sleep(0.010)  # 模拟 10ms 融合时间
        
        # 模拟融合过程
        fused_memory = {
            "id": f"fused_{int(time.time())}",
            "content": "融合后的记忆表示",
            "timestamp": time.time(),
            "source_entries": [e["id"] for e in batch],
            "importance": sum(e["importance"] for e in batch) / len(batch),
            "complexity": len(batch) * 0.2,
            "insights": [f"从条目{e['id']}中提取的洞察" for e in batch]
        }
        
        return fused_memory


async def run_coreloopthree_benchmarks():
    """运行 CoreLoopThree 基准测试套件"""
    print("=" * 70)
    print("AgentOS CoreLoopThree 性能基准测试套件")
    print("=" * 70)
    
    # 创建基准测试运行器
    runner = BenchmarkRunner()
    
    # 注册基准测试
    registry = BenchmarkRegistry()
    registry.register("cognition", CoreLoopThreeCognitionBenchmark())
    registry.register("memory", CoreLoopThreeMemoryBenchmark())
    
    # 配置测试参数
    config = {
        "iterations": 3,
        "warmup_iterations": 1,
        "timeout_seconds": 300,
        "resource_monitoring": True
    }
    
    # 运行所有基准测试
    print("\n🚀 开始执行 CoreLoopThree 基准测试...")
    results = await runner.run_all(registry, config)
    
    # 统计分析和报告生成
    print("\n📊 基准测试结果分析...")
    stats_engine = StatisticsEngine()
    for result in results:
        stats = stats_engine.compute_descriptive_statistics(result)
        print(f"\n测试 '{result.test_name}' 统计摘要:")
        print(f"  平均值: {stats.mean:.4f} {result.unit}")
        print(f"  标准差: {stats.std_dev:.4f}")
        print(f"  最小值: {stats.min:.4f}")
        print(f"  最大值: {stats.max:.4f}")
        print(f"  样本数: {stats.sample_size}")
    
    # 生成报告
    print("\n📄 生成基准测试报告...")
    report_gen = ReportGenerator()
    report_path = report_gen.generate_report(
        results=results,
        format=ReportFormat.HTML,
        filename="coreloopthree_benchmark_report.html"
    )
    
    print(f"\n✅ 基准测试完成!")
    print(f"📁 报告已生成: {report_path}")
    print(f"📈 共执行 {len(results)} 个基准测试")
    print(f"⏱️  总执行时间: {sum(r.metadata.get('total_time_sec', 0) for r in results):.2f} 秒")
    
    return results


if __name__ == "__main__":
    # 运行基准测试
    asyncio.run(run_coreloopthree_benchmarks())