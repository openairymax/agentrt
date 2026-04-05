// AgentOS TypeScript SDK - Request/Response Types
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 定义 SDK 中使用的所有请求/响应结构。
// 与 Go SDK types/types.go 请求/响应部分保持一致。

/**
 * 请求选项接口
 */
export interface RequestOptions {
  /** 单次请求超时（毫秒） */
  timeout?: number;
  /** 自定义请求头 */
  headers?: Record<string, string>;
  /** 查询参数 */
  queryParams?: Record<string, string>;
  /** 取消信号（用于取消请求） */
  signal?: AbortSignal;
}

/**
 * 请求选项函数签名
 */
export type RequestOption = (options: RequestOptions) => void;

/**
 * 设置单次请求超时
 * @param timeout - 超时时间（毫秒）
 */
export function withRequestTimeout(timeout: number): RequestOption {
  return (options: RequestOptions) => {
    if (timeout > 0) {
      options.timeout = timeout;
    }
  };
}

/**
 * 添加自定义请求头
 * @param key - 请求头名称
 * @param value - 请求头值
 */
export function withHeader(key: string, value: string): RequestOption {
  return (options: RequestOptions) => {
    if (!options.headers) {
      options.headers = {};
    }
    options.headers[key] = value;
  };
}

/**
 * 添加查询参数
 * @param key - 参数名称
 * @param value - 参数值
 */
export function withQueryParam(key: string, value: string): RequestOption {
  return (options: RequestOptions) => {
    if (!options.queryParams) {
      options.queryParams = {};
    }
    options.queryParams[key] = value;
  };
}

/**
 * 添加取消信号
 * @param signal - AbortSignal 用于取消请求
 */
export function withSignal(signal: AbortSignal): RequestOption {
  return (options: RequestOptions) => {
    options.signal = signal;
  };
}

/**
 * 通用 API 响应结构
 */
export interface APIResponse<T = unknown> {
  /** 是否成功 */
  success: boolean;
  /** 响应数据 */
  data?: T;
  /** 消息 */
  message?: string;
}

/**
 * 分页选项
 */
export interface PaginationOptions {
  /** 页码 */
  page: number;
  /** 每页数量 */
  pageSize: number;
}

/**
 * 返回默认分页选项
 */
export function defaultPaginationOptions(): PaginationOptions {
  return { page: 1, pageSize: 20 };
}

/**
 * 将分页参数转换为查询参数
 * @param options - 分页选项
 */
export function buildQueryParams(options: PaginationOptions): Record<string, string> {
  const params: Record<string, string> = {};
  if (options.page > 0) {
    params['page'] = String(options.page);
  }
  if (options.pageSize > 0) {
    params['page_size'] = String(options.pageSize);
  }
  return params;
}

/**
 * 排序选项
 */
export interface SortOptions {
  /** 排序字段 */
  field: string;
  /** 排序方向 (asc/desc) */
  order: 'asc' | 'desc';
}

/**
 * 过滤选项
 */
export interface FilterOptions {
  /** 过滤字段 */
  key: string;
  /** 过滤值 */
  value: unknown;
}

/**
 * 列表查询的复合选项
 */
export interface ListOptions {
  /** 分页选项 */
  pagination?: PaginationOptions;
  /** 排序选项 */
  sort?: SortOptions;
  /** 过滤选项 */
  filter?: FilterOptions;
}

/**
 * 将列表选项转换为查询参数
 * @param options - 列表选项
 */
export function toQueryParams(options: ListOptions): Record<string, string> {
  const params: Record<string, string> = {};

  if (options.pagination) {
    Object.assign(params, buildQueryParams(options.pagination));
  }

  if (options.sort) {
    if (options.sort.field) {
      params['sort_by'] = options.sort.field;
    }
    if (options.sort.order) {
      params['sort_order'] = options.sort.order;
    }
  }

  if (options.filter) {
    if (options.filter.key) {
      params['filter_key'] = options.filter.key;
      params['filter_value'] = String(options.filter.value);
    }
  }

  return params;
}
