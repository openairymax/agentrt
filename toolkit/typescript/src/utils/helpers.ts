// AgentOS TypeScript SDK - Helper Functions
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供类型安全的 map 数据提取、API 响应解析和 URL 构建等通用工具函数。
// 与 Go SDK utils/helpers.go 保持一致。

import { APIResponse, ListOptions } from '../types';
import { AgentOSError, ErrorCode } from '../errors';

// ============================================================
// Map 类型安全提取函数
// ============================================================

/**
 * 从 map 中安全提取字符串值
 * @param m - 数据 map
 * @param key - 键名
 */
export function getString(m: Record<string, unknown>, key: string): string {
  const v = m[key];
  if (v !== undefined && typeof v === 'string') {
    return v;
  }
  return '';
}

/**
 * 从 map 中安全提取 number 值
 * @param m - 数据 map
 * @param key - 键名
 */
export function getInt64(m: Record<string, unknown>, key: string): number {
  const v = m[key];
  if (v !== undefined) {
    if (typeof v === 'number') {
      return Math.floor(v);
    }
    if (typeof v === 'string') {
      const num = parseInt(v, 10);
      if (!isNaN(num)) {
        return num;
      }
    }
  }
  return 0;
}

/**
 * 从 map 中安全提取 float 值
 * @param m - 数据 map
 * @param key - 键名
 */
export function getFloat64(m: Record<string, unknown>, key: string): number {
  const v = m[key];
  if (v !== undefined) {
    if (typeof v === 'number') {
      return v;
    }
    if (typeof v === 'string') {
      const num = parseFloat(v);
      if (!isNaN(num)) {
        return num;
      }
    }
  }
  return 0;
}

/**
 * 从 map 中安全提取 boolean 值
 * @param m - 数据 map
 * @param key - 键名
 */
export function getBool(m: Record<string, unknown>, key: string): boolean {
  const v = m[key];
  if (v !== undefined && typeof v === 'boolean') {
    return v;
  }
  return false;
}

/**
 * 从 map 中安全提取嵌套 map
 * @param m - 数据 map
 * @param key - 键名
 */
export function getMap(
  m: Record<string, unknown>,
  key: string,
): Record<string, unknown> | undefined {
  const v = m[key];
  if (v !== undefined && typeof v === 'object' && v !== null && !Array.isArray(v)) {
    return v as Record<string, unknown>;
  }
  return undefined;
}

/**
 * 从 map 中安全提取 string→string map
 * @param m - 数据 map
 * @param key - 键名
 */
export function getStringMap(
  m: Record<string, unknown>,
  key: string,
): Record<string, string> {
  const result: Record<string, string> = {};
  const v = m[key];
  if (v !== undefined && typeof v === 'object' && v !== null && !Array.isArray(v)) {
    const nested = v as Record<string, unknown>;
    for (const [k, val] of Object.entries(nested)) {
      if (typeof val === 'string') {
        result[k] = val;
      }
    }
  }
  return result;
}

/**
 * 从 map 中安全提取数组
 * @param m - 数据 map
 * @param key - 键名
 */
export function getInterfaceSlice(
  m: Record<string, unknown>,
  key: string,
): unknown[] {
  const v = m[key];
  if (v !== undefined && Array.isArray(v)) {
    return v;
  }
  return [];
}

// ============================================================
// API 响应解析函数
// ============================================================

/**
 * 从 APIResponse 中提取 Data 字段为 map
 * @param resp - API 响应
 */
export function extractDataMap(
  resp: APIResponse | null | undefined,
): Record<string, unknown> | null {
  if (!resp) {
    return null;
  }

  if ('success' in resp) {
    if (!resp.success || resp.data === undefined || resp.data === null) {
      return null;
    }
    if (typeof resp.data === 'object' && !Array.isArray(resp.data)) {
      return resp.data as Record<string, unknown>;
    }
    return null;
  }

  if (typeof resp === 'object' && !Array.isArray(resp)) {
    return resp as Record<string, unknown>;
  }
  return null;
}

/**
 * 拼接基础路径和查询参数，返回完整 URL
 * @param basePath - 基础路径
 * @param queryParams - 查询参数
 */
export function buildURL(
  basePath: string,
  queryParams?: Record<string, string>,
): string {
  if (!queryParams || Object.keys(queryParams).length === 0) {
    return basePath;
  }
  const queryString = buildQueryString(queryParams);
  if (basePath.includes('?')) {
    return `${basePath}&${queryString}`;
  }
  return `${basePath}?${queryString}`;
}

/**
 * 将参数 map 编码为查询字符串
 * @param params - 参数 map
 */
export function buildQueryString(params: Record<string, string>): string {
  if (!params || Object.keys(params).length === 0) {
    return '';
  }
  const urlParams = new URLSearchParams();
  for (const [k, v] of Object.entries(params)) {
    urlParams.append(k, v);
  }
  return urlParams.toString();
}

/**
 * 从 map 中安全提取并解析时间
 * @param m - 数据 map
 * @param key - 键名
 */
export function parseTimeFromMap(
  m: Record<string, unknown>,
  key: string,
): Date {
  const v = m[key];
  if (v !== undefined) {
    if (typeof v === 'string') {
      const parsed = new Date(v);
      if (!isNaN(parsed.getTime())) {
        return parsed;
      }
    }
    if (typeof v === 'number') {
      return new Date(v * 1000); // 假设是 Unix 时间戳（秒）
    }
  }
  return new Date(0); // 返回零值
}

