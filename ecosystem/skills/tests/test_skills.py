# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""Tests for ecosystem/skills/ — CodeReview, WebSearch, DataAnalysis, SecurityAudit, TextSummarization."""

import pytest

from ecosystem.skills.src.code_review import CodeReviewSkill
from ecosystem.skills.src.web_search import WebSearchSkill
from ecosystem.skills.src.data_analysis import DataAnalysisSkill
from ecosystem.skills.src.security_audit import SecurityAuditSkill
from ecosystem.skills.src.text_summarization import TextSummarizationSkill


# ============================================================
# CodeReviewSkill
# ============================================================

class TestCodeReviewDefinition:
    def test_get_definition(self):
        skill = CodeReviewSkill()
        d = skill.get_definition()
        assert d.name == "code_review"
        assert d.category == "development"
        assert d.version == "1.0.0"
        assert "code" in d.input_schema.get("properties", {})
        assert "language" in d.input_schema.get("properties", {})

    def test_get_prompt_template(self):
        skill = CodeReviewSkill()
        tmpl = skill.get_prompt_template()
        assert "{code}" in tmpl
        assert "{language}" in tmpl
        assert "{focus}" in tmpl

    def test_get_system_instructions(self):
        skill = CodeReviewSkill()
        inst = skill.get_system_instructions()
        assert "code reviewer" in inst.lower()


class TestCodeReviewValidate:
    def test_valid_input(self):
        skill = CodeReviewSkill()
        assert skill.validate_input({"code": "print('hello')", "language": "python"}) is True

    def test_empty_code(self):
        skill = CodeReviewSkill()
        assert skill.validate_input({"code": "", "language": "python"}) is False

    def test_unsupported_language(self):
        skill = CodeReviewSkill()
        assert skill.validate_input({"code": "x", "language": "brainfuck"}) is False

    def test_typescript_supported(self):
        skill = CodeReviewSkill()
        assert skill.validate_input({"code": "const x = 1", "language": "typescript"}) is True


class TestCodeReviewQuickScan:
    def test_detects_eval(self):
        skill = CodeReviewSkill()
        findings = skill._quick_scan("eval('1+1')")
        assert any("eval" in f["title"].lower() for f in findings)

    def test_detects_hardcoded_secret(self):
        skill = CodeReviewSkill()
        findings = skill._quick_scan('password = "secret123"')
        assert any("hardcoded" in f["title"].lower() for f in findings)

    def test_detects_subprocess_shell_true(self):
        skill = CodeReviewSkill()
        findings = skill._quick_scan("subprocess.call('ls', shell=True)")
        assert any("shell" in f["title"].lower() for f in findings)

    def test_no_false_positive(self):
        skill = CodeReviewSkill()
        findings = skill._quick_scan("print('hello world')")
        assert len(findings) == 0


class TestCodeReviewStaticAnalysis:
    def test_detects_long_line(self):
        skill = CodeReviewSkill()
        code = "x = " + "a" * 130
        findings = skill._static_analysis(code, "python")
        assert any("too long" in f["title"].lower() for f in findings)

    def test_detects_bare_except(self):
        skill = CodeReviewSkill()
        findings = skill._static_analysis("try:\n    pass\nexcept:\n    pass", "python")
        assert any("bare except" in f["title"].lower() for f in findings)

    def test_detects_wildcard_import(self):
        skill = CodeReviewSkill()
        findings = skill._static_analysis("from os import *", "python")
        assert any("wildcard" in f["title"].lower() for f in findings)

    def test_normal_line_ok(self):
        skill = CodeReviewSkill()
        code = "x = 1\ny = 2\n"
        findings = skill._static_analysis(code, "python")
        assert len(findings) == 0


