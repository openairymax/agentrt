import {
  getString,
  getInt64,
  getFloat64,
  getBool,
  getMap,
  getStringMap,
  getInterfaceSlice,
  extractDataMap,
  buildURL,
  buildQueryString,
  parseTimeFromMap,
  parseTime,
  extractInt64Stats,
  parseResponseData,
  appendPagination,
  generateId,
  generateTimestamp,
  validateJSON,
  sanitizeString,
  deepClone,
  isEmpty,
  listOptionsToParams,
  parseList,
  buildListPath,
  validateAndExtractData,
  validateRequiredString,
  validatePositiveNumber,
  validateRequiredObject,
  validateNonEmptyArray,
} from '../src/utils/helpers';
import { AgentOSError, ErrorCode } from '../src/errors';

describe('Map extraction functions', () => {
  describe('getString', () => {
    test('should return string value', () => {
      expect(getString({ name: 'test' }, 'name')).toBe('test');
    });

    test('should return empty string for missing key', () => {
      expect(getString({}, 'name')).toBe('');
    });

    test('should return empty string for non-string value', () => {
      expect(getString({ name: 123 }, 'name')).toBe('');
    });

    test('should return empty string for undefined value', () => {
      expect(getString({ name: undefined }, 'name')).toBe('');
    });
  });

  describe('getInt64', () => {
    test('should return number value', () => {
      expect(getInt64({ count: 42 }, 'count')).toBe(42);
    });

    test('should floor number value', () => {
      expect(getInt64({ count: 42.7 }, 'count')).toBe(42);
    });

    test('should parse string to number', () => {
      expect(getInt64({ count: '100' }, 'count')).toBe(100);
    });

    test('should return 0 for missing key', () => {
      expect(getInt64({}, 'count')).toBe(0);
    });

    test('should return 0 for non-numeric string', () => {
      expect(getInt64({ count: 'abc' }, 'count')).toBe(0);
    });
  });

  describe('getFloat64', () => {
    test('should return number value', () => {
      expect(getFloat64({ rate: 3.14 }, 'rate')).toBe(3.14);
    });

    test('should parse string to float', () => {
      expect(getFloat64({ rate: '2.5' }, 'rate')).toBe(2.5);
    });

    test('should return 0 for missing key', () => {
      expect(getFloat64({}, 'rate')).toBe(0);
    });
  });

  describe('getBool', () => {
    test('should return boolean value', () => {
      expect(getBool({ active: true }, 'active')).toBe(true);
      expect(getBool({ active: false }, 'active')).toBe(false);
    });

    test('should return false for missing key', () => {
      expect(getBool({}, 'active')).toBe(false);
    });

    test('should return false for non-boolean value', () => {
      expect(getBool({ active: 'true' }, 'active')).toBe(false);
    });
  });

  describe('getMap', () => {
    test('should return nested map', () => {
      const nested = { key: 'value' };
      expect(getMap({ data: nested }, 'data')).toEqual(nested);
    });

    test('should return undefined for missing key', () => {
      expect(getMap({}, 'data')).toBeUndefined();
    });

    test('should return undefined for array value', () => {
      expect(getMap({ data: [1, 2] }, 'data')).toBeUndefined();
    });

    test('should return undefined for null value', () => {
      expect(getMap({ data: null }, 'data')).toBeUndefined();
    });
  });

  describe('getStringMap', () => {
    test('should return string map', () => {
      const result = getStringMap({ data: { a: '1', b: '2' } }, 'data');
      expect(result).toEqual({ a: '1', b: '2' });
    });

    test('should filter non-string values', () => {
      const result = getStringMap({ data: { a: '1', b: 2 } }, 'data');
      expect(result).toEqual({ a: '1' });
    });

    test('should return empty object for missing key', () => {
      expect(getStringMap({}, 'data')).toEqual({});
    });
  });

  describe('getInterfaceSlice', () => {
    test('should return array', () => {
      expect(getInterfaceSlice({ items: [1, 2, 3] }, 'items')).toEqual([1, 2, 3]);
    });

    test('should return empty array for missing key', () => {
      expect(getInterfaceSlice({}, 'items')).toEqual([]);
    });

    test('should return empty array for non-array value', () => {
      expect(getInterfaceSlice({ items: 'not-array' }, 'items')).toEqual([]);
    });
  });
});

