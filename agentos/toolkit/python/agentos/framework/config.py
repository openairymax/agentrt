# AgentOS Configuration Center
# Version: 2.0.0
# Last updated: 2026-04-11

"""
集中式配置管理中心

提供统一的配置加载、验证、更新和监控能力。
支持多种数据源（文件、环境变量、字典、远程），并支持动态热更新。

设计原则:
1. 分层覆盖 - 支持多级配置优先级（默认 < 文件 < 环境变量 < 远程）
2. 类型安全 - 配置值的类型检查和转换
3. 变更感知 - 配置变更的实时通知和回调
4. 验证机制 - 完整性校验和约束检查
"""

import asyncio
import json
import logging
import os
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Type, TypeVar, Union

logger = logging.getLogger(__name__)

T = TypeVar('T')


@dataclass
class ConfigSource:
    """配置源定义"""
    source_type: str  # file, env, dict, remote
    source_location: str = ""  # 文件路径或URL
    priority: int = 0  # 优先级，数值越高越优先
    format: str = "auto"  # yaml, json, toml, ini, env
    optional: bool = True  # 是否可选（不存在是否报错）
    reload_interval: Optional[float] = None  # 自动重载间隔（秒）


@dataclass
class ValidationResult:
    """配置验证结果"""
    is_valid: bool = True
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    checked_keys: List[str] = field(default_factory=list)

    def add_error(self, message: str) -> None:
        self.is_valid = False
        self.errors.append(message)

    def add_warning(self, message: str) -> None:
        self.warnings.append(message)

    def merge(self, other: 'ValidationResult') -> None:
        """合并另一个验证结果"""
        if not other.is_valid:
            self.is_valid = False
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)
        self.checked_keys.extend(other.checked_keys)


@dataclass
class ConfigChangeRecord:
    """配置变更记录"""
    timestamp: datetime = field(default_factory=datetime.now)
    key: str = ""
    old_value: Any = None
    new_value: Any = None
    source: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "timestamp": self.timestamp.isoformat(),
            "key": self.key,
            "old_value": self.old_value,
            "new_value": self.new_value,
            "source": self.source
        }


