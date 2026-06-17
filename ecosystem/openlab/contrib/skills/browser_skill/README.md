# Browser Skill — 浏览器自动化技能

**模块路径**: `ecosystem/openlab/contrib/skills/browser_skill/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentRT 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentRT 核心运行时集成。

## 概述

Browser Skill 为 AgentRT 智能体提供浏览器自动化能力，支持网页导航、数据抓取、表单操作和页面交互。基于 Playwright/Selenium 等浏览器驱动框架，实现无头浏览器控制，适用于 Web 自动化测试、数据采集和页面监控等场景。

## 目录结构

```
browser_skill/
└── README.md                   # 本文件
```

> **注意**: 本技能当前为规范定义阶段，源代码尚未实现。

## 核心能力

- **网页导航**：URL 跳转、标签页管理、前进/后退/刷新、历史记录
- **数据抓取**：页面元素提取（CSS Selector/XPath）、表格数据抓取、页面截图
- **表单操作**：自动填写表单、提交、文件上传、下拉选择
- **页面交互**：点击、滚动、悬停、键盘输入、拖拽操作
- **等待策略**：显式等待、隐式等待、条件等待（元素可见/可点击/存在）
- **多页面管理**：多标签页切换、iframe 操作、弹窗处理

## 接口说明

### 操作接口

| 操作 | 说明 | 参数 |
|------|------|------|
| `browser.navigate` | 导航到指定 URL | url |
| `browser.click` | 点击页面元素 | selector |
| `browser.type` | 输入文本到表单 | selector, text |
| `browser.select` | 选择下拉选项 | selector, value |
| `browser.screenshot` | 页面截图 | path, full_page |
| `browser.extract` | 提取页面数据 | selector, attribute |
| `browser.evaluate` | 执行 JavaScript | script |
| `browser.wait_for` | 等待元素出现 | selector, timeout |
| `browser.scroll` | 滚动页面 | x, y |
| `browser.upload` | 上传文件 | selector, file_path |
| `browser.get_content` | 获取页面内容 | — |
| `browser.close` | 关闭浏览器 | — |

### 配置参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `headless` | 无头模式 | `true` |
| `browser_type` | 浏览器类型 | `chromium` |
| `viewport_width` | 视口宽度 | 1280 |
| `viewport_height` | 视口高度 | 720 |
| `default_timeout` | 默认超时（毫秒） | 30000 |
| `slow_mo` | 操作间隔（毫秒） | 0 |
| `user_agent` | 自定义 UA | 默认 UA |

## 依赖关系

- **核心依赖**: AgentRT OpenLab Core
- **浏览器驱动**: Playwright >= 1.40.0 或 Selenium >= 4.15.0
- **安装**: `pip install -e ".[browser]"`

## 使用示例

```python
from contrib.skills.browser_skill import BrowserSkill

browser = BrowserSkill(headless=True)

await browser.navigate("https://example.com")

data = await browser.extract(
    selector=".article-content",
    attribute="text"
)

await browser.type("#search-input", "AgentOS")
await browser.click("#search-button")

await browser.screenshot("output/page.png")

await browser.close()
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