describe('API response parsing', () => {
  describe('extractDataMap', () => {
    test('should extract data from successful response', () => {
      const resp = { success: true, data: { id: '123' } };
      expect(extractDataMap(resp as any)).toEqual({ id: '123' });
    });

    test('should return null for failed response', () => {
      const resp = { success: false, data: null };
      expect(extractDataMap(resp as any)).toBeNull();
    });

    test('should return null for null response', () => {
      expect(extractDataMap(null)).toBeNull();
    });

    test('should return null for undefined response', () => {
      expect(extractDataMap(undefined)).toBeNull();
    });

    test('should return null for array data', () => {
      const resp = { success: true, data: [1, 2, 3] };
      expect(extractDataMap(resp as any)).toBeNull();
    });

    test('should extract from object without success field', () => {
      const resp = { id: '123', name: 'test' };
      expect(extractDataMap(resp as any)).toEqual({ id: '123', name: 'test' });
    });
  });

  describe('parseResponseData', () => {
    test('should parse successful response data', () => {
      const resp = { success: true, data: { id: '123' } };
      expect(parseResponseData<{ id: string }>(resp as any)).toEqual({ id: '123' });
    });

    test('should return null for failed response', () => {
      const resp = { success: false, data: null };
      expect(parseResponseData(resp as any)).toBeNull();
    });

    test('should return null for null response', () => {
      expect(parseResponseData(null)).toBeNull();
    });
  });

  describe('validateAndExtractData', () => {
    test('should extract valid data', () => {
      const resp = { success: true, data: { id: '123' } };
      expect(validateAndExtractData(resp as any)).toEqual({ id: '123' });
    });

    test('should throw for invalid data', () => {
      expect(() => validateAndExtractData(null)).toThrow(AgentOSError);
    });

    test('should throw with custom error message', () => {
      expect(() => validateAndExtractData(null, 'Custom error')).toThrow('Custom error');
    });
  });
});

describe('URL building', () => {
  describe('buildURL', () => {
    test('should return basePath without query params', () => {
      expect(buildURL('/api/v1/tasks')).toBe('/api/v1/tasks');
    });

    test('should append query params', () => {
      const url = buildURL('/api/v1/tasks', { page: '1', size: '10' });
      expect(url).toContain('page=1');
      expect(url).toContain('size=10');
      expect(url).toContain('?');
    });

    test('should append with & if basePath already has query', () => {
      const url = buildURL('/api?existing=1', { new: '2' });
      expect(url).toContain('&new=2');
    });

    test('should handle empty query params', () => {
      expect(buildURL('/api', {})).toBe('/api');
    });
  });

  describe('buildQueryString', () => {
    test('should build query string', () => {
      const qs = buildQueryString({ a: '1', b: '2' });
      expect(qs).toContain('a=1');
      expect(qs).toContain('b=2');
    });

    test('should return empty string for empty params', () => {
      expect(buildQueryString({})).toBe('');
    });
  });

  describe('buildListPath', () => {
    test('should build path without options', () => {
      expect(buildListPath('/api/v1/tasks')).toBe('/api/v1/tasks');
    });

    test('should build path with pagination', () => {
      const path = buildListPath('/api/v1/tasks', {
        pagination: { page: 2, pageSize: 10 },
      });
      expect(path).toContain('page=2');
      expect(path).toContain('page_size=10');
    });
  });
});

