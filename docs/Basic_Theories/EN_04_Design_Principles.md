Copyright (c) 2026 SPHARX. All Rights Reserved.  
"From data intelligence emerges."

# AgentOS Design Principles

**Version**: Doc V1.7  
**Last Updated**: 2026-03-31  
**Author**: LirenWang

## 1. Introduction

AgentOS is an operating system for multi-agent collaboration, designed to evolve agents from single executors into dynamically formable, self-evolving teams. Its design is rooted in the Five-Dimensional Orthogonal System, encompassing systems perspective, kernel perspective, cognitive perspective, engineering perspective, and design aesthetics. Theoretical foundations include the core ideas of the "Two Engineering Theories," Daniel Kahneman's dual-system cognitive theory, microkernel philosophy, and Jobsian design aesthetics. This document systematically elaborates AgentOS's core design principles, providing a unified theoretical foundation for the entire project.

## 2. Two Engineering Theories: Unity of Control and Systems

### 2.1 Feedback Loops: The Core of Cybernetics

*Engineering Cybernetics* states that any control system relies on feedback to correct deviations. AgentOS integrates feedback throughout every layer:

- **Real-time feedback**: Action layer execution results return to the cognitive layer in real time, revising current task planning.
- **Intra-round feedback**: The incremental planner adjusts subsequent phase task graphs based on execution feedback.
- **Cross-round feedback**: The evolution committee updates role rules and technical specifications based on historical data, enabling system self-optimization.

This mechanism ensures AgentOS can both respond rapidly to changes and learn from historical experience.

### 2.2 Hierarchical Decomposition: The Systems Engineering Approach

*On Systems Engineering* emphasizes that complex systems should be decomposed hierarchically. AgentOS adopts a three-layer integrated structure:

- **Cognitive layer**: Responsible for intent understanding, task planning, and scheduling decisions.
- **Action layer**: Responsible for concrete execution, consisting of specialized agent pools and pluggable execution units.
- **Memory and evolution layer**: Responsible for memory storage, pattern mining, and rule updates.

Each layer is further subdivided internally; for example, the memory layer is divided into L1 Raw Volume, L2 Feature Layer, L3 Structural Layer, and L4 Pattern Layer, forming clear abstraction hierarchies that reduce system complexity.

### 2.3 Overall Design Department: Coordinating the Whole

*On Systems Engineering* proposes the "overall design department" concept, responsible for overall coordination and trade-offs. In AgentOS, the **cognitive layer** plays this role: it does not execute specific tasks but is responsible for intent decomposition, strategy selection, and agent scheduling, ensuring optimal global objectives.

## 3. Dual-System Theory: Collaboration of Fast and Slow

Daniel Kahneman proposes in *Thinking, Fast and Slow* that human thinking comprises two systems:

- **System 1**: Fast, intuitive, automatic, used for daily decisions.
- **System 2**: Slow, analytical, deliberative, used for complex reasoning.

AgentOS embeds this theory into its architecture:

- **Cognitive layer**: System 2 deep planning, System 1 rapid classification and cross-validation.
- **Action layer**: System 1 fast execution, System 2 result verification and exception handling.
- **Memory layer**: System 1 rapid retrieval, System 2 deep pattern mining.

The collaboration of dual systems enables the system to efficiently handle routine tasks while confronting complex challenges.

## 4. Microkernel and Pure Kernel

AgentOS follows microkernel design philosophy:

- **Minimalist kernel**: The `agentos/atoms/` directory contains only the most fundamental foundations (IPC, memory management, task scheduling, time services) along with the two major subsystems `coreloopthree` and `memoryrovol`.
- **Externalized services**: All user-mode services (e.g., LLM, marketplace, monitoring) run as independent daemon processes (`agentos/daemon/`), interacting with the kernel through system calls.
- **Security isolation**: The `agentos/cupolas/` module provides security mechanisms like virtual workstations, permission arbitration, and input sanitization, decoupled from the kernel.

This design ensures kernel stability and portability while allowing services to evolve independently.

## 5. Modularity and Pluggability

Every component of AgentOS is designed as a pluggable strategy:

- **Cognitive strategies**: Planning (hierarchical/reactive/reflective/ML), collaboration (dual-model/majority voting/weighted), scheduling (weighted/round-robin/priority/ML) can all be dynamically replaced.
- **Execution units**: Tools, code, APIs, files, and other units are dynamically loaded through a registry, enabling community contributions of new units.
- **Memory strategies**: Forgetting curve types, clustering algorithms, rule generators are all configurable.

This design allows system optimization for different scenarios and encourages community contributions.

## 6. Security by Design

Security is no longer an add-on feature but embedded in system design:

- **Virtual workstations**: Each agent runs in an independent sandbox (process or container) with limited resources and optional network access.
- **Permission arbitration**: Based on YAML rules supporting wildcards and caching, dynamically reloadable.
- **Input sanitization**: Rule-based filtering using regular expressions, capable of replacing or deleting malicious content.
- **Audit logs**: Asynchronous writing, supporting rotation and querying, recording all sensitive operations.

Security mechanisms permeate the entire system, covering from kernel to service layers.

## 7. Observability

AgentOS has built-in comprehensive observability support:

- **Structured logging**: Unified log format supporting TraceID propagation for distributed tracing.
- **Metrics collection**: Counters, gauges, histograms supporting Prometheus export.
- **Distributed tracing**: Tracing data based on OpenTelemetry, recording full request chains.
- **Health checks**: Each component provides health status JSON interfaces for easy monitoring system integration.

## 8. Summary

AgentOS's design principles integrate the systems thinking of the Two Engineering Theories, the cognitive model of dual-system theory, and the microkernel philosophy of modern operating systems. It pursues **pure kernel, independent services, security by design, and comprehensive observability**, providing a stable, efficient, evolvable operational foundation for agent teams.

---

**References**

1. *Engineering Cybernetics*. 1954.
2. *On Systems Engineering*. 1981.
3. Kahneman, D. (2011). *Thinking, Fast and Slow*. Farrar, Straus and Giroux.
4. Liedtke, J. (1995). On μ-Kernel Construction. *Proc. 15th ACM Symposium on Operating Systems Principles*.

**Document Author**
- DechengLi [https://gitee.com/spharx01]