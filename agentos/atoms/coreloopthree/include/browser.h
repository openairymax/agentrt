/**
 * @file browser.h
 * @brief CoreLoopThree 浏览器执行单元 — 公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供浏览器自动化操作的独立API，包括：
 * - fill_form(): 使用CDP DOM.setAttributeValue 精确填充表单
 * - wait_for_element(): 使用CSS选择器或Page.loadEventFired 等待
 *
 * 所有函数同时支持真实CDP连接和模拟模式。
 */

#ifndef AGENTOS_CORELOOPTHREE_BROWSER_H
#define AGENTOS_CORELOOPTHREE_BROWSER_H

#include "agentos.h"
#include "agentos_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 表单填充 ==================== */

/**
 * @brief 使用 CDP DOM.setAttributeValue 精确填充表单字段
 *
 * 实现策略:
 * 1. 真实 CDP 模式:
 *    a. DOM.querySelector → 获取 nodeId
 *    b. DOM.setAttributeValue(nodeId, "value", value) → 设置值
 *    c. DOM.dispatchEvent(Input, "input") → 触发 React/Vue 状态更新
 * 2. 模拟模式: 返回成功JSON但不执行实际操作
 *
 * @param conn [in] 浏览器连接句柄
 * @param selector [in] CSS选择器 (如 "#username", "input[name='email']")
 * @param selector_len [in] 选择器长度
 * @param value [in] 要填充的值 (空字符串触发 DOM.setAttributeValue 不带值以清空)
 * @param value_len [in] 值长度
 * @param out_output [out] JSON格式的执行结果
 * @param out_output_len [out] 结果长度
 * @return agentos_error_t
 *
 * @ownership out_output 由调用者负责释放
 * @threadsafe 否
 */
AGENTOS_API agentos_error_t agentos_browser_fill_form(
    void* conn,
    const char* selector, size_t selector_len,
    const char* value, size_t value_len,
    char** out_output, size_t* out_output_len);

/* ==================== 元素等待 ==================== */

/**
 * @brief 等待页面元素出现后再继续
 *
 * 实现策略 (按优先级降级):
 * 1. 真实 CDP + CSS选择器:
 *    - 注入 JS Promise 轮询 document.querySelector(selector)
 *    - 轮询间隔 100ms，最大超时 timeout_ms
 *    - 找到元素返回 "found"，超时返回 "timeout"
 * 2. 真实 CDP + 页面加载:
 *    - Page.enable → 轮询 Page.loadEventFired 事件
 * 3. 真实 CDP + 网络空闲:
 *    - 注入 JS 监控 XHR 请求完成 + document.readyState === 'complete'
 * 4. 真实 CDP + 纯延迟:
 *    - setTimeout Promise with awaitPromise
 * 5. 模拟模式: 直接返回成功
 *
 * @param conn [in] 浏览器连接句柄
 * @param selector [in] CSS选择器 (NULL表示不等待特定元素)
 * @param selector_len [in] 选择器长度
 * @param wait_type [in] 等待类型: "element"(默认)/"load"/"network"/"delay"
 * @param timeout_ms [in] 超时毫秒数 (0 = 使用默认5000ms)
 * @param out_output [out] JSON格式的执行结果
 * @param out_output_len [out] 结果长度
 * @return agentos_error_t
 *
 * @ownership out_output 由调用者负责释放
 * @threadsafe 否
 */
AGENTOS_API agentos_error_t agentos_browser_wait_for_element(
    void* conn,
    const char* selector, size_t selector_len,
    const char* wait_type,
    uint32_t timeout_ms,
    char** out_output, size_t* out_output_len);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORELOOPTHREE_BROWSER_H */