describe('Time parsing', () => {
  describe('parseTimeFromMap', () => {
    test('should parse ISO string', () => {
      const m = { ts: '2026-01-15T10:30:00Z' };
      const result = parseTimeFromMap(m, 'ts');
      expect(result.getFullYear()).toBe(2026);
    });

    test('should parse Unix timestamp (seconds)', () => {
      const m = { ts: 1736935800 };
      const result = parseTimeFromMap(m, 'ts');
      expect(result.getTime()).toBeGreaterThan(0);
    });

    test('should return epoch for missing key', () => {
      const result = parseTimeFromMap({}, 'ts');
      expect(result.getTime()).toBe(0);
    });
  });

  describe('parseTime', () => {
    test('should parse valid ISO string', () => {
      const result = parseTime('2026-01-15T10:30:00Z');
      expect(result.getFullYear()).toBe(2026);
    });

    test('should return current date for undefined', () => {
      const result = parseTime(undefined);
      expect(result.getTime()).toBeGreaterThan(0);
    });

    test('should return current date for invalid string', () => {
      const result = parseTime('not-a-date');
      expect(result.getTime()).toBeGreaterThan(0);
    });
  });
});

describe('extractInt64Stats', () => {
  test('should extract number fields', () => {
    const stats = extractInt64Stats({ a: 10, b: 20.5, c: '30' });
    expect(stats.a).toBe(10);
    expect(stats.b).toBe(20);
    expect(stats.c).toBe(30);
  });

  test('should ignore non-numeric fields', () => {
    const stats = extractInt64Stats({ a: 10, b: 'hello', c: true });
    expect(stats.a).toBe(10);
    expect(stats.b).toBeUndefined();
  });
});

describe('appendPagination', () => {
  test('should append page and page_size', () => {
    const result = appendPagination({}, 2, 20);
    expect(result.page).toBe('2');
    expect(result.page_size).toBe('20');
  });

  test('should preserve existing params', () => {
    const result = appendPagination({ sort: 'name' }, 1, 10);
    expect(result.sort).toBe('name');
    expect(result.page).toBe('1');
  });

  test('should skip zero page', () => {
    const result = appendPagination({}, 0, 10);
    expect(result.page).toBeUndefined();
    expect(result.page_size).toBe('10');
  });

  test('should skip zero pageSize', () => {
    const result = appendPagination({}, 1, 0);
    expect(result.page).toBe('1');
    expect(result.page_size).toBeUndefined();
  });
});

describe('ID and timestamp generation', () => {
  describe('generateId', () => {
    test('should generate unique IDs', () => {
      const id1 = generateId();
      const id2 = generateId();
      expect(id1).not.toBe(id2);
    });

    test('should start with aos_ prefix', () => {
      expect(generateId()).toMatch(/^aos_/);
    });
  });

  describe('generateTimestamp', () => {
    test('should return current Unix timestamp in seconds', () => {
      const ts = generateTimestamp();
      const now = Math.floor(Date.now() / 1000);
      expect(Math.abs(ts - now)).toBeLessThanOrEqual(1);
    });
  });
});

describe('Validation and sanitization', () => {
  describe('validateJSON', () => {
    test('should return true for valid JSON', () => {
      expect(validateJSON('{"key":"value"}')).toBe(true);
      expect(validateJSON('[1,2,3]')).toBe(true);
    });

    test('should return false for invalid JSON', () => {
      expect(validateJSON('not json')).toBe(false);
      expect(validateJSON('{invalid}')).toBe(false);
    });
  });

  describe('sanitizeString', () => {
    test('should trim whitespace', () => {
      expect(sanitizeString('  hello  ')).toBe('hello');
    });

    test('should remove null bytes', () => {
      expect(sanitizeString('hello\x00world')).toBe('helloworld');
    });

    test('should normalize CRLF to LF', () => {
      expect(sanitizeString('line1\r\nline2')).toBe('line1\nline2');
    });
  });

  describe('deepClone', () => {
    test('should deep clone object', () => {
      const obj = { a: 1, b: { c: 2 } };
      const clone = deepClone(obj);
      clone.b.c = 99;
      expect(obj.b.c).toBe(2);
    });

    test('should return null as-is', () => {
      expect(deepClone(null)).toBeNull();
    });

    test('should return primitive as-is', () => {
      expect(deepClone(42)).toBe(42);
      expect(deepClone('hello')).toBe('hello');
    });
  });

  describe('isEmpty', () => {
    test('should return true for null', () => {
      expect(isEmpty(null)).toBe(true);
    });

    test('should return true for undefined', () => {
      expect(isEmpty(undefined)).toBe(true);
    });

    test('should return true for empty array', () => {
      expect(isEmpty([])).toBe(true);
    });

    test('should return false for non-empty array', () => {
      expect(isEmpty([1])).toBe(false);
    });

    test('should return true for empty object', () => {
      expect(isEmpty({})).toBe(true);
    });

    test('should return false for non-empty object', () => {
      expect(isEmpty({ a: 1 })).toBe(false);
    });
  });
});

