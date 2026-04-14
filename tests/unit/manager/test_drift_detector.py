#!/usr/bin/env python3
"""
AgentOS Configuration Drift Detector Tests

测试配置漂移检测器的功能

Usage:
    python test_drift_detector.py
    pytest test_drift_detector.py -v
"""

import json
import os
import pytest
import shutil
import tempfile
from datetime import datetime
from pathlib import Path
import sys

# 添加父目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent / "tools"))

from drift_detector import ConfigDriftDetector, DriftType, DriftSeverity


class TestConfigDriftDetector:
    """配置漂移检测器测试"""
    
    @pytest.fixture
    def temp_config_dir(self):
        """创建临时配置目录"""
        temp_dir = tempfile.mkdtemp()
        config_path = Path(temp_dir) / "config"
        config_path.mkdir()
        
        # 创建一些测试配置文件
        (config_path / "kernel").mkdir()
        (config_path / "security").mkdir()
        (config_path / "model").mkdir()
        
        # 创建配置文件
        (config_path / "kernel" / "settings.yaml").write_text(
            "kernel:\n  log_level: info\n  max_workers: 4\n",
            encoding='utf-8'
        )
        (config_path / "security" / "policy.yaml").write_text(
            "security:\n  sandbox: process\n",
            encoding='utf-8'
        )
        (config_path / "model" / "model.yaml").write_text(
            "model:\n  provider: openai\n",
            encoding='utf-8'
        )
        
        yield config_path
        
        # 清理临时目录
        shutil.rmtree(temp_dir)
    
    @pytest.fixture
    def detector(self, temp_config_dir):
        """创建检测器实例"""
        return ConfigDriftDetector(temp_config_dir, verbose=False)
    
    def test_create_baseline(self, detector, temp_config_dir):
        """测试创建基线"""
        baseline_path = detector.create_baseline()
        
        assert baseline_path.exists()
        assert baseline_path.name == "manifest.json"
        
        # 验证基线内容
        manifest = json.loads(baseline_path.read_text(encoding='utf-8'))
        
        assert "created_at" in manifest
        assert "version" in manifest
        assert "config_dir" in manifest
        assert "files" in manifest
        
        # 应该有3个配置文件
        assert len(manifest["files"]) == 3
        
        # 验证每个文件的哈希
        for rel_path, file_info in manifest["files"].items():
            assert "sha256" in file_info
            assert "size" in file_info
            assert "last_modified" in file_info
            assert len(file_info["sha256"]) == 64  # SHA-256哈希长度
    
    def test_detect_no_drift(self, detector, temp_config_dir):
        """测试无漂移检测"""
        # 先创建基线
        detector.create_baseline()
        
        # 检测漂移（应该没有）
        report = detector.detect_drift()
        
        assert not report.has_drift
        assert report.drifted_files == 0
        assert report.unchanged_files == 3
        assert report.drift_rate == 0.0
    
    def test_detect_modified_file(self, detector, temp_config_dir):
        """测试检测文件修改"""
        # 创建基线
        detector.create_baseline()
        
        # 修改一个文件
        settings_file = temp_config_dir / "kernel" / "settings.yaml"
        settings_file.write_text(
            "kernel:\n  log_level: debug\n  max_workers: 8\n",
            encoding='utf-8'
        )
        
        # 检测漂移
        report = detector.detect_drift()
        
        assert report.has_drift
        assert report.drifted_files == 1
        
        # 找到修改的文件
        drift_item = next(
            item for item in report.drift_items 
            if item.file_path == "kernel/settings.yaml"
        )
        
        assert drift_item.drift_type == DriftType.MODIFIED.value
        assert drift_item.severity == DriftSeverity.INFO.value
        assert drift_item.details == "File content has been modified"
    
    def test_detect_deleted_file(self, detector, temp_config_dir):
        """测试检测文件删除"""
        # 创建基线
        detector.create_baseline()
        
        # 删除一个文件
        policy_file = temp_config_dir / "security" / "policy.yaml"
        policy_file.unlink()
        
        # 检测漂移
        report = detector.detect_drift()
        
        assert report.has_drift
        assert report.drifted_files == 1
        
        # 找到删除的文件
        drift_item = next(
            item for item in report.drift_items 
            if item.file_path == "security/policy.yaml"
        )
        
        assert drift_item.drift_type == DriftType.DELETED.value
        assert drift_item.severity == DriftSeverity.CRITICAL.value
        assert drift_item.current_hash == "[DELETED]"
    
    def test_detect_added_file(self, detector, temp_config_dir):
        """测试检测文件新增"""
        # 创建基线
        detector.create_baseline()
        
        # 新增一个文件
        new_file = temp_config_dir / "agent" / "registry.yaml"
        new_file.parent.mkdir()
        new_file.write_text("agents: []\n", encoding='utf-8')
        
        # 检测漂移
        report = detector.detect_drift()
        
        assert report.has_drift
        assert report.drifted_files == 1
        
        # 找到新增的文件
        drift_item = next(
            item for item in report.drift_items 
            if item.file_path == "agent/registry.yaml"
        )
        
        assert drift_item.drift_type == DriftType.ADDED.value
        assert drift_item.severity == DriftSeverity.INFO.value
        assert drift_item.baseline_hash == "[NEW FILE]"
    
    def test_sensitive_file_severity(self, detector, temp_config_dir):
        """测试敏感文件严重程度"""
        # 创建基线
        detector.create_baseline()
        
        # 修改敏感文件
        policy_file = temp_config_dir / "security" / "policy.yaml"
        policy_file.write_text(
            "security:\n  sandbox: container\n",
            encoding='utf-8'
        )
        
        # 检测漂移
        report = detector.detect_drift()
        
        drift_item = next(
            item for item in report.drift_items 
            if item.file_path == "security/policy.yaml"
        )
        
        assert drift_item.severity == DriftSeverity.CRITICAL.value
    
    def test_export_report_json(self, detector, temp_config_dir, tmp_path):
        """测试导出JSON报告"""
        # 创建基线并修改文件
        detector.create_baseline()
        settings_file = temp_config_dir / "kernel" / "settings.yaml"
        settings_file.write_text(
            "kernel:\n  log_level: debug\n",
            encoding='utf-8'
        )
        
        # 检测并导出
        report = detector.detect_drift()
        output_path = tmp_path / "drift_report.json"
        detector.export_report_json(report, output_path)
        
        # 验证导出内容
        assert output_path.exists()
        data = json.loads(output_path.read_text(encoding='utf-8'))
        
        assert "scan_time" in data
        assert "config_dir" in data
        assert "baseline_created" in data
        assert "total_files_scanned" in data
        assert "drifted_files" in data
        assert "has_drift" in data
        assert "drift_items" in data
        assert len(data["drift_items"]) == 1
    
    def test_export_report_markdown(self, detector, temp_config_dir, tmp_path):
        """测试导出Markdown报告"""
        # 创建基线并修改文件
        detector.create_baseline()
        settings_file = temp_config_dir / "kernel" / "settings.yaml"
        settings_file.write_text(
            "kernel:\n  log_level: debug\n",
            encoding='utf-8'
        )
        
        # 检测并导出
        report = detector.detect_drift()
        output_path = tmp_path / "drift_report.md"
        detector.export_report_markdown(report, output_path)
        
        # 验证导出内容
        assert output_path.exists()
        md_content = output_path.read_text(encoding='utf-8')
        
        assert "# Configuration Drift Report" in md_content
        assert "Summary" in md_content
        assert "Drift Details" in md_content
        assert "kernel/settings.yaml" in md_content
    
    def test_ignore_patterns(self, detector, temp_config_dir):
        """测试忽略模式"""
        # 创建应该被忽略的文件
        (temp_config_dir / "test.pyc").write_text("test", encoding='utf-8')
        (temp_config_dir / "__pycache__").mkdir(exist_ok=True)
        (temp_config_dir / "__pycache__" / "test.pyc").write_text("test", encoding='utf-8')
        (temp_config_dir / "test.log").write_text("test", encoding='utf-8')
        
        # 获取配置文件
        files = detector._get_config_files()
        
        # 验证忽略的文件不在列表中
        file_names = [f.name for f in files]
        assert "test.pyc" not in file_names
        assert "test.log" not in file_names
    
    def test_baseline_not_found_error(self, temp_config_dir):
        """测试基线不存在错误"""
        detector = ConfigDriftDetector(temp_config_dir, verbose=False)
        
        with pytest.raises(RuntimeError) as exc_info:
            detector.detect_drift()
        
        assert "Baseline not found" in str(exc_info.value)
    
    def test_drift_rate_calculation(self, detector, temp_config_dir):
        """测试漂移率计算"""
        # 创建基线
        detector.create_baseline()
        
        # 修改所有文件
        for yaml_file in temp_config_dir.rglob("*.yaml"):
            yaml_file.write_text(f"modified: {datetime.now()}\n", encoding='utf-8')
        
        # 检测漂移
        report = detector.detect_drift()
        
        assert report.drift_rate == 100.0
        assert report.drifted_files == 3
        assert report.total_files_scanned == 3


