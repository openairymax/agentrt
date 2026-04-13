import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useI18n } from '../i18n';

interface ProtocolInfo {
  id: string;
  name: string;
  description: string;
  version: string;
  status: string;
  endpoint: string;
  capabilities: string[];
  color: string;
  icon: string;
}

interface ProtocolCapability {
  name: string;
  description: string;
  params: string[];
}

interface ConnectionTestResult {
  protocol_id: string;
  endpoint: string;
  success: boolean;
  latency_ms: number;
  message: string;
  details?: Record<string, unknown>;
}

interface ProtocolMessageResponse {
  protocol: string;
  success: boolean;
  data: unknown;
  error?: string;
  latency_ms: number;
}

const ProtocolPlayground: React.FC = () => {
  const { t } = useI18n();
  const [protocols, setProtocols] = useState<ProtocolInfo[]>([]);
  const [selectedProtocol, setSelectedProtocol] = useState<string>('');
  const [capabilities, setCapabilities] = useState<ProtocolCapability[]>([]);
  const [testEndpoint, setTestEndpoint] = useState<string>('http://localhost:18789');
  const [testResult, setTestResult] = useState<ConnectionTestResult | null>(null);
  const [testing, setTesting] = useState(false);
  const [messageMethod, setMessageMethod] = useState<string>('');
  const [messageParams, setMessageParams] = useState<string>('{}');
  const [messageResponse, setMessageResponse] = useState<ProtocolMessageResponse | null>(null);
  const [sending, setSending] = useState(false);
  const [error, setError] = useState<string>('');

  useEffect(() => {
    loadProtocols();
  }, []);

  useEffect(() => {
    if (selectedProtocol) {
      loadCapabilities(selectedProtocol);
    }
  }, [selectedProtocol]);

  const loadProtocols = async () => {
    try {
      const result = await invoke<ProtocolInfo[]>('list_protocols');
      setProtocols(result);
      if (result.length > 0 && !selectedProtocol) {
        setSelectedProtocol(result[0].id);
      }
    } catch (e) {
      setError(`Failed to load protocols: ${e}`);
    }
  };

  const loadCapabilities = async (protocolId: string) => {
    try {
      const result = await invoke<ProtocolCapability[]>('get_protocol_capabilities', { protocolId });
      setCapabilities(result);
    } catch {
      setCapabilities([]);
    }
  };

  const testConnection = async () => {
    if (!selectedProtocol || !testEndpoint) return;
    setTesting(true);
    setTestResult(null);
    setError('');

    try {
      const result = await invoke<ConnectionTestResult>('test_protocol_connection', {
        protocolId: selectedProtocol,
        endpoint: testEndpoint,
      });
      setTestResult(result);
    } catch (e) {
      setError(`Connection test failed: ${e}`);
    } finally {
      setTesting(false);
    }
  };

  const sendMessage = async () => {
    if (!selectedProtocol || !messageMethod) return;
    setSending(true);
    setMessageResponse(null);
    setError('');

    let params;
    try {
      params = JSON.parse(messageParams || '{}');
    } catch {
      setError('Invalid JSON in parameters');
      setSending(false);
      return;
    }

    try {
      const result = await invoke<ProtocolMessageResponse>('send_protocol_message', {
        message: {
          protocol: selectedProtocol,
          method: messageMethod,
          params,
        },
      });
      setMessageResponse(result);
    } catch (e) {
      setError(`Message send failed: ${e}`);
    } finally {
      setSending(false);
    }
  };

  const selectCapability = (cap: ProtocolCapability) => {
    setMessageMethod(cap.name);
    const defaultParams: Record<string, string> = {};
    cap.params.forEach((p) => {
      defaultParams[p] = '';
    });
    setMessageParams(JSON.stringify(defaultParams, null, 2));
  };

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold text-gray-900 dark:text-white">
          协议兼容性演示
        </h1>
        <span className="text-sm text-gray-500 dark:text-gray-400">
          AgentOS UnifiedProtocol
        </span>
      </div>

      {error && (
        <div className="bg-red-50 dark:bg-red-900/20 border border-red-200 dark:border-red-800 rounded-lg p-3 text-red-700 dark:text-red-300 text-sm">
          {error}
        </div>
      )}

      <div className="grid grid-cols-1 lg:grid-cols-4 gap-4">
        {protocols.map((proto) => (
          <button
            key={proto.id}
            onClick={() => setSelectedProtocol(proto.id)}
            className={`p-4 rounded-xl border-2 transition-all text-left ${
              selectedProtocol === proto.id
                ? 'border-blue-500 bg-blue-50 dark:bg-blue-900/20 shadow-md'
                : 'border-gray-200 dark:border-gray-700 hover:border-gray-300 dark:hover:border-gray-600'
            }`}
          >
            <div className="flex items-center gap-2 mb-2">
              <span className="text-2xl">{proto.icon}</span>
              <span className="font-semibold text-gray-900 dark:text-white">
                {proto.name}
              </span>
            </div>
            <p className="text-xs text-gray-500 dark:text-gray-400 mb-2">
              {proto.description}
            </p>
            <div className="flex items-center gap-2">
              <span
                className="inline-block w-2 h-2 rounded-full"
                style={{ backgroundColor: proto.status === 'active' ? '#4CAF50' : '#9E9E9E' }}
              />
              <span className="text-xs text-gray-500 dark:text-gray-400">
                {proto.status} · {proto.endpoint}
              </span>
            </div>
          </button>
        ))}
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="bg-white dark:bg-gray-800 rounded-xl border border-gray-200 dark:border-gray-700 p-5">
          <h2 className="text-lg font-semibold mb-4 text-gray-900 dark:text-white">
            连接测试
          </h2>
          <div className="space-y-3">
            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
                端点地址
              </label>
              <input
                type="text"
                value={testEndpoint}
                onChange={(e) => setTestEndpoint(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white text-sm"
                placeholder="http://localhost:18789"
              />
            </div>
            <button
              onClick={testConnection}
              disabled={testing || !selectedProtocol}
              className="w-full px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed text-sm font-medium"
            >
              {testing ? '测试中...' : `测试 ${selectedProtocol.toUpperCase()} 连接`}
            </button>
            {testResult && (
              <div
                className={`p-3 rounded-lg text-sm ${
                  testResult.success
                    ? 'bg-green-50 dark:bg-green-900/20 text-green-700 dark:text-green-300'
                    : 'bg-red-50 dark:bg-red-900/20 text-red-700 dark:text-red-300'
                }`}
              >
                <div className="flex items-center gap-2 mb-1">
                  <span>{testResult.success ? '✅' : '❌'}</span>
                  <span className="font-medium">{testResult.message}</span>
                </div>
                <div className="text-xs opacity-75">
                  延迟: {testResult.latency_ms}ms · 端点: {testResult.endpoint}
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="bg-white dark:bg-gray-800 rounded-xl border border-gray-200 dark:border-gray-700 p-5">
          <h2 className="text-lg font-semibold mb-4 text-gray-900 dark:text-white">
            协议能力
          </h2>
          {capabilities.length === 0 ? (
            <p className="text-sm text-gray-500 dark:text-gray-400">
              选择协议查看可用方法
            </p>
          ) : (
            <div className="space-y-2 max-h-64 overflow-y-auto">
              {capabilities.map((cap) => (
                <button
                  key={cap.name}
                  onClick={() => selectCapability(cap)}
                  className={`w-full text-left p-2 rounded-lg text-sm transition-colors ${
                    messageMethod === cap.name
                      ? 'bg-blue-50 dark:bg-blue-900/20 border border-blue-200 dark:border-blue-800'
                      : 'hover:bg-gray-50 dark:hover:bg-gray-700'
                  }`}
                >
                  <div className="font-mono font-medium text-gray-900 dark:text-white">
                    {cap.name}
                  </div>
                  <div className="text-xs text-gray-500 dark:text-gray-400">
                    {cap.description}
                    {cap.params.length > 0 && (
                      <span className="ml-1">
                        · 参数: {cap.params.join(', ')}
                      </span>
                    )}
                  </div>
                </button>
              ))}
            </div>
          )}
        </div>
      </div>

      <div className="bg-white dark:bg-gray-800 rounded-xl border border-gray-200 dark:border-gray-700 p-5">
        <h2 className="text-lg font-semibold mb-4 text-gray-900 dark:text-white">
          消息发送
        </h2>
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
          <div className="space-y-3">
            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
                方法名
              </label>
              <input
                type="text"
                value={messageMethod}
                onChange={(e) => setMessageMethod(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white text-sm font-mono"
                placeholder="tools/list"
              />
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
                参数 (JSON)
              </label>
              <textarea
                value={messageParams}
                onChange={(e) => setMessageParams(e.target.value)}
                rows={6}
                className="w-full px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-gray-700 text-gray-900 dark:text-white text-sm font-mono"
                placeholder='{"key": "value"}'
              />
            </div>
            <button
              onClick={sendMessage}
              disabled={sending || !selectedProtocol || !messageMethod}
              className="w-full px-4 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700 disabled:opacity-50 disabled:cursor-not-allowed text-sm font-medium"
            >
              {sending ? '发送中...' : `发送 ${selectedProtocol.toUpperCase()} 消息`}
            </button>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">
              响应
            </label>
            {messageResponse ? (
              <div className="space-y-2">
                <div
                  className={`flex items-center gap-2 text-sm ${
                    messageResponse.success
                      ? 'text-green-600 dark:text-green-400'
                      : 'text-red-600 dark:text-red-400'
                  }`}
                >
                  <span>{messageResponse.success ? '✅' : '❌'}</span>
                  <span>
                    {messageResponse.protocol.toUpperCase()} · {messageResponse.latency_ms}ms
                  </span>
                </div>
                <pre className="p-3 bg-gray-50 dark:bg-gray-900 rounded-lg text-xs font-mono overflow-auto max-h-64 text-gray-800 dark:text-gray-200">
                  {JSON.stringify(
                    messageResponse.success ? messageResponse.data : { error: messageResponse.error },
                    null,
                    2
                  )}
                </pre>
              </div>
            ) : (
              <div className="p-3 bg-gray-50 dark:bg-gray-900 rounded-lg text-sm text-gray-400 dark:text-gray-500 text-center">
                发送消息后查看响应
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ProtocolPlayground;