describe('List parsing', () => {
  describe('listOptionsToParams', () => {
    test('should return empty params for no options', () => {
      expect(listOptionsToParams()).toEqual({});
    });

    test('should convert pagination', () => {
      const params = listOptionsToParams({ pagination: { page: 2, pageSize: 10 } });
      expect(params.page).toBe('2');
      expect(params.page_size).toBe('10');
    });

    test('should convert sort', () => {
      const params = listOptionsToParams({ sort: { field: 'name', order: 'asc' } });
      expect(params.sort_by).toBe('name');
      expect(params.sort_order).toBe('asc');
    });

    test('should convert filter', () => {
      const params = listOptionsToParams({ filter: { key: 'status', value: 'active' } });
      expect(params.filter_key).toBe('status');
      expect(params.filter_value).toBe('active');
    });
  });

  describe('parseList', () => {
    test('should parse list from response', () => {
      const resp = {
        success: true,
        data: {
          tasks: [
            { task_id: '1', description: 'task1' },
            { task_id: '2', description: 'task2' },
          ],
        },
      };
      const result = parseList(resp as any, 'tasks', (item) => ({
        id: item.task_id as string,
        desc: item.description as string,
      }));
      expect(result).toHaveLength(2);
      expect(result[0].id).toBe('1');
      expect(result[1].desc).toBe('task2');
    });

    test('should throw for invalid response', () => {
      expect(() => parseList(null as any, 'tasks')).toThrow(AgentOSError);
    });
  });
});

describe('Parameter validation', () => {
  describe('validateRequiredString', () => {
    test('should pass for valid string', () => {
      expect(() => validateRequiredString('hello', 'param')).not.toThrow();
    });

    test('should throw for empty string', () => {
      expect(() => validateRequiredString('', 'param')).toThrow(AgentOSError);
    });

    test('should throw for whitespace-only string', () => {
      expect(() => validateRequiredString('  ', 'param')).toThrow(AgentOSError);
    });

    test('should throw for null', () => {
      expect(() => validateRequiredString(null, 'param')).toThrow(AgentOSError);
    });

    test('should throw for undefined', () => {
      expect(() => validateRequiredString(undefined, 'param')).toThrow(AgentOSError);
    });
  });

  describe('validatePositiveNumber', () => {
    test('should pass for positive number', () => {
      expect(() => validatePositiveNumber(1, 'param')).not.toThrow();
    });

    test('should throw for zero', () => {
      expect(() => validatePositiveNumber(0, 'param')).toThrow(AgentOSError);
    });

    test('should throw for negative number', () => {
      expect(() => validatePositiveNumber(-1, 'param')).toThrow(AgentOSError);
    });
  });

  describe('validateRequiredObject', () => {
    test('should pass for non-empty object', () => {
      expect(() => validateRequiredObject({ a: 1 }, 'param')).not.toThrow();
    });

    test('should throw for null', () => {
      expect(() => validateRequiredObject(null, 'param')).toThrow(AgentOSError);
    });

    test('should throw for empty object', () => {
      expect(() => validateRequiredObject({}, 'param')).toThrow(AgentOSError);
    });
  });

  describe('validateNonEmptyArray', () => {
    test('should pass for non-empty array', () => {
      expect(() => validateNonEmptyArray([1], 'param')).not.toThrow();
    });

    test('should throw for empty array', () => {
      expect(() => validateNonEmptyArray([], 'param')).toThrow(AgentOSError);
    });

    test('should throw for null', () => {
      expect(() => validateNonEmptyArray(null, 'param')).toThrow(AgentOSError);
    });

    test('should throw for undefined', () => {
      expect(() => validateNonEmptyArray(undefined, 'param')).toThrow(AgentOSError);
    });
  });
});
