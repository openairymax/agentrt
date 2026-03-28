﻿#!/usr/bin/env python3
"""
AgentOS 配置模块常量定义

包含配置验证和部署所需的公共常量
"""

# Schema 映射关系
SCHEMA_MAPPING = {
    'kernel/settings.yaml': 'kernel-settings.schema.json',
    'model/model.yaml': 'model.schema.json',
    'agent/registry.yaml': 'agent-registry.schema.json',
    'skill/registry.yaml': 'skill-registry.schema.json',
    'security/policy.yaml': 'security-policy.schema.json',
    'sanitizer/sanitizer_rules.json': 'sanitizer-rules.schema.json',
    'logging/manager.yaml': 'logging.schema.json',
    'config_management.yaml': 'manager-management.schema.json',
    'service/tool_d/tool.yaml': 'tool-service.schema.json',
}

# 必需的配置文件列表
REQUIRED_CONFIG_FILES = [
    'kernel/settings.yaml',
    'model/model.yaml',
    'agent/registry.yaml',
    'skill/registry.yaml',
    'security/policy.yaml',
    'security/permission_rules.yaml',
    'sanitizer/sanitizer_rules.json',
    'logging/manager.yaml',
    'config_management.yaml',
]

# 环境配置文件列表
ENV_CONFIG_FILES = [
    'env/development.yaml',
    'env/staging.yaml',
    'env/production.yaml',
]

# 版本检查配置文件列表
VERSION_CHECK_FILES = [
    'kernel/settings.yaml',
    'model/model.yaml',
    'agent/registry.yaml',
    'skill/registry.yaml',
    'security/policy.yaml',
    'logging/manager.yaml',
    'config_management.yaml',
]

# 环境变量检查配置文件列表
ENV_VAR_CHECK_FILES = [
    'kernel/settings.yaml',
    'model/model.yaml',
    'security/policy.yaml',
    'logging/manager.yaml',
    'config_management.yaml',
]
