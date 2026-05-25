# Browser Skill — 浏览器技能

> **Preview Status**: 本模块当前处于预览/开发阶段，作为 AgentOS v0.0.5 的一部分发布。API 和功能可能在未来版本中发生变化。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

`openlab/contrib/skills/browser_skill/` 提供智能体的浏览器自动化能力，支持网页导航、数据抓取和表单操作。

## 核心能力

- **网页导航**：URL 跳转、标签页管理、历史记录
- **数据抓取**：页面元素提取、表格数据抓取、截图
- **表单操作**：自动填写表单、提交、文件上传
- **页面交互**：点击、滚动、悬停、键盘输入

## 支持操作

| 操作 | 说明 |
|------|------|
| `browser.navigate` | 导航到指定 URL |
| `browser.click` | 点击页面元素 |
| `browser.type` | 输入文本到表单 |
| `browser.screenshot` | 页面截图 |
| `browser.extract` | 提取页面数据 |
| `browser.evaluate` | 执行 JavaScript |

## 使用方式

```python
from contrib.skills.browser_skill import BrowserSkill

browser = BrowserSkill(headless=True)

# 打开网页
await browser.navigate("https://example.com")

# 提取数据
data = await browser.extract(
    selector=".article-content",
    attribute="text"
)

# 截图
await browser.screenshot("output/page.png")

# 关闭浏览器
await browser.close()
```

---

*AgentOS OpenLab — Browser Skill*