class TestDriftDetectorCLI:
    """漂移检测器CLI测试"""
    
    @pytest.fixture
    def temp_config_dir(self):
        """创建临时配置目录"""
        temp_dir = tempfile.mkdtemp()
        config_path = Path(temp_dir) / "config"
        config_path.mkdir()
        
        # 创建测试文件
        (config_path / "test.yaml").write_text("test: value\n", encoding='utf-8')
        
        yield config_path
        
        shutil.rmtree(temp_dir)
    
    def test_cli_create_baseline(self, temp_config_dir):
        """测试CLI创建基线"""
        import subprocess
        
        result = subprocess.run(
            [
                sys.executable,
                str(Path(__file__).parent.parent / "tools" / "drift_detector.py"),
                "--config-dir", str(temp_config_dir),
                "--action", "create-baseline",
                "--verbose"
            ],
            capture_output=True,
            text=True,
            cwd=temp_config_dir
        )
        
        assert result.returncode == 0
        assert "Baseline created" in result.stdout
    
    def test_cli_detect(self, temp_config_dir):
        """测试CLI检测漂移"""
        import subprocess
        
        # 先创建基线
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).parent.parent / "tools" / "drift_detector.py"),
                "--config-dir", str(temp_config_dir),
                "--action", "create-baseline"
            ],
            capture_output=True
        )
        
        # 修改文件
        (temp_config_dir / "test.yaml").write_text("test: modified\n", encoding='utf-8')
        
        # 检测漂移
        result = subprocess.run(
            [
                sys.executable,
                str(Path(__file__).parent.parent / "tools" / "drift_detector.py"),
                "--config-dir", str(temp_config_dir),
                "--action", "detect",
                "--output", "test_drift_report.json"
            ],
            capture_output=True,
            text=True,
            cwd=temp_config_dir
        )
        
        assert result.returncode == 0
        assert "drift" in result.stdout.lower() or "Drift" in result.stdout
        
        # 验证报告文件
        report_file = temp_config_dir / "test_drift_report.json"
        assert report_file.exists()


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