class TestCodeReviewScore:
    def test_perfect_score(self):
        skill = CodeReviewSkill()
        score = skill._compute_score([], 100)
        assert score == 100.0

    def test_with_critical(self):
        skill = CodeReviewSkill()
        findings = [{"severity": "critical", "title": "test"}]
        score = skill._compute_score(findings, 100)
        assert score == 75.0

    def test_mixed(self):
        skill = CodeReviewSkill()
        findings = [
            {"severity": "high", "title": "a"},
            {"severity": "medium", "title": "b"},
            {"severity": "low", "title": "c"},
        ]
        score = skill._compute_score(findings, 100)
        assert score == 74.0

    def test_floor_zero(self):
        skill = CodeReviewSkill()
        findings = [{"severity": "critical", "title": "c"} for _ in range(10)]
        score = skill._compute_score(findings, 100)
        assert score == 0.0


class TestCodeReviewPreExecute:
    @pytest.mark.asyncio
    async def test_sets_defaults(self):
        skill = CodeReviewSkill()
        ctx = await skill.pre_execute({"code": "x", "language": "python"})
        assert ctx["focus"] == "all"
        assert ctx["severity_threshold"] == "low"
        assert "quick_scan_hints" in ctx


class TestCodeReviewExecute:
    @pytest.mark.asyncio
    async def test_execute_python_code(self):
        skill = CodeReviewSkill()
        ctx = await skill.pre_execute({
            "code": "eval('1+1')\nprint('hello')\n",
            "language": "python",
        })
        result = await skill.execute(ctx)
        assert "overall_score" in result
        assert "findings" in result
        assert "summary" in result
        assert result["language"] == "python"
        assert result["overall_score"] < 100.0  # eval should be penalized

    @pytest.mark.asyncio
    async def test_execute_clean_code(self):
        skill = CodeReviewSkill()
        result = await skill.execute({
            "code": "x = 1\ny = 2\nz = x + y\n",
            "language": "python",
            "focus": "all",
            "quick_scan_hints": [],
        })
        assert result["overall_score"] == 100.0


class TestCodeReviewPostExecute:
    @pytest.mark.asyncio
    async def test_filters_by_threshold(self):
        skill = CodeReviewSkill()
        result = {
            "findings": [
                {"severity": "critical", "title": "a"},
                {"severity": "info", "title": "b"},
            ],
        }
        ctx = {"severity_threshold": "low"}
        filtered = await skill.post_execute(ctx, result)
        assert len(filtered["findings"]) == 1
        assert filtered["findings"][0]["severity"] == "critical"


# ============================================================
# WebSearchSkill
# ============================================================

class TestWebSearchDefinition:
    def test_get_definition(self):
        skill = WebSearchSkill()
        d = skill.get_definition()
        assert d.name == "web_search"
        assert d.category == "information"
        assert "query" in d.input_schema.get("properties", {})


class TestWebSearchValidate:
    def test_valid_input(self):
        skill = WebSearchSkill()
        assert skill.validate_input({"query": "Rust async best practices"}) is True

    def test_empty_query(self):
        skill = WebSearchSkill()
        assert skill.validate_input({"query": ""}) is False

    def test_query_too_long(self):
        skill = WebSearchSkill()
        assert skill.validate_input({"query": "x" * 501}) is False


class TestWebSearchNormalizeUrl:
    def test_removes_www(self):
        result = WebSearchSkill._normalize_url("https://www.example.com/path")
        assert "www." not in result

    def test_removes_utm_params(self):
        result = WebSearchSkill._normalize_url("https://example.com?utm_source=twitter&ref=share")
        assert "utm_source" not in result
        assert "ref" not in result

    def test_lowercase_and_trailing_slash(self):
        result = WebSearchSkill._normalize_url("HTTPS://Example.COM/Path/")
        assert result == "example.com/path"


class TestWebSearchDeduplicate:
    def test_removes_duplicates(self):
        skill = WebSearchSkill()
        results = [
            {"url": "https://example.com/a", "title": "A"},
            {"url": "https://www.example.com/a", "title": "A duplicate"},
            {"url": "https://example.com/b", "title": "B"},
        ]
        deduped = skill._deduplicate(results)
        assert len(deduped) == 2

    def test_no_duplicates(self):
        skill = WebSearchSkill()
        results = [
            {"url": "https://a.com", "title": "A"},
            {"url": "https://b.com", "title": "B"},
        ]
        assert len(skill._deduplicate(results)) == 2


