# Browser Skill

Browser automation skill for web scraping, testing, and interaction.

## Features

- Web page navigation and rendering
- Element selection using CSS, XPath, and other selectors
- Form filling and submission
- Screenshot and PDF generation
- Cookie and local storage management
- JavaScript execution
- Web scraping with BeautifulSoup

## Installation

```bash
pip install playwright
playwright install chromium
```

## Usage

```python
from browser_skill import BrowserSkill, BrowserType, SelectorType

# Create and initialize browser
browser = BrowserSkill({
    "browser_type": "chromium",
    "headless": True
})
result = browser.initialize()

# Navigate to a page
browser.navigate("https://example.com")

# Take a screenshot
browser.screenshot("screenshot.png")

# Find and interact with elements
browser.fill("input[name='username']", "user123")
browser.click("button[type='submit']")

# Close browser
browser.close()
```

## Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| browser_type | string | "chromium" | Browser to use: chromium, firefox, webkit |
| headless | boolean | true | Run browser in headless mode |
| timeout | integer | 30000 | Default timeout in milliseconds |
| user_agent | string | None | Custom user agent string |
| viewport | object | 1920x1080 | Browser viewport size |
| proxy | string | None | Proxy server URL |

## API Reference

### Methods

- `initialize()` - Initialize the browser
- `navigate(url)` - Navigate to a URL
- `screenshot(path)` - Capture screenshot
- `get_html()` - Get page HTML
- `execute_script(script)` - Execute JavaScript
- `find_element(selector, type)` - Find single element
- `find_elements(selector, type)` - Find multiple elements
- `click(selector)` - Click an element
- `fill(selector, value)` - Fill an input field
- `select_option(selector, value)` - Select dropdown option
- `hover(selector)` - Hover over element
- `scroll(x, y)` - Scroll page
- `wait_for_selector(selector)` - Wait for element
- `get_cookies()` - Get browser cookies
- `set_cookies(cookies)` - Set cookies
- `close()` - Close browser
