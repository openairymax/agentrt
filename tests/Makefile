# AgentOS 测试模块 Makefile
# Version: 1.0.0.6
# Last updated: 2026-03-22

.PHONY: help install test test-unit test-integration test-security test-benchmark
.PHONY: clean coverage lint format type-check security-scan ci-setup
.PHONY: setup-env check-env run-all generate-report

# 默认目标
help:
	@echo "AgentOS 测试模块命令:"
	@echo ""
	@echo "环境设置:"
	@echo "  install      - 安装测试依赖"
	@echo "  setup-env    - 设置测试环境"
	@echo "  check-env    - 检查环境配置"
	@echo ""
	@echo "测试执行:"
	@echo "  test         - 运行所有测试"
	@echo "  test-unit    - 运行单元测试"
	@echo "  test-integration - 运行集成测试"
	@echo "  test-security - 运行安全测试"
	@echo "  test-benchmark - 运行性能测试"
	@echo "  run-all      - 运行所有测试和检查"
	@echo ""
	@echo "代码质量:"
	@echo "  lint         - 代码风格检查"
	@echo "  format       - 代码格式化"
	@echo "  type-check   - 类型检查"
	@echo "  security-scan - 安全扫描"
	@echo ""
	@echo "报告和清理:"
	@echo "  coverage     - 生成覆盖率报告"
	@echo "  generate-report - 生成完整测试报告"
	@echo "  clean        - 清理临时文件"

# 变量定义
PYTHON := python3
PIP := pip3
PYTEST := pytest
COVERAGE := coverage
BLACK := black
ISORT := isort
MYPY := mypy
FLAKE8 := flake8
BANDIT := bandit

# 测试目录
TEST_DIR := .
UNIT_DIR := unit
INTEGRATION_DIR := integration
SECURITY_DIR := security
BENCHMARK_DIR := benchmarks

# 配置文件
PYTEST_INI := pytest.ini
REQUIREMENTS := requirements.txt
ENV_FILE := .env.test

# 安装依赖
install:
	@echo "📦 安装测试依赖..."
	$(PIP) install -r $(REQUIREMENTS)
	@echo "✅ 依赖安装完成"

# 设置环境
setup-env:
	@echo "🔧 设置测试环境..."
	@if [ ! -f .env ]; then cp $(ENV_FILE) .env; fi
	@mkdir -p logs reports artifacts
	@echo "✅ 环境设置完成"

# 检查环境
check-env:
	@echo "🔍 检查环境配置..."
	$(PYTHON) run_tests.py
	@echo "✅ 环境检查完成"

# 运行所有测试
test:
	@echo "🧪 运行所有测试..."
	$(PYTEST) $(TEST_DIR) -v --tb=short

# 运行单元测试
test-unit:
	@echo "🔬 运行单元测试..."
	$(PYTEST) $(UNIT_DIR) -v --tb=short

# 运行集成测试
test-integration:
	@echo "🔗 运行集成测试..."
	$(PYTEST) $(INTEGRATION_DIR) -v --tb=short

# 运行安全测试
test-security:
	@echo "🔒 运行安全测试..."
	$(PYTEST) $(SECURITY_DIR) -v --tb=short

# 运行性能测试
test-benchmark:
	@echo "⚡ 运行性能测试..."
	$(PYTEST) $(BENCHMARK_DIR) -v --tb=short

# 代码风格检查
lint:
	@echo "🔍 代码风格检查..."
	$(FLAKE8) $(TEST_DIR) --max-line-length=100 --ignore=E203,W503
	@echo "✅ 代码风格检查完成"

# 代码格式化
format:
	@echo "🎨 代码格式化..."
	$(BLACK) $(TEST_DIR) --line-length=100
	$(ISORT) $(TEST_DIR) --profile black
	@echo "✅ 代码格式化完成"

# 类型检查
type-check:
	@echo "🔬 类型检查..."
	$(MYPY) $(TEST_DIR) --ignore-missing-imports
	@echo "✅ 类型检查完成"

# 安全扫描
security-scan:
	@echo "🔒 安全扫描..."
	$(BANDIT) -r $(TEST_DIR) -f json -o reports/security_report.json
	@echo "✅ 安全扫描完成"

# 生成覆盖率报告
coverage:
	@echo "📊 生成覆盖率报告..."
	$(COVERAGE) run -m pytest $(TEST_DIR)
	$(COVERAGE) report
	$(COVERAGE) html -d reports/coverage
	@echo "✅ 覆盖率报告生成完成"

# 生成完整测试报告
generate-report:
	@echo "📋 生成完整测试报告..."
	$(PYTEST) $(TEST_DIR) --html=reports/test_report.html --self-contained-html
	$(PYTEST) $(TEST_DIR) --json-report --json-report-file=reports/test_report.json
	@echo "✅ 测试报告生成完成"

# 运行所有检查和测试
run-all: setup-env check-env lint type-check security-scan test coverage generate-report
	@echo "🎉 所有检查和测试完成!"

# 清理临时文件
clean:
	@echo "🧹 清理临时文件..."
	@rm -rf .coverage
	@rm -rf htmlcov
	@rm -rf .pytest_cache
	@rm -rf reports/*
	@rm -rf logs/*
	@rm -rf artifacts/*
	@rm -rf .mypy_cache
	@rm -rf .tox
	@find . -type d -name __pycache__ -exec rm -rf {} +
	@find . -type f -name "*.pyc" -delete
	@echo "✅ 清理完成"

# CI/CD 设置
ci-setup: install setup-env
	@echo "🚀 CI/CD 环境设置完成"

# 快速测试（开发用）
quick-test:
	@echo "⚡ 快速测试..."
	$(PYTEST) $(UNIT_DIR) -x --tb=short

# 并行测试
parallel-test:
	@echo "🔄 并行测试..."
	$(PYTEST) $(TEST_DIR) -n auto --tb=short

# 监视模式测试
watch-test:
	@echo "👀 监视模式测试..."
	$(PYTEST) $(TEST_DIR) -f --tb=short