class TestWebSearchRankResults:
    def test_authority_boost(self):
        skill = WebSearchSkill()
        results = [
            {"url": "https://arbitrary.com", "title": "Rust async", "snippet": "Rust async programming", "source": "web", "relevance": 0.5},
            {"url": "https://docs.rs/tokio", "title": "Rust async", "snippet": "Rust async programming", "source": "docs", "relevance": 0.5},
        ]
        ranked = skill._rank_results(results, "Rust async")
        # docs.rs should get higher relevance
        assert ranked[0]["url"] == "https://docs.rs/tokio"

    def test_keyword_match(self):
        skill = WebSearchSkill()
        results = [
            {"url": "https://a.com", "title": "unrelated", "snippet": "nothing here", "source": "web", "relevance": 0.5},
            {"url": "https://b.com", "title": "Python tutorial", "snippet": "learn Python", "source": "web", "relevance": 0.5},
        ]
        ranked = skill._rank_results(results, "Python tutorial")
        assert ranked[0]["url"] == "https://b.com"


class TestWebSearchGenerateLinks:
    def test_generates_search_links(self):
        skill = WebSearchSkill()
        links = skill._generate_search_links("test query")
        assert len(links) >= 2
        assert any("duckduckgo" in l["url"] for l in links)
        assert all(l["relevance"] > 0 for l in links)


class TestWebSearchGenerateSummary:
    def test_with_results(self):
        skill = WebSearchSkill()
        results = [
            {"title": "A", "url": "https://a.com", "source": "docs", "snippet": "..."},
            {"title": "B", "url": "https://b.com", "source": "blog", "snippet": "..."},
            {"title": "C", "url": "https://c.com", "source": "wiki", "snippet": "..."},
        ]
        summary = skill._generate_summary("test", results)
        assert "3 results" in summary
        assert "A" in summary

    def test_empty(self):
        skill = WebSearchSkill()
        summary = skill._generate_summary("test", [])
        assert "No results" in summary


class TestWebSearchPreExecute:
    @pytest.mark.asyncio
    async def test_sets_defaults(self):
        skill = WebSearchSkill()
        ctx = await skill.pre_execute({"query": "test"})
        assert ctx["max_results"] == 10
        assert ctx["engine"] == "auto"
        assert ctx["time_range"] == "all"


class TestWebSearchExecute:
    @pytest.mark.asyncio
    async def test_execute_fallback(self):
        skill = WebSearchSkill()
        result = await skill.execute({"query": "test query", "max_results": 5})
        assert result["query"] == "test query"
        assert "results" in result
        assert result["total_found"] > 0
        assert "summary" in result


# ============================================================
# DataAnalysisSkill
# ============================================================

class TestDataAnalysisDefinition:
    def test_get_definition(self):
        skill = DataAnalysisSkill()
        d = skill.get_definition()
        assert d.name == "data_analysis"
        assert d.category == "analytics"


class TestDataAnalysisValidate:
    def test_valid_input(self):
        skill = DataAnalysisSkill()
        assert skill.validate_input({"data": '[{"a": 1, "b": 2}]'}) is True

    def test_empty_data(self):
        skill = DataAnalysisSkill()
        assert skill.validate_input({"data": ""}) is False


class TestDataAnalysisParseJson:
    def test_parse_list(self):
        result = DataAnalysisSkill._parse_json('[{"a": 1}, {"a": 2}]')
        assert len(result) == 2
        assert result[0]["a"] == 1

    def test_parse_dict(self):
        result = DataAnalysisSkill._parse_json('{"a": 1, "b": 2}')
        assert len(result) == 1
        assert result[0]["a"] == 1

    def test_invalid_json(self):
        result = DataAnalysisSkill._parse_json("not json")
        assert len(result) == 0


