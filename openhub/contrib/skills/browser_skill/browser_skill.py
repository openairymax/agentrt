# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Browser Automation Skill
======================

This module provides browser automation capabilities for the OpenHub platform,
including web scraping, form filling, screenshot capture, and page interaction.

Architecture:
- Playwright for modern browser automation
- Selenium as fallback for legacy support
- BeautifulSoup for HTML parsing
- Async support for concurrent operations

Features:
- Web page navigation and rendering
- Element selection and interaction
- Form filling and submission
- Screenshot and PDF generation
- Cookie management
- JavaScript execution
- Web scraping with CSS/XPath selectors
"""

import asyncio
import base64
import json
import logging
import os
import time
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

try:
    from playwright.sync_api import sync_playwright, Page, Browser, BrowserContext
    from playwright._impl._api_types import TimeoutError as PlaywrightTimeout
    PLAYWRIGHT_AVAILABLE = True
except ImportError:
    PLAYWRIGHT_AVAILABLE = False

try:
    from selenium import webdriver
    from selenium.webdriver.common.by import By
    from selenium.webdriver.common.keys import Keys
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.support import expected_conditions as EC
    from selenium.common.exceptions import TimeoutException as SeleniumTimeout
    SELENIUM_AVAILABLE = True
except ImportError:
    SELENIUM_AVAILABLE = False

try:
    from bs4 import BeautifulSoup
    BS4_AVAILABLE = True
except ImportError:
    BS4_AVAILABLE = False


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class SelectorType(Enum):
    """Supported element selector types."""
    CSS = "css"
    XPATH = "xpath"
    ID = "id"
    NAME = "name"
    CLASS = "class"
    TEXT = "text"
    LINK_TEXT = "link_text"
    PARTIAL_LINK_TEXT = "partial_link_text"


class BrowserType(Enum):
    """Supported browser types."""
    CHROMIUM = "chromium"
    FIREFOX = "firefox"
    WEBKIT = "webkit"


class ActionType(Enum):
    """Browser action types."""
    CLICK = "click"
    DOUBLE_CLICK = "double_click"
    RIGHT_CLICK = "right_click"
    HOVER = "hover"
    FILL = "fill"
    SELECT = "select"
    CHECK = "check"
    UNCHECK = "uncheck"
    PRESS = "press"
    SCROLL = "scroll"
    DRAG = "drag"


@dataclass
class BrowserConfig:
    """Configuration for browser automation."""
    browser_type: BrowserType = BrowserType.CHROMIUM
    headless: bool = True
    timeout: int = 30000
    user_agent: Optional[str] = None
    viewport_width: int = 1920
    viewport_height: int = 1080
    proxy: Optional[str] = None
    downloads_path: Optional[str] = None
    ignore_https_errors: bool = False
    slow_mo: int = 0


@dataclass
class ElementInfo:
    """Information about a page element."""
    tag: str
    text: str
    html: str
    attributes: Dict[str, str]
    bounding_box: Optional[Dict[str, float]]
    is_visible: bool
    is_enabled: bool
    is_checked: Optional[bool] = None


@dataclass
class PageMetrics:
    """Page performance metrics."""
    title: str
    url: str
    load_time: float
    dom_content_loaded: float
    script_duration: float
    layout_duration: float
    resource_count: int
    response_size: int


@dataclass
class CookieInfo:
    """Cookie information."""
    name: str
    value: str
    domain: Optional[str]
    path: Optional[str]
    expires: Optional[float]
    http_only: bool
    secure: bool
    same_site: Optional[str]


@dataclass
class BrowserResult:
    """Result of a browser operation."""
    success: bool
    data: Any = None
    error: Optional[str] = None
    duration: float = 0.0
    page_source: Optional[str] = None
    screenshot: Optional[str] = None


class BrowserSkill:
    """Main browser automation skill class."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the browser skill with configuration."""
        self.config = self._parse_config(config or {})
        self.browser = None
        self.context = None
        self.page = None
        self._playwright = None
        self._driver = None
        self._use_selenium = False
        self._start_time = None

    def _parse_config(self, config: Dict[str, Any]) -> BrowserConfig:
        """Parse configuration dictionary."""
        viewport = config.get("viewport", {})
        return BrowserConfig(
            browser_type=BrowserType(config.get("browser_type", "chromium")),
            headless=config.get("headless", True),
            timeout=config.get("timeout", 30000),
            user_agent=config.get("user_agent"),
            viewport_width=viewport.get("width", 1920),
            viewport_height=viewport.get("height", 1080),
            proxy=config.get("proxy"),
            downloads_path=config.get("downloads_path"),
            ignore_https_errors=config.get("ignore_https_errors", False),
            slow_mo=config.get("slow_mo", 0)
        )

    def initialize(self) -> BrowserResult:
        """Initialize the browser."""
        self._start_time = time.time()
        try:
            if PLAYWRIGHT_AVAILABLE:
                return self._initialize_playwright()
            elif SELENIUM_AVAILABLE:
                return self._initialize_selenium()
            else:
                return BrowserResult(
                    success=False,
                    error="No browser automation library available. Install playwright or selenium."
                )
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def _initialize_playwright(self) -> BrowserResult:
        """Initialize Playwright browser."""
        try:
            self._playwright = sync_playwright().start()
            browser_name = self.config.browser_type.value

            launch_options = {
                "headless": self.config.headless,
                "slow_mo": self.config.slow_mo
            }

            if self.config.proxy:
                launch_options["proxy"] = {"server": self.config.proxy}

            # 根据浏览器类型选择正确的启动方法
            browser_launcher = getattr(self._playwright, browser_name)
            self.browser = browser_launcher.launch(**launch_options)

            context_options = {
                "viewport": {"width": self.config.viewport_width, "height": self.config.viewport_height},
                "ignore_https_errors": self.config.ignore_https_errors
            }

            if self.config.user_agent:
                context_options["user_agent"] = self.config.user_agent

            if self.config.downloads_path:
                context_options["accept_downloads"] = True
                context_options["downloads_path"] = self.config.downloads_path

            self.context = self.browser.new_context(**context_options)
            self.page = self.context.new_page()

            logger.info(f"Playwright browser initialized: {self.config.browser_type.value}")
            return BrowserResult(success=True)

        except Exception as e:
            logger.error(f"Playwright initialization failed: {e}")
            return BrowserResult(success=False, error=str(e))

    def _initialize_selenium(self) -> BrowserResult:
        """Initialize Selenium WebDriver."""
        try:
            options_map = {
                BrowserType.CHROMIUM: "chromium",
                BrowserType.FIREFOX: "firefox",
                BrowserType.WEBKIT: "webkit"
            }

            browser_name = options_map.get(self.config.browser_type, "chromium")

            if browser_name == "chromium":
                from selenium.webdriver.chrome.options import Options
                options = Options()
                if self.config.headless:
                    options.add_argument("--headless")
                if self.config.user_agent:
                    options.add_argument(f"user-agent={self.config.user_agent}")
                if self.config.proxy:
                    options.add_argument(f"--proxy-server={self.config.proxy}")
                options.add_argument(f"--window-size={self.config.viewport_width},{self.config.viewport_height}")
                self._driver = webdriver.Chrome(options=options)

            elif browser_name == "firefox":
                from selenium.webdriver.firefox.options import Options
                options = Options()
                if self.config.headless:
                    options.add_argument("--headless")
                self._driver = webdriver.Firefox(options=options)

            self._driver.set_page_load_timeout(self.config.timeout / 1000)
            self._use_selenium = True

            logger.info(f"Selenium WebDriver initialized: {browser_name}")
            return BrowserResult(success=True)

        except Exception as e:
            logger.error(f"Selenium initialization failed: {e}")
            return BrowserResult(success=False, error=str(e))

    def navigate(self, url: str, wait_until: str = "load") -> BrowserResult:
        """Navigate to a URL."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                self._driver.get(url)
                return BrowserResult(success=True, data={"url": url})
            else:
                wait_map = {
                    "load": "load",
                    "domcontentloaded": "domcontentloaded",
                    "networkidle": "networkidle",
                    "commit": "commit"
                }
                self.page.goto(url, wait_until=wait_map.get(wait_until, "load"))
                return BrowserResult(
                    success=True,
                    data={"url": self.page.url, "title": self.page.title()}
                )
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    async def navigate_async(self, url: str, wait_until: str = "load") -> BrowserResult:
        """Async version of navigate."""
        if not PLAYWRIGHT_AVAILABLE:
            return self.navigate(url, wait_until)

        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, self.navigate, url, wait_until)

    def screenshot(self, path: Optional[str] = None, full_page: bool = False, encoding: str = "binary") -> BrowserResult:
        """Capture a screenshot of the current page."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                img = self._driver.get_screenshot_as_png()
                if encoding == "base64":
                    img = base64.b64encode(img).decode()
                return BrowserResult(success=True, data=img)
            else:
                if path:
                    self.page.screenshot(path=path, full_page=full_page)
                    return BrowserResult(success=True, data=path)
                else:
                    img = self.page.screenshot(full_page=full_page)
                    if encoding == "base64":
                        img = base64.b64encode(img).decode()
                    return BrowserResult(success=True, data=img)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def get_html(self, pretty: bool = False) -> BrowserResult:
        """Get the current page HTML content."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                html = self._driver.page_source
            else:
                html = self.page.content()

            if pretty and BS4_AVAILABLE:
                soup = BeautifulSoup(html, "lxml")
                html = soup.prettify()

            return BrowserResult(success=True, data=html)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def execute_script(self, script: str) -> BrowserResult:
        """Execute JavaScript code in the page context."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                result = self._driver.execute_script(script)
            else:
                result = self.page.evaluate(script)
            return BrowserResult(success=True, data=result)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def find_element(self, selector: str, selector_type: SelectorType = SelectorType.CSS) -> BrowserResult:
        """Find a single element on the page."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            element = self._find_element_impl(selector, selector_type)
            if element is None:
                return BrowserResult(success=False, error=f"Element not found: {selector}")

            info = self._get_element_info(element)
            return BrowserResult(success=True, data=info)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def find_elements(self, selector: str, selector_type: SelectorType = SelectorType.CSS) -> BrowserResult:
        """Find multiple elements on the page."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            elements = self._find_elements_impl(selector, selector_type)
            info_list = [self._get_element_info(el) for el in elements]
            return BrowserResult(success=True, data=info_list)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def _find_element_impl(self, selector: str, selector_type: SelectorType):
        """Implementation of element finding."""
        if self._use_selenium:
            by_map = {
                SelectorType.CSS: By.CSS_SELECTOR,
                SelectorType.XPATH: By.XPATH,
                SelectorType.ID: By.ID,
                SelectorType.NAME: By.NAME,
                SelectorType.CLASS: By.CLASS_NAME,
                SelectorType.LINK_TEXT: By.LINK_TEXT,
                SelectorType.PARTIAL_LINK_TEXT: By.PARTIAL_LINK_TEXT,
            }
            by = by_map.get(selector_type, By.CSS_SELECTOR)
            return self._driver.find_element(by, selector)
        else:
            if selector_type == SelectorType.CSS:
                return self.page.query_selector(selector)
            elif selector_type == SelectorType.XPATH:
                return self.page.query_selector(f"xpath={selector}")
            elif selector_type == SelectorType.TEXT:
                return self.page.get_by_text(selector)
            elif selector_type == SelectorType.LINK_TEXT:
                return self.page.get_by_role("link", name=selector)
            return None

    def _find_elements_impl(self, selector: str, selector_type: SelectorType):
        """Implementation of multi-element finding."""
        if self._use_selenium:
            by_map = {
                SelectorType.CSS: By.CSS_SELECTOR,
                SelectorType.XPATH: By.XPATH,
                SelectorType.ID: By.ID,
                SelectorType.NAME: By.NAME,
                SelectorType.CLASS: By.CLASS_NAME,
            }
            by = by_map.get(selector_type, By.CSS_SELECTOR)
            return self._driver.find_elements(by, selector)
        else:
            if selector_type == SelectorType.CSS:
                return self.page.query_selector_all(selector)
            elif selector_type == SelectorType.XPATH:
                return self.page.query_selector_all(f"xpath={selector}")
            elif selector_type == SelectorType.TEXT:
                return self.page.get_by_text(selector).all()
            return []

    def _get_element_info(self, element) -> ElementInfo:
        """Extract information from a page element."""
        if self._use_selenium:
            return ElementInfo(
                tag=element.tag_name,
                text=element.text,
                html=element.get_attribute("outerHTML") or "",
                attributes={attr: element.get_attribute(attr) for attr in element.get_property("attributes")},
                bounding_box=None,
                is_visible=element.is_displayed(),
                is_enabled=element.is_enabled(),
                is_checked=element.is_selected() if element.tag_name in ["input", "checkbox", "radio"] else None
            )
        else:
            bounding = element.bounding_box()
            return ElementInfo(
                tag=element.evaluate("el => el.tagName"),
                text=element.inner_text(),
                html=element.inner_html(),
                attributes=element.evaluate("el => Object.fromEntries(Array.from(el.attributes).map(a => [a.name, a.value]))"),
                bounding_box=bounding,
                is_visible=element.is_visible(),
                is_enabled=element.is_enabled(),
                is_checked=element.is_checked() if element.tag_name in ["input", "checkbox", "radio"] else None
            )

    def click(self, selector: str, selector_type: SelectorType = SelectorType.CSS, modifier_keys: Optional[List[str]] = None) -> BrowserResult:
        """Click an element."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            element = self._find_element_impl(selector, selector_type)
            if element is None:
                return BrowserResult(success=False, error=f"Element not found: {selector}")

            if self._use_selenium:
                element.click()
            else:
                modifiers = modifier_keys or []
                element.click(modifiers=modifiers)

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def fill(self, selector: str, value: str, selector_type: SelectorType = SelectorType.CSS, press_enter: bool = False) -> BrowserResult:
        """Fill an input field."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            element = self._find_element_impl(selector, selector_type)
            if element is None:
                return BrowserResult(success=False, error=f"Element not found: {selector}")

            if self._use_selenium:
                element.clear()
                element.send_keys(value)
                if press_enter:
                    element.send_keys(Keys.ENTER)
            else:
                element.fill(value)
                if press_enter:
                    element.press("Enter")

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def select_option(self, selector: str, value: Optional[str] = None, label: Optional[str] = None, selector_type: SelectorType = SelectorType.CSS) -> BrowserResult:
        """Select an option from a dropdown."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            from selenium.webdriver.support.ui import Select as SeleniumSelect

            if self._use_selenium:
                element = self._find_element_impl(selector, selector_type)
                if element is None:
                    return BrowserResult(success=False, error=f"Element not found: {selector}")

                select = SeleniumSelect(element)
                if value:
                    select.select_by_value(value)
                elif label:
                    select.select_by_visible_text(label)
            else:
                options = {"value": value} if value else {"label": label}
                self.page.select_option(selector, **options)

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def hover(self, selector: str, selector_type: SelectorType = SelectorType.CSS) -> BrowserResult:
        """Hover over an element."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                from selenium.webdriver.common.action_chains import ActionChains
                element = self._find_element_impl(selector, selector_type)
                if element is None:
                    return BrowserResult(success=False, error=f"Element not found: {selector}")
                ActionChains(self._driver).move_to_element(element).perform()
            else:
                self.page.hover(selector)

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def scroll(self, x: int = 0, y: int = 0, selector: Optional[str] = None, selector_type: SelectorType = SelectorType.CSS) -> BrowserResult:
        """Scroll the page or to a specific element."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if selector:
                element = self._find_element_impl(selector, selector_type)
                if element is None:
                    return BrowserResult(success=False, error=f"Element not found: {selector}")

                if self._use_selenium:
                    self._driver.execute_script("arguments[0].scrollIntoView(true);", element)
                else:
                    element.scroll_into_view_if_needed()
            else:
                if self._use_selenium:
                    self._driver.execute_script(f"window.scrollTo({x}, {y})")
                else:
                    self.page.evaluate(f"window.scrollTo({x}, {y})")

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def wait_for_selector(self, selector: str, selector_type: SelectorType = SelectorType.CSS, timeout: Optional[int] = None, state: str = "visible") -> BrowserResult:
        """Wait for an element to appear or have a specific state."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        timeout_ms = timeout or self.config.timeout

        try:
            if self._use_selenium:
                by_map = {
                    SelectorType.CSS: By.CSS_SELECTOR,
                    SelectorType.XPATH: By.XPATH,
                    SelectorType.ID: By.ID,
                    SelectorType.NAME: By.NAME,
                }
                by = by_map.get(selector_type, By.CSS_SELECTOR)

                state_map = {
                    "visible": EC.visibility_of_element_located,
                    "present": EC.presence_of_element_located,
                    "clickable": EC.element_to_be_clickable,
                }
                condition = state_map.get(state, EC.visibility_of_element_located)
                element = WebDriverWait(self._driver, timeout_ms / 1000).until(condition((by, selector)))
            else:
                state_map = {
                    "visible": "visible",
                    "hidden": "hidden",
                    "attached": "attached",
                    "detached": "detached",
                }
                self.page.wait_for_selector(selector, state=state_map.get(state, "visible"), timeout=timeout_ms)

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def wait_for_navigation(self, wait_until: str = "load", timeout: Optional[int] = None) -> BrowserResult:
        """Wait for navigation to complete."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        timeout_ms = timeout or self.config.timeout

        try:
            if self._use_selenium:
                from selenium.webdriver.support.ui import WebDriverWait
                WebDriverWait(self._driver, timeout_ms / 1000).until(
                    lambda d: d.execute_script("return document.readyState") == "complete"
                )
            else:
                self.page.wait_for_load_state(wait_until, timeout=timeout_ms)

            return BrowserResult(success=True, data={"url": self._driver.current_url if self._use_selenium else self.page.url})
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def get_cookies(self) -> BrowserResult:
        """Get all cookies from the current context."""
        if not self.context and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                cookies = self._driver.get_cookies()
            else:
                cookies = self.context.cookies()

            cookie_list = [
                CookieInfo(
                    name=c.get("name", c.name) if isinstance(c, dict) else c.name,
                    value=c.get("value", c.value) if isinstance(c, dict) else c.value,
                    domain=c.get("domain", c.domain) if isinstance(c, dict) else getattr(c, "domain", None),
                    path=c.get("path", c.path) if isinstance(c, dict) else getattr(c, "path", None),
                    expires=c.get("expiry", c.expires) if isinstance(c, dict) else getattr(c, "expires", None),
                    http_only=c.get("httpOnly", c.http_only) if isinstance(c, dict) else getattr(c, "http_only", False),
                    secure=c.get("secure", c.secure) if isinstance(c, dict) else getattr(c, "secure", False),
                    same_site=c.get("sameSite", c.same_site) if isinstance(c, dict) else getattr(c, "same_site", None)
                )
                for c in cookies
            ]

            return BrowserResult(success=True, data=cookie_list)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def set_cookies(self, cookies: List[Dict[str, Any]]) -> BrowserResult:
        """Set cookies in the current context."""
        if not self.context and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            for cookie in cookies:
                if self._use_selenium:
                    self._driver.add_cookie(cookie)
                else:
                    self.context.add_cookies([cookie])

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def delete_cookies(self, names: Optional[List[str]] = None) -> BrowserResult:
        """Delete cookies from the current context."""
        if not self.context and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if names:
                for name in names:
                    if self._use_selenium:
                        self._driver.delete_cookie(name)
                    else:
                        self.context.delete_cookies(name)
            else:
                if self._use_selenium:
                    self._driver.delete_all_cookies()
                else:
                    self.context.clear_cookies()

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def get_local_storage(self) -> BrowserResult:
        """Get local storage data."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                data = self._driver.execute_script("return JSON.stringify(localStorage);")
                storage = json.loads(data) if data else {}
            else:
                storage = self.page.evaluate("() => JSON.stringify(localStorage)")
                storage = json.loads(storage) if storage else {}

            return BrowserResult(success=True, data=storage)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def set_local_storage(self, data: Dict[str, str]) -> BrowserResult:
        """Set local storage data."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                for key, value in data.items():
                    self._driver.execute_script(f"localStorage.setItem('{key}', '{value}');")
            else:
                for key, value in data.items():
                    self.page.evaluate(f"() => localStorage.setItem('{key}', '{value}')")

            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def clear_local_storage(self) -> BrowserResult:
        """Clear local storage."""
        return self.set_local_storage({})

    def download(self, url: str, save_path: str, timeout: Optional[int] = None) -> BrowserResult:
        """Download a file from a URL."""
        try:
            import requests
            timeout_val = timeout / 1000 if timeout else 30

            response = requests.get(url, timeout=timeout_val, stream=True)
            response.raise_for_status()

            with open(save_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)

            return BrowserResult(success=True, data={"path": save_path, "size": os.path.getsize(save_path)})
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def get_metrics(self) -> BrowserResult:
        """Get page performance metrics."""
        if not self.page and not self._driver:
            return BrowserResult(success=False, error="Browser not initialized")

        try:
            if self._use_selenium:
                timing = self._driver.execute_script("""
                    var perf = window.performance;
                    if (perf && perf.timing) {
                        return {
                            navigationStart: perf.timing.navigationStart,
                            loadEventEnd: perf.timing.loadEventEnd,
                            domContentLoadedEventEnd: perf.timing.domContentLoadedEventEnd,
                            responseEnd: perf.timing.responseEnd
                        };
                    }
                    return null;
                """)

                if timing:
                    load_time = (timing.get("loadEventEnd", 0) - timing.get("navigationStart", 0)) / 1000
                    dom_content = (timing.get("domContentLoadedEventEnd", 0) - timing.get("navigationStart", 0)) / 1000
                else:
                    load_time = dom_content = 0

                metrics = PageMetrics(
                    title=self._driver.title,
                    url=self._driver.current_url,
                    load_time=load_time,
                    dom_content_loaded=dom_content,
                    script_duration=0,
                    layout_duration=0,
                    resource_count=0,
                    response_size=0
                )
            else:
                metrics = self.page.evaluate("""() => {
                    const perf = window.performance;
                    const timing = perf.timing;
                    const resources = perf.getEntriesByType('resource');
                    return {
                        navigationStart: timing.navigationStart,
                        loadEventEnd: timing.loadEventEnd,
                        domContentLoadedEventEnd: timing.domContentLoadedEventEnd,
                        responseEnd: timing.responseEnd,
                        resources: resources.length,
                        responseSize: resources.reduce((acc, r) => acc + r.transferSize, 0)
                    };
                }""")

                load_time = (metrics["loadEventEnd"] - metrics["navigationStart"]) / 1000
                dom_content = (metrics["domContentLoadedEventEnd"] - metrics["navigationStart"]) / 1000

                page_metrics = PageMetrics(
                    title=self.page.title(),
                    url=self.page.url,
                    load_time=load_time,
                    dom_content_loaded=dom_content,
                    script_duration=0,
                    layout_duration=0,
                    resource_count=metrics.get("resources", 0),
                    response_size=metrics.get("responseSize", 0)
                )
                metrics = page_metrics

            return BrowserResult(success=True, data=metrics)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def close(self) -> BrowserResult:
        """Close the browser and cleanup resources."""
        try:
            if self._driver:
                self._driver.quit()
                self._driver = None

            if self.browser:
                self.browser.close()
                self.browser = None

            if self._playwright:
                self._playwright.stop()
                self._playwright = None

            self.context = None
            self.page = None

            logger.info("Browser closed successfully")
            return BrowserResult(success=True)
        except Exception as e:
            return BrowserResult(success=False, error=str(e))

    def __enter__(self):
        """Context manager entry."""
        self.initialize()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()


def create_skill(config: Optional[Dict[str, Any]] = None) -> BrowserSkill:
    """Factory function to create a BrowserSkill instance."""
    return BrowserSkill(config)