class ConfigurationCenter:
    """
    集中式配置管理器

    提供完整的配置管理能力：
    - 多源加载和合并
    - 类型安全的访问接口
    - 动态更新和热重载
    - 变更监听和回调
    - 配置验证和完整性检查

    使用示例:
        config_center = ConfigurationCenter()

        # 加载多个配置源
        await config_center.load_from_file("config.yaml")
        await config_center.load_from_env(prefix="AGENTOS_")

        # 获取配置值
        debug_mode = config_center.get("debug", default=False)

        # 监听配置变更
        config_center.watch("log_level", lambda new_val: set_log_level(new_val))

        # 动态更新
        await config_center.set("max_connections", 100)
    """

    def __init__(self):
        """初始化配置中心"""
        self._config: Dict[str, Any] = {}
        self._defaults: Dict[str, Any] = {}
        self._validators: Dict[str, List[Callable[[Any], bool]]] = {}
        self._watchers: Dict[str, List[Callable[[Any, Any], None]]] = {}
        self._global_watchers: List[Callable[[str, Any, Any], None]] = []
        self._sources: List[ConfigSource] = []
        self._change_history: List[ConfigChangeRecord] = []
        self._type_hints: Dict[str, Type] = {}

        logger.info("ConfigurationCenter initialized")

    async def load_config(self, source: ConfigSource) -> int:
        """
        从指定源加载配置

        Args:
            source: 配置源定义

        Returns:
            加载的配置项数量
        """
        try:
            if source.source_type == "file":
                loaded = await self._load_from_file(source)
            elif source.source_type == "env":
                loaded = await self._load_from_env(source)
            elif source.source_type == "dict":
                loaded = await self._load_from_dict(source)
            elif source.source_type == "remote":
                loaded = await self._load_from_remote(source)
            else:
                raise ValueError(f"Unknown source type: {source.source_type}")

            self._sources.append(source)
            logger.info(
                f"Loaded {loaded} config entries from "
                f"{source.source_type}:{source.source_location}"
            )

            return loaded

        except Exception as e:
            if not source.optional:
                logger.error(f"Failed to load required config source: {e}")
                raise
            else:
                logger.warning(f"Optional config source failed (ignored): {e}")
                return 0

    async def load_from_file(self, file_path: str, format: str = "auto") -> int:
        """
        从文件加载配置

        Args:
            file_path: 文件路径
            format: 格式 (yaml, json, toml, ini, auto)

        Returns:
            加载的配置项数量
        """
        source = ConfigSource(
            source_type="file",
            source_location=file_path,
            format=format
        )
        return await self.load_config(source)

    async def load_from_env(self, prefix: str = "") -> int:
        """
        从环境变量加载配置

        Args:
            prefix: 环境变量前缀过滤

        Returns:
            加载的配置项数量
        """
        source = ConfigSource(
            source_type="env",
            source_location=prefix
        )
        return await self.load_config(source)

    async def load_from_dict(self, config_dict: Dict[str, Any]) -> int:
        """
        从字典加载配置

        Args:
            config_dict: 配置字典

        Returns:
            加载的配置项数量
        """
        import json
        source = ConfigSource(
            source_type="dict",
            source_location=json.dumps(config_dict)
        )
        return await self.load_config(source)

    def get(self, key: str, default: T = None) -> Union[T, Any]:
        """
        获取配置值（支持嵌套键，如 "database.host"）

        Args:
            key: 配置键（支持点分隔的嵌套键）
            default: 默认值

        Returns:
            配置值
        """
        value = self._get_nested(key)
        if value is None:
            return default
        return value

    def get_typed(self, key: str, type_hint: Type[T], default: T = None) -> Optional[T]:
        """
        获取类型化的配置值

        Args:
            key: 配置键
            type_hint: 期望的类型
            default: 默认值

        Returns:
            类型化的配置值
        """
        value = self._get_nested(key)
        if value is None:
            return default

        if isinstance(value, type_hint):
            return value

        # 尝试类型转换
        try:
            if type_hint == bool and isinstance(value, str):
                return type_hint(value.lower() in ("true", "1", "yes"))
            else:
                return type_hint(value)
        except (ValueError, TypeError):
            logger.warning(f"Type conversion failed for '{key}': {value} -> {type_hint}")
            return default

    def get_int(self, key: str, default: int = 0) -> int:
        """获取整数配置值"""
        return self.get_typed(key, int, default)

    def get_float(self, key: str, default: float = 0.0) -> float:
        """获取浮点数配置值"""
        return self.get_typed(key, float, default)

    def get_bool(self, key: str, default: bool = False) -> bool:
        """获取布尔配置值"""
        return self.get_typed(key, bool, default)

    def get_str(self, key: str, default: str = "") -> str:
        """获取字符串配置值"""
        return self.get_typed(key, str, default)

    def get_list(self, key: str, default: list = None) -> list:
        """获取列表配置值"""
        if default is None:
            default = []
        return self.get_typed(key, list, default)

    def get_dict(self, key: str, default: dict = None) -> dict:
        """获取字典配置值"""
        if default is None:
            default = {}
        return self.get_typed(key, dict, default)

    async def set(self, key: str, value: Any, source: str = "manual") -> bool:
        """
        动态设置配置值

        Args:
            key: 配置键
            value: 配置值
            source: 变更来源标识

        Returns:
            是否成功设置
        """
        old_value = self._get_nested(key)

        # 设置新值
        self._set_nested(key, value)

        # 记录变更
        record = ConfigChangeRecord(
            key=key,
            old_value=old_value,
            new_value=value,
            source=source
        )
        self._change_history.append(record)

        # 通知 watchers
        await self._notify_watchers(key, old_value, value)

        logger.debug(f"Config updated: {key} = {value} (source={source})")
        return True

    async def update(self, updates: Dict[str, Any], source: str = "batch") -> int:
        """
        批量更新配置

        Args:
            updates: 更新的键值对
            source: 来源标识

        Returns:
            成功更新的数量
        """
        count = 0
        for key, value in updates.items():
            if await self.set(key, value, source=source):
                count += 1
        return count

    def register_validator(self, key: str, validator: Callable[[Any], bool]) -> None:
        """
        注册配置验证器

        Args:
            key: 配置键
            validator: 验证函数，返回True表示有效
        """
        if key not in self._validators:
            self._validators[key] = []
        self._validators[key].append(validator)
        logger.debug(f"Registered validator for key: {key}")

    def watch(self, key: str, callback: Callable[[Any, Any], None]) -> None:
        """
        监听配置变更

        Args:
            key: 配置键
            callback: 回调函数 (old_value, new_value)
        """
        if key not in self._watchers:
            self._watchers[key] = []
        self._watchers[key].append(callback)
        logger.debug(f"Registered watcher for key: {key}")

    def watch_all(self, callback: Callable[[str, Any, Any], None]) -> None:
        """
        监听所有配置变更

        Args:
            callback: 回调函数 (key, old_value, new_value)
        """
        self._global_watchers.append(callback)

    def unwatch(self, key: str, callback: Callable) -> bool:
        """
        取消监听

        Returns:
            是否成功移除
        """
        if key in self._watchers:
            try:
                self._watchers[key].remove(callback)
                return True
            except ValueError:
                pass
        return False

    async def validate(self) -> ValidationResult:
        """
        验证所有已注册的配置项

        Returns:
            验证结果
        """
        result = ValidationResult()

        for key, validators in self._validators.items():
            result.checked_keys.append(key)
            value = self._get_nested(key)

            for validator in validators:
                try:
                    is_valid = validator(value)
                    if not is_valid:
                        result.add_error(f"Validation failed for '{key}' with value: {value}")
                except Exception as e:
                    result.add_error(f"Validator error for '{key}': {e}")

        if result.is_valid:
            logger.info(f"Configuration validation passed ({len(result.checked_keys)} keys)")
        else:
            logger.error(f"Configuration validation failed: {result.errors}")

        return result

    def has_key(self, key: str) -> bool:
        """检查配置键是否存在"""
        return self._get_nested(key) is not None

    def get_all_config(self) -> Dict[str, Any]:
        """获取所有配置的副本"""
        return dict(self._config)

    def get_change_history(self, limit: int = 50) -> List[ConfigChangeRecord]:
        """获取配置变更历史"""
        return list(reversed(self._change_history[-limit:]))

    def get_sources_info(self) -> List[Dict[str, Any]]:
        """获取已加载的配置源信息"""
        return [
            {
                "type": src.source_type,
                "location": src.source_location,
                "priority": src.priority,
                "format": src.format
            }
            for src in self._sources
        ]

    def _get_nested(self, key: str) -> Any:
        """获取嵌套配置值"""
        keys = key.split(".")
        current = self._config

        for k in keys:
            if isinstance(current, dict) and k in current:
                current = current[k]
            else:
                return None

        return current

    def _set_nested(self, key: str, value: Any) -> None:
        """设置嵌套配置值"""
        keys = key.split(".")
        current = self._config

        for k in keys[:-1]:
            if k not in current or not isinstance(current[k], dict):
                current[k] = {}
            current = current[k]

        current[keys[-1]] = value

    async def _notify_watchers(self, key: str, old_value: Any, new_value: Any) -> None:
        """通知配置变更监听者"""
        # 键级别的监听者
        if key in self._watchers:
            for watcher in self._watchers[key]:
                try:
                    watcher(old_value, new_value)
                except Exception as e:
                    logger.error(f"Config watcher error for '{key}': {e}")

        # 全局监听者
        for watcher in self._global_watchers:
            try:
                watcher(key, old_value, new_value)
            except Exception as e:
                logger.error(f"Global config watcher error: {e}")

    async def _load_from_file(self, source: ConfigSource) -> int:
        """从文件加载配置"""
        file_path = Path(source.source_location)

        if not file_path.exists():
            raise FileNotFoundError(f"Config file not found: {file_path}")

        content = file_path.read_text(encoding='utf-8')

        # 根据格式解析
        fmt = source.format.lower()
        if fmt == "auto":
            ext = file_path.suffix.lower()
            fmt_map = {
                ".yaml": "yaml", ".yml": "yaml",
                ".json": "json",
                ".toml": "toml",
                ".ini": "ini",
                ".env": "env"
            }
            fmt = fmt_map.get(ext, "json")

        if fmt == "json":
            data = json.loads(content)
        elif fmt in ("yaml", "yml"):
            try:
                import yaml
                data = yaml.safe_load(content)
            except ImportError:
                raise ImportError("PyYAML required for YAML config files")
        elif fmt == "toml":
            try:
                import tomllib
                data = tomllib.loads(content)
            except ImportError:
                try:
                    import tomli
                    data = tomli.loads(content)
                except ImportError:
                    raise ImportError("toml or tomli required for TOML config files")
        elif fmt == "ini":
            import configparser
            parser = configparser.ConfigParser()
            parser.read_string(content)
            data = {section: dict(parser[section]) for section in parser.sections()}
        elif fmt == "env":
            data = self._parse_env_content(content)
        else:
            raise ValueError(f"Unsupported config format: {fmt}")

        # 深度合并到现有配置
        self._deep_merge(data)
        return self._count_keys(data)

    async def _load_from_env(self, source: ConfigSource) -> int:
        """从环境变量加载配置"""
        prefix = source.source_location
        data = {}

        for key, value in os.environ.items():
            if prefix and not key.startswith(prefix):
                continue

            # 移除前缀并转换为嵌套键
            config_key = key[len(prefix):] if prefix else key
            config_key = config_key.lower().replace("__", ".").replace("_", ".")

            # 尝试解析值类型
            parsed_value = self._parse_env_value(value)
            self._set_nested(config_key, parsed_value)
            data[config_key] = parsed_value

        return len(data)

    async def _load_from_dict(self, source: ConfigSource) -> int:
        """从字典加载配置"""
        import json
        data = json.loads(source.source_location)
        self._deep_merge(data)
        return self._count_keys(data)

    async def _load_from_remote(self, source: ConfigSource) -> int:
        """从远程URL加载配置"""
        url = source.source_location
        if not url:
            raise ValueError("Remote config source requires a URL")

        try:
            import urllib.request
            import urllib.error

            headers = {"Accept": "application/json"}
            if source.format and source.format != "auto":
                fmt_headers = {
                    "yaml": "application/yaml",
                    "toml": "application/toml",
                }
                headers["Accept"] = fmt_headers.get(source.format, "application/json")

            req = urllib.request.Request(url, headers=headers)

            timeout_sec = source.reload_interval or 10.0
            loop = asyncio.get_running_loop()
            response = await loop.run_in_executor(
                None,
                lambda: urllib.request.urlopen(req, timeout=int(timeout_sec))
            )
            content = await loop.run_in_executor(None, response.read)
            content_str = content.decode('utf-8')

            fmt = source.format.lower()
            if fmt == "auto":
                content_type = response.headers.get('Content-Type', '')
                if 'yaml' in content_type:
                    fmt = "yaml"
                elif 'toml' in content_type:
                    fmt = "toml"
                elif url.endswith('.yaml') or url.endswith('.yml'):
                    fmt = "yaml"
                elif url.endswith('.toml'):
                    fmt = "toml"
                else:
                    fmt = "json"

            if fmt == "json":
                data = json.loads(content_str)
            elif fmt in ("yaml", "yml"):
                try:
                    import yaml
                    data = yaml.safe_load(content_str)
                except ImportError:
                    raise ImportError("PyYAML required for remote YAML config")
            elif fmt == "toml":
                try:
                    import tomllib
                    data = tomllib.loads(content_str)
                except ImportError:
                    try:
                        import tomli
                        data = tomli.loads(content_str)
                    except ImportError:
                        raise ImportError("toml or tomli required for remote TOML config")
            else:
                data = json.loads(content_str)

            self._deep_merge(data)
            loaded = self._count_keys(data)

            logger.info(f"Loaded {loaded} config entries from remote: {url}")
            return loaded

        except urllib.error.HTTPError as e:
            raise ConnectionError(f"Remote config HTTP error: {e.code} {e.reason}")
        except urllib.error.URLError as e:
            raise ConnectionError(f"Remote config URL error: {e.reason}")
        except Exception as e:
            raise RuntimeError(f"Failed to load remote config from {url}: {e}")

    def _deep_merge(self, new_data: Dict[str, Any]) -> None:
        """深度合并配置字典"""
        for key, value in new_data.items():
            if key in self._config and isinstance(self._config[key], dict) and isinstance(value, dict):
                sub = self._config[key]
                for sub_key, sub_value in value.items():
                    if sub_key in sub and isinstance(sub[sub_key], dict) and isinstance(sub_value, dict):
                        sub[sub_key].update(sub_value)
                    else:
                        sub[sub_key] = sub_value
            else:
                self._config[key] = value

    def _count_keys(self, data: Dict[str, Any], prefix: str = "") -> int:
        """递归计算键的数量"""
        count = 0
        for key, value in data.items():
            full_key = f"{prefix}.{key}" if prefix else key
            if isinstance(value, dict):
                count += self._count_keys(value, full_key)
            else:
                count += 1
        return count

    def _parse_env_value(self, value: str) -> Any:
        """解析环境变量值为合适的Python类型"""
        if value.lower() in ("true", "yes", "1"):
            return True
        elif value.lower() in ("false", "no", "0"):
            return False
        elif value.isdigit():
            return int(value)
        else:
            try:
                return float(value)
            except ValueError:
                return value

    def _parse_env_content(self, content: str) -> Dict[str, Any]:
        """解析.env格式的内容"""
        data = {}
        for line in content.splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip().lower().replace("_", ".")
            data[key] = self._parse_env_value(value.strip())
        return data


__all__ = [
    "ConfigurationCenter",
    "ConfigSource",
    "ValidationResult",
    "ConfigChangeRecord",
]