class TestDataAnalysisParseCsv:
    def test_parse_csv(self):
        raw = "name,age,score\nAlice,30,95\nBob,25,87"
        result = DataAnalysisSkill._parse_csv(raw)
        assert len(result) == 2
        assert result[0]["name"] == "Alice"
        assert result[0]["age"] == 30
        assert result[0]["score"] == 95

    def test_empty_csv(self):
        assert DataAnalysisSkill._parse_csv("") == []

    def test_mismatched_columns(self):
        raw = "name,age\nAlice,30,extra"
        result = DataAnalysisSkill._parse_csv(raw)
        assert len(result) == 0


class TestDataAnalysisDetectNumeric:
    def test_detects_numeric(self):
        records = [{"a": 1, "b": "text", "c": 3.14}]
        fields = DataAnalysisSkill._detect_numeric_fields(records, [])
        assert "a" in fields
        assert "b" not in fields
        assert "c" in fields

    def test_with_target_filter(self):
        records = [{"a": 1, "b": 2, "c": "text"}]
        fields = DataAnalysisSkill._detect_numeric_fields(records, ["a", "c"])
        assert fields == ["a"]


class TestDataAnalysisPercentile:
    def test_median(self):
        assert DataAnalysisSkill._percentile([1, 2, 3, 4, 5], 50) == 3.0

    def test_quartiles(self):
        assert DataAnalysisSkill._percentile([1, 2, 3, 4, 5], 25) == 2.0
        assert DataAnalysisSkill._percentile([1, 2, 3, 4, 5], 75) == 4.0

    def test_single_element(self):
        assert DataAnalysisSkill._percentile([42], 50) == 42.0


class TestDataAnalysisSkewness:
    def test_symmetric(self):
        assert DataAnalysisSkill._skewness([1, 2, 3, 2, 1], 1.8, 0.837) == pytest.approx(0.0, abs=0.5)

    def test_zero_std(self):
        assert DataAnalysisSkill._skewness([1, 1, 1], 1.0, 0.0) == 0.0


class TestDataAnalysisLinearRegression:
    def test_positive_trend(self):
        slope, intercept, r_sq = DataAnalysisSkill._linear_regression(
            [0, 1, 2, 3, 4], [0.0, 1.0, 2.0, 3.0, 4.0]
        )
        assert slope == pytest.approx(1.0)
        assert r_sq == pytest.approx(1.0)

    def test_small_sample(self):
        slope, intercept, r_sq = DataAnalysisSkill._linear_regression([0], [0])
        assert slope == 0.0
        assert r_sq == 0.0


class TestDataAnalysisDescriptive:
    def test_stats(self):
        skill = DataAnalysisSkill()
        records = [{"x": 1}, {"x": 2}, {"x": 3}, {"x": 4}, {"x": 5}]
        stats = skill._descriptive_statistics(records, ["x"])
        assert "x" in stats
        assert stats["x"]["mean"] == 3.0
        assert stats["x"]["median"] == 3.0
        assert stats["x"]["min"] == 1
        assert stats["x"]["max"] == 5


class TestDataAnalysisOutliers:
    def test_no_outliers(self):
        skill = DataAnalysisSkill()
        records = [{"x": 1}, {"x": 2}, {"x": 3}, {"x": 4}, {"x": 5}]
        outliers = skill._detect_outliers(records, ["x"])
        assert "x" in outliers
        assert outliers["x"]["iqr_method"]["count"] == 0

    def test_with_outlier(self):
        skill = DataAnalysisSkill()
        records = [{"x": 1}, {"x": 2}, {"x": 3}, {"x": 4}, {"x": 100}]
        outliers = skill._detect_outliers(records, ["x"])
        assert outliers["x"]["iqr_method"]["count"] > 0


