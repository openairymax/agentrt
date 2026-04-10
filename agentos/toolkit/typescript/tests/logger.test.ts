import {
  Logger,
  LogLevel,
  getLogger,
  setLogger,
  newLogger,
} from '../src/utils/logger';

describe('LogLevel', () => {
  test('should have correct numeric ordering', () => {
    expect(LogLevel.DEBUG).toBeLessThan(LogLevel.INFO);
    expect(LogLevel.INFO).toBeLessThan(LogLevel.WARN);
    expect(LogLevel.WARN).toBeLessThan(LogLevel.ERROR);
    expect(LogLevel.ERROR).toBeLessThan(LogLevel.NONE);
  });
});

describe('Logger', () => {
  test('should create logger with default config', () => {
    const logger = new Logger();
    expect(logger).toBeInstanceOf(Logger);
  });

  test('should create logger with debug mode', () => {
    const logger = new Logger({ debug: true });
    const output = jest.fn();
    const testLogger = new Logger({ debug: true, output });
    testLogger.debug('test message');
    expect(output).toHaveBeenCalledWith(LogLevel.DEBUG, 'test message');
  });

  test('should create logger with level string', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'warn', output });
    logger.info('should not appear');
    logger.warn('should appear');
    expect(output).toHaveBeenCalledTimes(1);
    expect(output).toHaveBeenCalledWith(LogLevel.WARN, 'should appear');
  });

  test('should respect DEBUG level', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'debug', output });
    logger.debug('debug msg');
    expect(output).toHaveBeenCalledWith(LogLevel.DEBUG, 'debug msg');
  });

  test('should respect INFO level', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'info', output });
    logger.debug('should be filtered');
    logger.info('info msg');
    expect(output).toHaveBeenCalledTimes(1);
    expect(output).toHaveBeenCalledWith(LogLevel.INFO, 'info msg');
  });

  test('should respect WARN level', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'warn', output });
    logger.info('should be filtered');
    logger.warn('warn msg');
    expect(output).toHaveBeenCalledTimes(1);
    expect(output).toHaveBeenCalledWith(LogLevel.WARN, 'warn msg');
  });

  test('should respect ERROR level', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'error', output });
    logger.warn('should be filtered');
    logger.error('error msg');
    expect(output).toHaveBeenCalledTimes(1);
    expect(output).toHaveBeenCalledWith(LogLevel.ERROR, 'error msg');
  });

  test('should respect NONE level', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'none', output });
    logger.error('should be filtered');
    expect(output).not.toHaveBeenCalled();
  });

  test('should default to INFO for unknown level', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'unknown', output });
    logger.info('info msg');
    logger.debug('debug msg');
    expect(output).toHaveBeenCalledTimes(1);
    expect(output).toHaveBeenCalledWith(LogLevel.INFO, 'info msg');
  });

  test('should pass extra args to output', () => {
    const output = jest.fn();
    const logger = new Logger({ level: 'debug', output });
    logger.debug('msg', 'arg1', 42);
    expect(output).toHaveBeenCalledWith(LogLevel.DEBUG, 'msg', 'arg1', 42);
  });

  test('should use custom output function', () => {
    const messages: string[] = [];
    const output = (_level: LogLevel, message: string) => {
      messages.push(message);
    };
    const logger = new Logger({ level: 'debug', output });
    logger.info('hello');
    logger.warn('world');
    expect(messages).toEqual(['hello', 'world']);
  });

  test('should use default output when no custom output', () => {
    const logger = new Logger({ level: 'info' });
    const consoleSpy = jest.spyOn(console, 'info').mockImplementation();
    logger.info('test info');
    expect(consoleSpy).toHaveBeenCalledWith(
      expect.stringContaining('[INFO] test info'),
    );
    consoleSpy.mockRestore();
  });
});

describe('getLogger / setLogger', () => {
  test('should return default logger', () => {
    const logger = getLogger();
    expect(logger).toBeInstanceOf(Logger);
  });

  test('should set new default logger', () => {
    const output = jest.fn();
    setLogger({ level: 'debug', output });
    const logger = getLogger();
    logger.debug('test');
    expect(output).toHaveBeenCalledWith(LogLevel.DEBUG, 'test');
    setLogger({});
  });

  test('should replace default logger on subsequent call', () => {
    const output1 = jest.fn();
    const output2 = jest.fn();
    setLogger({ level: 'debug', output: output1 });
    getLogger().debug('first');
    setLogger({ level: 'debug', output: output2 });
    getLogger().debug('second');
    expect(output1).toHaveBeenCalledTimes(1);
    expect(output2).toHaveBeenCalledTimes(1);
    setLogger({});
  });
});

describe('newLogger', () => {
  test('should create new logger instance', () => {
    const logger = newLogger();
    expect(logger).toBeInstanceOf(Logger);
    expect(logger).not.toBe(getLogger());
  });

  test('should create logger with config', () => {
    const output = jest.fn();
    const logger = newLogger({ level: 'warn', output });
    logger.info('filtered');
    logger.warn('visible');
    expect(output).toHaveBeenCalledTimes(1);
  });
});