/**
 * 解析时间字符串或时间戳
 * @param value - 时间值
 */
export function parseTime(value: string | undefined): Date {
  if (!value) {
    return new Date();
  }
  const parsed = new Date(value);
  if (!isNaN(parsed.getTime())) {
    return parsed;
  }
  return new Date();
}

/**
 * 从 map 中提取所有 number 类型的统计值
 * @param data - 数据 map
 */
export function extractInt64Stats(
  data: Record<string, unknown>,
): Record<string, number> {
  const stats: Record<string, number> = {};
  for (const [k, v] of Object.entries(data)) {
    if (typeof v === 'number') {
      stats[k] = Math.floor(v);
    } else if (typeof v === 'string') {
      const num = parseInt(v, 10);
      if (!isNaN(num)) {
        stats[k] = num;
      }
    }
  }
  return stats;
}

/**
 * 将 APIResponse.Data 解析到目标对象
 * @param resp - API 响应
 */
export function parseResponseData<T>(resp: APIResponse | null): T | null {
  if (!resp || !resp.success || resp.data === undefined) {
    return null;
  }
  return resp.data as T;
}

/**
 * 向查询参数追加分页信息
 * @param params - 查询参数
 * @param page - 页码
 * @param pageSize - 每页数量
 */
export function appendPagination(
  params: Record<string, string>,
  page: number,
  pageSize: number,
): Record<string, string> {
  const result = { ...params };
  if (page > 0) {
    result['page'] = String(page);
  }
  if (pageSize > 0) {
    result['page_size'] = String(pageSize);
  }
  return result;
}

// ============================================================
// ID/时间戳生成
// ============================================================

/**
 * 生成唯一的 AgentOS ID（时间戳+随机数）
 */
export function generateId(): string {
  const timestamp = Date.now();
  const random = Math.random().toString(16).substring(2, 10);
  return `aos_${timestamp}_${random}`;
}

/**
 * 生成当前 Unix 时间戳（秒）
 */
export function generateTimestamp(): number {
  return Math.floor(Date.now() / 1000);
}

// ============================================================
// 验证和清理
// ============================================================

/**
 * 验证字符串是否为合法 JSON
 * @param s - JSON 字符串
 */
export function validateJSON(s: string): boolean {
  try {
    JSON.parse(s);
    return true;
  } catch {
    return false;
  }
}

/**
 * 清理字符串中的危险字符
 * @param s - 原始字符串
 */
export function sanitizeString(s: string): string {
  return s
    .trim()
    .replace(/\x00/g, '')
    .replace(/\r\n/g, '\n');
}

/**
 * 深拷贝对象
 * @param obj - 原始对象
 */
export function deepClone<T>(obj: T): T {
  if (obj === null || typeof obj !== 'object') {
    return obj;
  }
  return JSON.parse(JSON.stringify(obj));
}

/**
 * 检查对象是否为空
 * @param obj - 对象
 */
export function isEmpty(obj: Record<string, unknown> | unknown[] | null | undefined): boolean {
  if (obj === null || obj === undefined) {
    return true;
  }
  if (Array.isArray(obj)) {
    return obj.length === 0;
  }
  if (typeof obj === 'object') {
    return Object.keys(obj).length === 0;
  }
  return false;
}

// ============================================================
// 通用列表解析函数
// ============================================================

/**
 * 将 ListOptions 转换为查询参数
 * @param opts - 列表选项
 */
export function listOptionsToParams(opts?: ListOptions): Record<string, string> {
  const params: Record<string, string> = {};
  if (!opts) {
    return params;
  }

  if (opts.pagination) {
    if (opts.pagination.page > 0) {
      params['page'] = String(opts.pagination.page);
    }
    if (opts.pagination.pageSize > 0) {
      params['page_size'] = String(opts.pagination.pageSize);
    }
  }
  if (opts.sort?.field) {
    params['sort_by'] = opts.sort.field;
  }
  if (opts.sort?.order) {
    params['sort_order'] = opts.sort.order;
  }
  if (opts.filter?.key) {
    params['filter_key'] = opts.filter.key;
    params['filter_value'] = String(opts.filter.value);
  }

  return params;
}

/**
 * 泛型列表解析函数
 * @param resp - API 响应
 * @param listKey - 列表字段名（如 'tasks', 'memories', 'sessions', 'skills'）
 * @param parser - 单项解析函数
 * @param errorMsg - 错误消息
 */
export function parseList<T>(
  resp: APIResponse,
  listKey: string,
  parser: (item: Record<string, unknown>) => T,
  errorMsg: string = '列表响应格式异常',
): T[] {
  const data = extractDataMap(resp);
  if (!data) {
    throw new AgentOSError(errorMsg, ErrorCode.INVALID_RESPONSE);
  }

  const items = getInterfaceSlice(data, listKey);
  return items
    .filter((item): item is Record<string, unknown> => typeof item === 'object' && item !== null)
    .map(parser);
}

/**
 * 构建带查询参数的路径
 * @param basePath - 基础路径
 * @param opts - 列表选项
 */
export function buildListPath(basePath: string, opts?: ListOptions): string {
  const params = listOptionsToParams(opts);
  return buildURL(basePath, params);
}