class TestDataAnalysisTrends:
    def test_upward_trend(self):
        skill = DataAnalysisSkill()
        records = [{"x": 1}, {"x": 2}, {"x": 3}, {"x": 4}, {"x": 5}]
        trends = skill._detect_trends(records, ["x"])
        assert trends["x"]["direction"] == "upward"
        assert trends["x"]["r_squared"] == pytest.approx(1.0)

    def test_flat_trend(self):
        skill = DataAnalysisSkill()
        records = [{"x": 5}, {"x": 5}, {"x": 5}]
        trends = skill._detect_trends(records, ["x"])
        assert trends["x"]["direction"] == "flat"


class TestDataAnalysisExecute:
    @pytest.mark.asyncio
    async def test_execute_json(self):
        skill = DataAnalysisSkill()
        result = await skill.execute({
            "data": '[{"x": 1}, {"x": 2}, {"x": 3}, {"x": 4}, {"x": 5}]',
            "format": "json",
            "analysis_type": "all",
            "target_fields": [],
        })
        assert result["record_count"] == 5
        assert "x" in result["fields_analyzed"]
        assert "descriptive_stats" in result
        assert "insights" in result

    @pytest.mark.asyncio
    async def test_execute_empty(self):
        skill = DataAnalysisSkill()
        result = await skill.execute({"data": "", "format": "auto"})
        assert result["record_count"] == 0


# ============================================================
# SecurityAuditSkill
# ============================================================

class TestSecurityAuditDefinition:
    def test_get_definition(self):
        skill = SecurityAuditSkill()
        d = skill.get_definition()
        assert d.name == "security_audit"
        assert d.category == "security"


class TestSecurityAuditValidate:
    def test_valid_input(self):
        skill = SecurityAuditSkill()
        assert skill.validate_input({"target": "SELECT * FROM users"}) is True

    def test_empty_target(self):
        skill = SecurityAuditSkill()
        assert skill.validate_input({"target": ""}) is False

    def test_invalid_audit_type(self):
        skill = SecurityAuditSkill()
        assert skill.validate_input({"target": "x", "audit_type": "invalid"}) is False


class TestSecurityAuditDetectType:
    def test_detect_config(self):
        skill = SecurityAuditSkill()
        assert skill._detect_audit_type("nginx.conf server { ... }") == "config"

    def test_detect_dependencies(self):
        skill = SecurityAuditSkill()
        assert skill._detect_audit_type("requirements.txt flask==2.0") == "dependencies"

    def test_detect_permissions(self):
        skill = SecurityAuditSkill()
        assert skill._detect_audit_type("chmod 777 /var/www") == "permissions"

    def test_detect_network(self):
        skill = SecurityAuditSkill()
        assert skill._detect_audit_type("port 443 firewall") == "network"

    def test_detect_compliance(self):
        skill = SecurityAuditSkill()
        assert skill._detect_audit_type("owasp top 10 compliance") == "compliance"

    def test_default_config(self):
        skill = SecurityAuditSkill()
        assert skill._detect_audit_type("random text") == "config"


class TestSecurityAuditQuickScan:
    def test_detects_hardcoded_password(self):
        skill = SecurityAuditSkill()
        findings = skill._quick_scan('password = "secret123"', "auto")
        assert any("hardcoded" in f["title"].lower() for f in findings)

    def test_detects_chmod_777(self):
        skill = SecurityAuditSkill()
        findings = skill._quick_scan("chmod 777 /var/www", "auto")
        assert any("777" in f["title"] for f in findings)

    def test_detects_ssl_v3(self):
        skill = SecurityAuditSkill()
        findings = skill._quick_scan("SSLv3 enabled", "auto")
        assert any("ssl" in f["title"].lower() for f in findings)


class TestSecurityAuditConfig:
    def test_detects_http_not_localhost(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_config("http://example.com/api")
        assert any("http" in f["title"].lower() for f in findings)

    def test_ignores_localhost(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_config("http://localhost:8080")
        assert len(findings) == 0

    def test_detects_md5(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_config("hash = md5(password)")
        assert any("md5" in f["title"].lower() or "hash" in f["title"].lower() for f in findings)


class TestSecurityAuditDependencies:
    def test_detects_vulnerable_requests(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_dependencies("requests==2.31.0")
        assert any("requests" in f["title"].lower() for f in findings)

    def test_no_false_positive(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_dependencies("requests>=2.32.0")
        assert len(findings) == 0


class TestSecurityAuditPermissions:
    def test_detects_chmod_777(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_permissions("chmod 777 /tmp/file")
        assert any("777" in f["title"] for f in findings)

    def test_detects_user_root(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_permissions("USER root")
        assert any("root" in f["title"].lower() for f in findings)

    def test_detects_privileged(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_permissions("docker run --privileged ...")
        assert any("privileged" in f["title"].lower() for f in findings)


class TestSecurityAuditNetwork:
    def test_detects_telnet(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_network("telnet server.example.com")
        assert any("telnet" in f["title"].lower() for f in findings)

    def test_detects_ftp(self):
        skill = SecurityAuditSkill()
        findings = skill._audit_network("ftp server.example.com")
        assert any("ftp" in f["title"].lower() for f in findings)


class TestSecurityAuditRiskScore:
    def test_empty(self):
        skill = SecurityAuditSkill()
        assert skill._compute_risk_score([]) == 0.0

    def test_critical(self):
        skill = SecurityAuditSkill()
        findings = [{"severity": "critical", "title": "test"}]
        assert skill._compute_risk_score(findings) == 20.0

    def test_mixed(self):
        skill = SecurityAuditSkill()
        findings = [
            {"severity": "critical", "title": "a"},
            {"severity": "high", "title": "b"},
            {"severity": "medium", "title": "c"},
        ]
        score = skill._compute_risk_score(findings)
        assert score == 38.0


class TestSecurityAuditRiskLevel:
    def test_levels(self):
        skill = SecurityAuditSkill()
        assert skill._risk_level(0) == "info"
        assert skill._risk_level(12) == "low"
        assert skill._risk_level(30) == "medium"
        assert skill._risk_level(60) == "high"
        assert skill._risk_level(85) == "critical"


class TestSecurityAuditVersionLe:
    def test_less(self):
        assert SecurityAuditSkill._version_le("1.0.0", "2.0.0") is True

    def test_equal(self):
        assert SecurityAuditSkill._version_le("2.0.0", "2.0.0") is True

    def test_greater(self):
        assert SecurityAuditSkill._version_le("3.0.0", "2.0.0") is False

    def test_patch(self):
        assert SecurityAuditSkill._version_le("2.0.0", "2.0.1") is True


class TestSecurityAuditExecute:
    @pytest.mark.asyncio
    async def test_execute_config(self):
        skill = SecurityAuditSkill()
        result = await skill.execute({
            "target": "password = 'admin123'\nhttp://example.com/api\n",
            "audit_type": "config",
            "framework": "owasp",
            "quick_scan_results": [],
        })
        assert result["audit_type"] == "config"
        assert "risk_score" in result
        assert "risk_level" in result
        assert "findings" in result
        assert "recommendations" in result

    @pytest.mark.asyncio
    async def test_execute_clean(self):
        skill = SecurityAuditSkill()
        result = await skill.execute({
            "target": "This is a safe configuration file\n",
            "audit_type": "config",
            "framework": "owasp",
            "quick_scan_results": [],
        })
        assert result["risk_score"] == 0.0


# ============================================================
# TextSummarizationSkill
# ============================================================

class TestTextSummarizationDefinition:
    def test_get_definition(self):
        skill = TextSummarizationSkill()
        d = skill.get_definition()
        assert d.name == "text_summarization"
        assert d.category == "text-processing"


class TestTextSummarizationValidate:
    def test_valid_input(self):
        skill = TextSummarizationSkill()
        assert skill.validate_input({"text": "This is a test text."}) is True

    def test_empty_text(self):
        skill = TextSummarizationSkill()
        assert skill.validate_input({"text": ""}) is False

    def test_invalid_mode(self):
        skill = TextSummarizationSkill()
        assert skill.validate_input({"text": "x", "mode": "invalid"}) is False


class TestTextSummarizationDetectLanguage:
    def test_detect_chinese(self):
        skill = TextSummarizationSkill()
        assert skill._detect_language("这是一个中文测试文本") == "zh"

    def test_detect_english(self):
        skill = TextSummarizationSkill()
        assert skill._detect_language("This is an English text") == "en"

    def test_empty(self):
        skill = TextSummarizationSkill()
        assert skill._detect_language("") == "en"


class TestTextSummarizationExtractSentences:
    def test_extracts_key_sentences(self):
        skill = TextSummarizationSkill()
        text = (
            "Python is a programming language. "
            "It is widely used in data science. "
            "Python has a simple syntax. "
            "Many libraries support Python. "
            "Python is great for automation. "
            "It runs on many platforms."
        )
        sentences = skill._extract_key_sentences(text, 500)
        assert len(sentences) > 0

    def test_short_text(self):
        skill = TextSummarizationSkill()
        sentences = skill._extract_key_sentences("Short text.", 500)
        assert len(sentences) == 1


class TestTextSummarizationGenerateSummary:
    def test_extractive_mode(self):
        skill = TextSummarizationSkill()
        result = skill._generate_summary(
            "First sentence. Second sentence. Third sentence.",
            "extractive", 500, "general",
            ["First sentence.", "Second sentence."],
        )
        assert "First sentence" in result

    def test_bullet_mode(self):
        skill = TextSummarizationSkill()
        result = skill._generate_summary(
            "Short text.", "bullet", 500, "general",
            ["Point 1", "Point 2"],
        )
        assert "•" in result

    def test_concise_mode(self):
        skill = TextSummarizationSkill()
        result = skill._generate_summary(
            "First sentence. Second sentence. Third sentence.",
            "concise", 500, "general", []
        )
        assert "First sentence" in result

    def test_abstractive_fallback(self):
        skill = TextSummarizationSkill()
        result = skill._generate_summary(
            "First sentence. Second sentence. Third sentence. Fourth sentence. Fifth sentence.",
            "abstractive", 200, "general", []
        )
        assert len(result) > 0


class TestTextSummarizationPreExecute:
    @pytest.mark.asyncio
    async def test_sets_defaults(self):
        skill = TextSummarizationSkill()
        ctx = await skill.pre_execute({"text": "Hello world."})
        assert ctx["mode"] == "abstractive"
        assert ctx["max_length"] == 500
        assert "original_length" in ctx


class TestTextSummarizationExecute:
    @pytest.mark.asyncio
    async def test_execute_concise(self):
        skill = TextSummarizationSkill()
        result = await skill.execute({
            "text": "Python is a programming language. It is very popular. Many people use it.",
            "mode": "concise",
            "max_length": 200,
            "focus": "general",
        })
        assert "summary" in result
        assert result["mode"] == "concise"
        assert result["original_length"] > 0
        assert result["compression_ratio"] > 0

    @pytest.mark.asyncio
    async def test_execute_extractive(self):
        skill = TextSummarizationSkill()
        result = await skill.execute({
            "text": "Python is a programming language. It is widely used. Python has a simple syntax.",
            "mode": "extractive",
            "max_length": 500,
            "focus": "technical",
        })
        assert result["summary_length"] > 0
        assert "key_points" in result


class TestTextSummarizationPostExecute:
    @pytest.mark.asyncio
    async def test_truncates_long_summary(self):
        skill = TextSummarizationSkill()
        result = {"summary": "word " * 200, "summary_length": 1000}
        ctx = {"max_length": 100}
        truncated = await skill.post_execute(ctx, result)
        # truncation adds "..." so length may be slightly over max_length
        assert len(truncated["summary"]) <= 105