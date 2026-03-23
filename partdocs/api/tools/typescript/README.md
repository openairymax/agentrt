Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# TypeScript SDK 文档

**版本**: v1.0.0.7  
**最后更新**: 2026-03-23  
**状态**: 生产就绪

---

## 1. 概述

TypeScript SDK 为 AgentOS 提供了完整的 TypeScript/JavaScript 客户端支持，包含文档管理、验证、生成和发布等全套功能。SDK 采用现代化的 TypeScript 开发，提供完整的类型定义和异步 API。

---

## 2. 安装

### 2.1 使用 npm 安装

```bash
# 安装最新版本
npm install @agentos/partdocs-sdk

# 或使用 yarn
yarn add @agentos/partdocs-sdk

# 或使用 pnpm
pnpm add @agentos/partdocs-sdk
```

### 2.2 使用 CDN

```html
<!-- 在浏览器中使用 -->
<script src="https://cdn.jsdelivr.net/npm/@agentos/partdocs-sdk/dist/browser/partdocs.min.js"></script>
```

### 2.3 环境要求

- **Node.js**: 18.0.0 或更高版本
- **TypeScript**: 5.0.0 或更高版本（可选）
- **浏览器**: Chrome 88+, Firefox 85+, Safari 14+, Edge 88+

---

## 3. 快速开始

### 3.1 初始化客户端

```typescript
import { PartdocsClient } from '@agentos/partdocs-sdk';

// 使用默认配置初始化客户端
const client = new PartdocsClient({
  baseUrl: 'http://localhost:8000',
  apiKey: 'your-api-key-here',
  timeout: 30000, // 30秒超时
});

// 或使用环境变量
const client = new PartdocsClient({
  baseUrl: process.env.PARTDOCS_API_URL || 'http://localhost:8000',
  apiKey: process.env.PARTDOCS_API_KEY,
});
```

### 3.2 创建第一个文档

```typescript
import { DocumentFormat, DocumentStatus } from '@agentos/partdocs-sdk';

async function createFirstDocument() {
  try {
    const document = await client.documents.create({
      title: '我的第一个文档',
      content: '# 欢迎使用 AgentOS Partdocs\n\n这是使用 TypeScript SDK 创建的第一个文档。',
      format: DocumentFormat.MARKDOWN,
      metadata: {
        author: '开发者',
        category: '示例',
      },
      tags: ['示例', '入门'],
    });

    console.log('文档创建成功:', document);
    console.log('文档ID:', document.id);
    console.log('文档链接:', `${client.config.baseUrl}/documents/${document.slug}`);
  } catch (error) {
    console.error('文档创建失败:', error);
  }
}

createFirstDocument();
```

### 3.3 验证文档

```typescript
async function validateDocument(documentId: string) {
  try {
    const validationResult = await client.validation.validate(documentId, {
      ruleTypes: ['syntax', 'links', 'terminology'],
      useCache: true,
    });

    console.log('验证结果:');
    console.log('总体状态:', validationResult.overallStatus);
    console.log('得分:', validationResult.score);
    console.log('问题数量:', validationResult.results.length);

    // 显示所有问题
    validationResult.results.forEach((result, index) => {
      console.log(`${index + 1}. [${result.severity}] ${result.message} (${result.location})`);
      if (result.suggestion) {
        console.log(`   建议: ${result.suggestion}`);
      }
    });
  } catch (error) {
    console.error('文档验证失败:', error);
  }
}
```

---

## 4. API 参考

### 4.1 文档管理 API

#### 4.1.1 文档接口

```typescript
// 获取文档列表
const documents = await client.documents.list({
  page: 1,
  limit: 20,
  status: DocumentStatus.PUBLISHED,
  format: DocumentFormat.MARKDOWN,
  tag: 'api',
  search: '系统调用',
});

// 获取单个文档
const document = await client.documents.get('document-id');

// 创建文档
const newDocument = await client.documents.create({
  title: '文档标题',
  content: '文档内容',
  format: DocumentFormat.MARKDOWN,
  templateId: 'template-id', // 可选
  metadata: { key: 'value' },
  tags: ['tag1', 'tag2'],
});

// 更新文档
const updatedDocument = await client.documents.update('document-id', {
  title: '新标题',
  content: '新内容',
  version: '1.0.1',
  metadata: { updated: true },
});

// 删除文档
await client.documents.delete('document-id');

// 获取文档版本历史
const versions = await client.documents.getVersions('document-id');

// 回滚到指定版本
const rolledBackDocument = await client.documents.rollback('document-id', '1.0.0');
```

---

## 4. 核心功能

### 4.1 文档管理

#### 4.1.1 文档模型

```typescript
import { 
  Document, 
  DocumentFormat, 
  DocumentStatus,
  DocumentVersion 
} from '@agentos/partdocs-sdk';

// 文档接口定义
interface Document {
  id: string;
  title: string;
  slug: string;
  content: string;
  format: DocumentFormat;
  version: string;
  status: DocumentStatus;
  authorId: string;
  createdAt: Date;
  updatedAt: Date;
  publishedAt?: Date;
  metadata: Record<string, any>;
  tags: string[];
  relatedDocs: string[];
}

// 文档版本接口
interface DocumentVersion {
  id: string;
  documentId: string;
  version: string;
  contentHash: string;
  changeSummary: string;
  authorId: string;
  createdAt: Date;
}
```

#### 4.1.2 批量操作

```typescript
// 批量获取文档
const documents = await client.documents.batchGet(['doc-1', 'doc-2', 'doc-3']);

// 批量删除文档
await client.documents.batchDelete(['doc-1', 'doc-2']);

// 批量更新文档标签
await client.documents.batchUpdateTags({
  documentIds: ['doc-1', 'doc-2'],
  addTags: ['新标签'],
  removeTags: ['旧标签'],
});
```

### 4.2 文档验证

#### 4.2.1 验证规则

```typescript
import { ValidationSeverity } from '@agentos/partdocs-sdk';

// 验证规则类型
enum ValidationRuleType {
  SYNTAX = 'syntax',      // 语法检查
  LINKS = 'links',        // 链接检查
  TERMINOLOGY = 'terminology', // 术语检查
  VERSION = 'version',    // 版本检查
  CROSS_REF = 'cross_ref', // 交叉引用检查
}

// 验证结果接口
interface ValidationResult {
  ruleId: string;
  ruleName: string;
  ruleType: ValidationRuleType;
  severity: ValidationSeverity;
  message: string;
  location: string; // 格式: "文件路径:行号:列号"
  suggestion?: string;
  timestamp: Date;
}

// 验证报告接口
interface ValidationReport {
  documentId: string;
  results: ValidationResult[];
  overallStatus: 'pass' | 'warning' | 'error';
  score: number; // 0.0 - 1.0
  validatedAt: Date;
}
```

#### 4.2.2 验证操作

```typescript
// 验证单个文档
const report = await client.validation.validate('document-id', {
  ruleTypes: [ValidationRuleType.SYNTAX, ValidationRuleType.LINKS],
  useCache: true,
});

// 批量验证文档
const batchReport = await client.validation.batchValidate({
  documentIds: ['doc-1', 'doc-2', 'doc-3'],
  ruleTypes: [ValidationRuleType.SYNTAX, ValidationRuleType.TERMINOLOGY],
});

// 获取验证规则列表
const rules = await client.validation.getRules();

// 创建自定义验证规则
const newRule = await client.validation.createRule({
  name: '自定义术语检查',
  description: '检查文档中是否使用了正确的术语',
  ruleType: ValidationRuleType.TERMINOLOGY,
  pattern: '\\b(api|API|Api)\\b',
  severity: ValidationSeverity.WARNING,
});
```

### 4.3 文档生成

#### 4.3.1 从模板生成

```typescript
// 从模板生成文档
const generatedDoc = await client.generation.fromTemplate({
  templateId: 'api-template',
  variables: {
    apiName: '用户管理API',
    version: '1.0.0',
    endpoints: [
      { method: 'GET', path: '/users', description: '获取用户列表' },
      { method: 'POST', path: '/users', description: '创建用户' },
    ],
  },
  outputFormat: DocumentFormat.MARKDOWN,
});

// 获取验证规则列表
const rules = await client.validation.getRules();

// 创建自定义验证规则
const newRule = await client.validation.createRule({
  name: '自定义术语检查',
  description: '检查文档中是否使用了正确的术语',
  ruleType: ValidationRuleType.TERMINOLOGY,
  pattern: '\\b(错误术语|incorrect term)\\b',
  severity: ValidationSeverity.WARNING,
});
```

### 4.3 文档生成

#### 4.3.1 从模板生成

```typescript
// 从模板生成文档
const generatedDoc = await client.generation.fromTemplate({
  templateId: 'api-documentation-template',
  variables: {
    projectName: 'AgentOS',
    module: 'Partdocs',
    version: '1.0.0',
    author: '开发团队',
    date: new Date().toISOString().split('T')[0],
  },
  outputFormat: DocumentFormat.MARKDOWN,
});

// 获取模板列表
const templates = await client.generation.getTemplates();

// 创建自定义模板
const newTemplate = await client.generation.createTemplate({
  name: 'API参考模板',
  description: '用于生成API参考文档的模板',
  templateType: 'api',
  content: `# {{apiName}} API 参考 v{{version}}

## 概述
{{description}}

## 端点列表
{% for endpoint in endpoints %}
### {{endpoint.method}} {{endpoint.path}}
{{endpoint.description}}

**请求参数:**
\`\`\`json
{{endpoint.request | tojson}}
\`\`\`

**响应示例:**
\`\`\`json
{{endpoint.response | tojson}}
\`\`\`
{% endfor %}
`,
  variables: {
    apiName: { type: 'string', required: true },
    version: { type: 'string', default: '1.0.0' },
    description: { type: 'string', required: true },
    endpoints: { type: 'array', items: { type: 'object' } },
  },
});
```

#### 4.3.2 从代码生成

```typescript
// 从TypeScript代码生成API文档
const apiDocs = await client.generation.fromCode({
  codePath: './src/api',
  language: 'typescript',
  outputFormat: DocumentFormat.MARKDOWN,
  options: {
    includePrivate: false,
    groupBy: 'category',
    includeExamples: true,
  },
});

// 从Python代码生成文档
const pythonDocs = await client.generation.fromCode({
  codePath: './python/modules',
  language: 'python',
  outputFormat: DocumentFormat.HTML,
});

// 从OpenAPI规范生成文档
const openapiDocs = await client.generation.fromContract({
  contractPath: './openapi.yaml',
  templateId: 'openapi-template',
});
```

### 4.4 文档发布

#### 4.4.1 发布配置

```typescript
// 发布文档到多个目标
const publishResult = await client.publishing.publish('document-id', {
  targets: ['web', 'pdf', 'epub'],
  options: {
    web: {
      theme: 'material',
      search: true,
      toc: true,
      analytics: {
        enabled: true,
        provider: 'google',
        trackingId: 'UA-XXXXX-Y',
      },
    },
    pdf: {
      size: 'A4',
      margin: '2cm',
      header: '{{title}} - 第{{page}}页',
      footer: '© {{year}} AgentOS',
    },
    epub: {
      coverImage: './cover.jpg',
      author: 'AgentOS Team',
      publisher: 'SPHARX',
      isbn: '978-3-16-148410-0',
    },
  },
});

// 获取发布状态
const status = await client.publishing.getStatus('document-id');

// 获取发布历史
const history = await client.publishing.getHistory('document-id');

// 取消发布
await client.publishing.cancel('document-id');
```

#### 4.4.2 静态网站生成

```typescript
// 生成静态网站
const website = await client.publishing.generateWebsite({
  documentIds: ['doc-1', 'doc-2', 'doc-3'],
  config: {
    siteName: 'AgentOS 文档中心',
    siteDescription: 'AgentOS 官方技术文档',
    siteUrl: 'https://docs.agentos.dev',
    theme: {
      primaryColor: '#1890ff',
      secondaryColor: '#52c41a',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial',
    },
    navigation: [
      { title: '首页', path: '/' },
      { title: 'API参考', path: '/api' },
      { title: '开发指南', path: '/guides' },
      { title: '架构设计', path: '/architecture' },
    ],
    plugins: ['search', 'mermaid', 'mathjax'],
  },
  outputDir: './dist/docs',
});
```

---

## 5. 高级功能

### 5.1 实时同步

```typescript
import { RealtimeClient } from '@agentos/partdocs-sdk/realtime';

// 创建实时客户端
const realtimeClient = new RealtimeClient({
  baseUrl: 'ws://localhost:8000',
  apiKey: 'your-api-key',
});

// 订阅文档更新
const unsubscribe = realtimeClient.subscribeToDocument('document-id', {
  onUpdate: (document) => {
    console.log('文档已更新:', document);
    // 更新UI或执行其他操作
  },
  onDelete: () => {
    console.log('文档已删除');
    // 清理UI或执行其他操作
  },
  onValidation: (report) => {
    console.log('验证完成:', report);
    // 显示验证结果
  },
});

// 取消订阅
unsubscribe();

// 订阅多个文档
const multiUnsubscribe = realtimeClient.subscribeToDocuments(['doc-1', 'doc-2'], {
  onBatchUpdate: (updates) => {
    updates.forEach(update => {
      console.log(`文档 ${update.documentId} 已更新`);
    });
  },
});

// 发送实时编辑
realtimeClient.sendEdit('document-id', {
  type: 'insert',
  position: { line: 10, column: 5 },
  text: '新内容',
  author: 'user-123',
});
```

### 5.2 搜索功能

```typescript
// 全文搜索
const searchResults = await client.search.fullText({
  query: '系统调用 API',
  filters: {
    format: DocumentFormat.MARKDOWN,
    status: DocumentStatus.PUBLISHED,
    tags: ['api', 'reference'],
  },
  options: {
    limit: 20,
    offset: 0,
    highlight: true,
    sortBy: 'relevance',
  },
});

// 语义搜索
const semanticResults = await client.search.semantic({
  query: '如何创建Agent',
  options: {
    limit: 10,
    similarityThreshold: 0.7,
  },
});

// 获取搜索建议
const suggestions = await client.search.suggest('sys');

// 获取相关文档
const relatedDocs = await client.search.getRelated('document-id', {
  limit: 5,
  minSimilarity: 0.6,
});
```

### 5.3 分析功能

```typescript
// 获取文档分析
const analytics = await client.analytics.getDocumentAnalytics('document-id', {
  period: '30d', // 30天
  metrics: ['views', 'clicks', 'shares', 'timeOnPage'],
});

// 获取用户行为分析
const userAnalytics = await client.analytics.getUserAnalytics({
  userId: 'user-123',
  period: '7d',
  actions: ['view', 'edit', 'share', 'comment'],
});

// 获取质量分析
const qualityAnalytics = await client.analytics.getQualityAnalytics({
  documentIds: ['doc-1', 'doc-2', 'doc-3'],
  metrics: ['readability', 'completeness', 'accuracy', 'freshness'],
});

// 导出分析报告
const report = await client.analytics.exportReport({
  type: 'pdf',
  period: '90d',
  metrics: ['all'],
  format: 'detailed',
});
```

### 5.4 国际化支持

```typescript
// 翻译文档
const translation = await client.i18n.translate({
  documentId: 'document-id',
  targetLanguage: 'en',
  options: {
    preserveFormatting: true,
    includeMetadata: true,
    quality: 'premium', // 'standard' | 'premium' | 'professional'
  },
});

// 获取可用语言
const languages = await client.i18n.getLanguages();

// 获取术语库
const terminology = await client.i18n.getTerminology({
  language: 'zh-CN',
  category: 'technical',
});

// 添加术语
await client.i18n.addTerm({
  term: '微内核',
  definition: '一种操作系统内核设计，只提供最基本的服务',
  language: 'zh-CN',
  category: 'architecture',
  synonyms: ['microkernel'],
  relatedTerms: ['内核', '操作系统'],
});

// 批量翻译
const batchTranslation = await client.i18n.batchTranslate({
  documentIds: ['doc-1', 'doc-2'],
  targetLanguages: ['en', 'ja', 'ko'],
  options: {
    useTranslationMemory: true,
    validateTerminology: true,
  },
});
```

---

## 6. 错误处理

### 6.1 错误类型

```typescript
import { 
  PartdocsError,
  AuthenticationError,
  AuthorizationError,
  ValidationError,
  NotFoundError,
  RateLimitError,
  ServerError 
} from '@agentos/partdocs-sdk/errors';

try {
  const document = await client.documents.get('non-existent-id');
} catch (error) {
  if (error instanceof NotFoundError) {
    console.error('文档不存在:', error.message);
    console.error('文档ID:', error.documentId);
  } else if (error instanceof AuthenticationError) {
    console.error('认证失败:', error.message);
    console.error('请检查API密钥');
  } else if (error instanceof RateLimitError) {
    console.error('请求频率限制:', error.message);
    console.error('重试时间:', error.retryAfter);
    await new Promise(resolve => setTimeout(resolve, error.retryAfter * 1000));
  } else if (error instanceof PartdocsError) {
    console.error('Partdocs错误:', error.message);
    console.error('错误代码:', error.code);
    console.error('请求ID:', error.requestId);
  } else {
    console.error('未知错误:', error);
  }
}
```

### 6.2 重试机制

```typescript
import { RetryConfig, withRetry } from '@agentos/partdocs-sdk/utils';

// 配置重试策略
const retryConfig: RetryConfig = {
  maxAttempts: 3,
  initialDelay: 1000, // 1秒
  maxDelay: 10000,    // 10秒
  backoffFactor: 2,
  retryableErrors: [RateLimitError, ServerError],
};

// 使用重试包装函数
const getDocumentWithRetry = withRetry(
  (id: string) => client.documents.get(id),
  retryConfig
);

// 调用带重试的函数
try {
  const document = await getDocumentWithRetry('document-id');
} catch (error) {
  console.error('所有重试尝试都失败了:', error);
}
```

### 6.3 请求拦截器

```typescript
// 添加请求拦截器
client.interceptors.request.use(
  (config) => {
    // 修改请求配置
    config.headers['X-Custom-Header'] = 'custom-value';
    config.headers['X-Request-ID'] = generateRequestId();
    
    // 记录请求日志
    console.log('发送请求:', config.method, config.url);
    
    return config;
  },
  (error) => {
    // 请求错误处理
    console.error('请求错误:', error);
    return Promise.reject(error);
  }
);

// 添加响应拦截器
client.interceptors.response.use(
  (response) => {
    // 处理响应数据
    console.log('收到响应:', response.status, response.config.url);
    
    // 可以修改响应数据
    if (response.data && response.data.metadata) {
      response.data.metadata.processedAt = new Date();
    }
    
    return response;
  },
  (error) => {
    // 响应错误处理
    console.error('响应错误:', error);
    
    // 可以重试或转换错误
    if (error.response?.status === 429) {
      return Promise.reject(new RateLimitError('请求频率限制', error.response));
    }
    
    return Promise.reject(error);
  }
);
```

---

## 7. 配置选项

### 7.1 客户端配置

```typescript
import { PartdocsClient, ClientConfig } from '@agentos/partdocs-sdk';

const config: ClientConfig = {
  // 必需配置
  baseUrl: 'http://localhost:8000',
  apiKey: 'your-api-key',
  
  // 可选配置
  timeout: 30000, // 请求超时时间（毫秒）
  maxRetries: 3,  // 最大重试次数
  retryDelay: 1000, // 重试延迟（毫秒）
  
  // 缓存配置
  cache: {
    enabled: true,
    ttl: 300000, // 缓存存活时间（毫秒）
    maxSize: 100, // 最大缓存条目数
  },
  
  // 日志配置
  logging: {
    level: 'info', // 'debug' | 'info' | 'warn' | 'error' | 'silent'
    format: 'json', // 'json' | 'text'
    destination: 'console', // 'console' | 'file' | 'remote'
  },
  
  // 请求配置
  request: {
    compress: true, // 启用压缩
    followRedirects: true, // 跟随重定向
    validateStatus: (status) => status >= 200 && status < 300,
  },
  
  // WebSocket配置（用于实时功能）
  websocket: {
    enabled: true,
    reconnect: true,
    reconnectInterval: 5000,
    maxReconnectAttempts: 10,
  },
};

const client = new PartdocsClient(config);
```

### 7.2 环境变量

```bash
# .env 文件示例
PARTDOCS_API_URL=http://localhost:8000
PARTDOCS_API_KEY=your-api-key-here
PARTDOCS_TIMEOUT=30000
PARTDOCS_MAX_RETRIES=3
PARTDOCS_CACHE_ENABLED=true
PARTDOCS_CACHE_TTL=300000
PARTDOCS_LOG_LEVEL=info
```

```typescript
// 从环境变量创建客户端
import { loadConfigFromEnv } from '@agentos/partdocs-sdk/config';

const config = loadConfigFromEnv();
const client = new PartdocsClient(config);
```

---

## 8. 最佳实践

### 8.1 性能优化

```typescript
// 1. 使用缓存
const client = new PartdocsClient({
  cache: {
    enabled: true,
    ttl: 5 * 60 * 1000, // 5分钟
    maxSize: 1000,
  },
});

// 2. 批量操作
async function processDocuments(documentIds: string[]) {
  // 批量获取而不是逐个获取
  const documents = await client.documents.batchGet(documentIds);
  
  // 批量验证
  const validationResults = await client.validation.batchValidate(documentIds);
  
  // 批量更新
  await client.documents.batchUpdateTags({
    documentIds,
    addTags: ['processed'],
  });
}

// 3. 使用流式处理
import { createDocumentStream } from '@agentos/partdocs-sdk/streams';

async function processLargeDocumentSet() {
  const stream = createDocumentStream({
    filter: { status: DocumentStatus.PUBLISHED },
    batchSize: 100,
  });
  
  for await (const batch of stream) {
    // 处理每批文档
    await processBatch(batch);
  }
}

// 4. 并行处理
async function parallelOperations() {
  const [documents, templates, rules] = await Promise.all([
    client.documents.list({ limit: 50 }),
    client.generation.getTemplates(),
    client.validation.getRules(),
  ]);
  
  // 使用结果...
}
```

### 8.2 错误处理最佳实践

```typescript
// 1. 集中错误处理
class DocumentService {
  private client: PartdocsClient;
  
  constructor(client: PartdocsClient) {
    this.client = client;
  }
  
  async getDocumentSafe(id: string): Promise<Document | null> {
    try {
      return await this.client.documents.get(id);
    } catch (error) {
      if (error instanceof NotFoundError) {
        console.warn(`文档 ${id} 不存在`);
        return null;
      }
      
      // 记录其他错误但继续执行
      console.error(`获取文档 ${id} 失败:`, error);
      throw error;
    }
  }
  
  async createDocumentWithValidation(data: CreateDocumentRequest): Promise<Document> {
    try {
      // 创建文档
      const document = await this.client.documents.create(data);
      
      // 验证文档
      const validation = await this.client.validation.validate(document.id);
      
      if (validation.overallStatus === 'error') {
        // 如果验证失败，删除文档
        await this.client.documents.delete(document.id);
        throw new ValidationError('文档验证失败', validation);
      }
      
      return document;
    } catch (error) {
      // 清理资源
      await this.cleanupOnError(error);
      throw error;
    }
  }
  
  private async cleanupOnError(error: any): Promise<void> {
    // 清理临时文件、回滚操作等
  }
}
```

### 8.3 安全最佳实践

```typescript
// 1. 安全存储API密钥
import { SecureStorage } from '@agentos/partdocs-sdk/security';

const storage = new SecureStorage({
  encryptionKey: process.env.ENCRYPTION_KEY,
});

// 安全存储API密钥
await storage.set('partdocs_api_key', 'your-api-key');

// 安全获取API密钥
const apiKey = await storage.get('partdocs_api_key');

// 2. 输入验证
import { validateDocumentInput } from '@agentos/partdocs-sdk/validation';

async function createDocumentSafe(input: any): Promise<Document> {
  // 验证输入
  const validatedInput = validateDocumentInput(input);
  
  // 清理HTML内容
  const cleanContent = sanitizeHtml(validatedInput.content);
  
  // 创建文档
  return await client.documents.create({
    ...validatedInput,
    content: cleanContent,
  });
}

// 3. 访问控制
class RoleBasedClient {
  private client: PartdocsClient;
  private userRole: string;
  
  constructor(client: PartdocsClient, userRole: string) {
    this.client = client;
    this.userRole = userRole;
  }
  
  async deleteDocument(id: string): Promise<void> {
    // 检查权限
    if (!this.hasPermission('document:delete')) {
      throw new AuthorizationError('没有删除文档的权限');
    }
    
    // 检查文档所有权
    const document = await this.client.documents.get(id);
    if (document.authorId !== this.getCurrentUserId() && !this.isAdmin()) {
      throw new AuthorizationError('只能删除自己的文档');
    }
    
    await this.client.documents.delete(id);
  }
  
  private hasPermission(permission: string): boolean {
    const rolePermissions = {
      admin: ['document:read', 'document:write', 'document:delete', 'document:publish'],
      editor: ['document:read', 'document:write'],
      viewer: ['document:read'],
    };
    
    return rolePermissions[this.userRole]?.includes(permission) || false;
  }
}
```

---

## 9. 示例应用

### 9.1 命令行工具

```typescript
// cli.ts
import { Command } from 'commander';
import { PartdocsClient } from '@agentos/partdocs-sdk';
import { config } from 'dotenv';

config();

const program = new Command();

program
  .name('partdocs-cli')
  .description('AgentOS Partdocs 命令行工具')
  .version('1.0.0');

program
  .command('create <title> <content>')
  .description('创建新文档')
  .option('-f, --format <format>', '文档格式', 'markdown')
  .option('-t, --tags <tags>', '文档标签（逗号分隔）')
  .action(async (title, content, options) => {
    const client = new PartdocsClient({
      baseUrl: process.env.PARTDOCS_API_URL!,
      apiKey: process.env.PARTDOCS_API_KEY!,
    });
    
    try {
      const document = await client.documents.create({
        title,
        content,
        format: options.format,
        tags: options.tags ? options.tags.split(',') : [],
      });
      
      console.log('文档创建成功:');
      console.log(`ID: ${document.id}`);
      console.log(`标题: ${document.title}`);
      console.log(`链接: ${process.env.PARTDOCS_API_URL}/documents/${document.slug}`);
    } catch (error) {
      console.error('文档创建失败:', error.message);
      process.exit(1);
    }
  });

program
  .command('validate <document-id>')
  .description('验证文档')
  .option('-r, --rules <rules>', '验证规则（逗号分隔）')
  .action(async (documentId, options) => {
    const client = new PartdocsClient({
      baseUrl: process.env.PARTDOCS_API_URL!,
      apiKey: process.env.PARTDOCS_API_KEY!,
    });
    
    try {
      const report = await client.validation.validate(documentId, {
        ruleTypes: options.rules ? options.rules.split(',') : undefined,
      });
      
      console.log('验证报告:');
      console.log(`总体状态: ${report.overallStatus}`);
      console.log(`得分: ${report.score.toFixed(2)}`);
      console.log(`问题数量: ${report.results.length}`);
      
      report.results.forEach((result, index) => {
        console.log(`\n${index + 1}. [${result.severity}] ${result.message}`);
        console.log(`   位置: ${result.location}`);
        if (result.suggestion) {
          console.log(`   建议: ${result.suggestion}`);
        }
      });
    } catch (error) {
      console.error('验证失败:', error.message);
      process.exit(1);
    }
  });

program.parse(process.argv);
```

### 9.2 React 组件

```typescript
// DocumentEditor.tsx
import React, { useState, useEffect } from 'react';
import { PartdocsClient, Document, DocumentFormat } from '@agentos/partdocs-sdk';
import { useDebounce } from './hooks/useDebounce';

interface DocumentEditorProps {
  documentId?: string;
  client: PartdocsClient;
  onSave?: (document: Document) => void;
  onError?: (error: Error) => void;
}

export const DocumentEditor: React.FC<DocumentEditorProps> = ({
  documentId,
  client,
  onSave,
  onError,
}) => {
  const [document, setDocument] = useState<Document | null>(null);
  const [content, setContent] = useState('');
  const [title, setTitle] = useState('');
  const [isSaving, setIsSaving] = useState(false);
  const [isValidating, setIsValidating] = useState(false);
  const [validationResults, setValidationResults] = useState<any[]>([]);
  
  const debouncedContent = useDebounce(content, 1000);
  
  // 加载文档
  useEffect(() => {
    if (documentId) {
      loadDocument(documentId);
    }
  }, [documentId]);
  
  // 自动保存
  useEffect(() => {
    if (document && (title !== document.title || content !== document.content)) {
      autoSave();
    }
  }, [debouncedContent, title]);
  
  // 自动验证
  useEffect(() => {
    if (document && content) {
      autoValidate();
    }
  }, [debouncedContent]);
  
  const loadDocument = async (id: string) => {
    try {
      const doc = await client.documents.get(id);
      setDocument(doc);
      setTitle(doc.title);
      setContent(doc.content);
    } catch (error) {
      onError?.(error as Error);
    }
  };
  
  const autoSave = async () => {
    if (isSaving) return;
    
    setIsSaving(true);
    try {
      const updatedDoc = document
        ? await client.documents.update(document.id, {
            title,
            content,
            version: `${document.version.split('.')[0]}.${parseInt(document.version.split('.')[1]) + 1}`,
          })
        : await client.documents.create({
            title,
            content,
            format: DocumentFormat.MARKDOWN,
          });
      
      setDocument(updatedDoc);
      onSave?.(updatedDoc);
    } catch (error) {
      onError?.(error as Error);
    } finally {
      setIsSaving(false);
    }
  };
  
  const autoValidate = async () => {
    if (!document || isValidating) return;
    
    setIsValidating(true);
    try {
      const report = await client.validation.validate(document.id, {
        ruleTypes: ['syntax', 'links'],
        useCache: true,
      });
      
      setValidationResults(report.results);
    } catch (error) {
      console.error('验证失败:', error);
    } finally {
      setIsValidating(false);
    }
  };
  
  const handlePublish = async () => {
    if (!document) return;
    
    try {
      await client.publishing.publish(document.id, {
        targets: ['web'],
        options: {
          web: { theme: 'material', search: true },
        },
      });
      
      alert('文档发布成功！');
    } catch (error) {
      onError?.(error as Error);
    }
  };
  
  return (
    <div className="document-editor">
      <div className="editor-header">
        <input
          type="text"
          value={title}
          onChange={(e) => setTitle(e.target.value)}
          placeholder="文档标题"
          className="title-input"
        />
        <div className="editor-actions">
          <button onClick={autoSave} disabled={isSaving}>
            {isSaving ? '保存中...' : '保存'}
          </button>
          <button onClick={handlePublish} disabled={!document}>
            发布
          </button>
        </div>
      </div>
      
      <div className="editor-container">
        <textarea
          value={content}
          onChange={(e) => setContent(e.target.value)}
          placeholder="开始编写文档..."
          className="content-editor"
        />
        
        <div className="validation-panel">
          <h3>验证结果</h3>
          {isValidating ? (
            <div>验证中...</div>
          ) : validationResults.length > 0 ? (
            <ul>
              {validationResults.map((result, index) => (
                <li key={index} className={`validation-${result.severity}`}>
                  <strong>{result.severity.toUpperCase()}:</strong> {result.message}
                  {result.suggestion && (
                    <div className="suggestion">建议: {result.suggestion}</div>
                  )}
                </li>
              ))}
            </ul>
          ) : (
            <div className="validation-pass">文档验证通过</div>
          )}
        </div>
      </div>
      
      {isSaving && <div className="saving-indicator">自动保存中...</div>}
    </div>
  );
};
```

---

## 10. 故障排除

### 10.1 常见问题

#### 10.1.1 连接问题

```typescript
// 问题: 无法连接到服务器
// 解决方案:
const client = new PartdocsClient({
  baseUrl: 'http://localhost:8000',
  timeout: 10000, // 增加超时时间
  maxRetries: 5,  // 增加重试次数
});

// 检查网络连接
try {
  await client.health.check();
  console.log('连接正常');
} catch (error) {
  console.error('连接失败:', error.message);
  console.error('请检查:');
  console.error('1. 服务器是否运行');
  console.error('2. 网络连接是否正常');
  console.error('3. 防火墙设置');
}
```

#### 10.1.2 认证问题

```typescript
// 问题: API密钥无效
// 解决方案:
// 1. 检查API密钥是否正确
console.log('API密钥:', process.env.PARTDOCS_API_KEY?.substring(0, 8) + '...');

// 2. 重新生成API密钥
const newApiKey = await client.auth.generateApiKey({
  name: '新的API密钥',
  expiresIn: '30d', // 30天后过期
  permissions: ['document:read', 'document:write'],
});

// 3. 更新环境变量
process.env.PARTDOCS_API_KEY = newApiKey.key;
```

#### 10.1.3 性能问题

```typescript
// 问题: 请求速度慢
// 解决方案:
const client = new PartdocsClient({
  // 启用缓存
  cache: {
    enabled: true,
    ttl: 300000, // 5分钟
  },
  
  // 使用连接池
  request: {
    pool: {
      maxSockets: 50,
      maxFreeSockets: 10,
      timeout: 60000,
    },
  },
});

// 使用批量操作
const documents = await client.documents.batchGet(['doc-1', 'doc-2', 'doc-3']);

// 使用流式处理
const stream = createDocumentStream({ batchSize: 100 });
for await (const batch of stream) {
  // 处理批量数据
}
```

### 10.2 调试技巧

```typescript
// 启用详细日志
const client = new PartdocsClient({
  logging: {
    level: 'debug',
    format: 'text',
  },
});

// 添加请求/响应拦截器进行调试
client.interceptors.request.use(config => {
  console.debug('请求:', {
    method: config.method,
    url: config.url,
    headers: config.headers,
    data: config.data,
  });
  return config;
});

client.interceptors.response.use(response => {
  console.debug('响应:', {
    status: response.status,
    data: response.data,
    headers: response.headers,
  });
  return response;
}, error => {
  console.error('错误:', {
    message: error.message,
    code: error.code,
    response: error.response?.data,
    config: error.config,
  });
  return Promise.reject(error);
});

// 使用性能监控
import { performance } from 'perf_hooks';

async function measurePerformance() {
  const start = performance.now();
  
  const document = await client.documents.get('document-id');
  
  const end = performance.now();
  console.log(`请求耗时: ${(end - start).toFixed(2)}ms`);
  
  return document;
}
```

### 10.3 获取帮助

```typescript
// 获取SDK版本信息
import { version } from '@agentos/partdocs-sdk/package.json';
console.log('SDK版本:', version);

// 获取客户端信息
console.log('客户端配置:', client.config);

// 获取服务器信息
const serverInfo = await client.system.getInfo();
console.log('服务器信息:', serverInfo);

// 获取健康状态
const health = await client.health.check();
console.log('健康状态:', health);

// 获取指标
const metrics = await client.metrics.get();
console.log('性能指标:', metrics);
```

---

## 11. 更新日志

### 11.1 v1.0.0 (2026-03-23)
- 初始版本发布
- 支持文档管理、验证、生成、发布全套功能
- 提供完整的TypeScript类型定义
- 支持实时同步和WebSocket
- 包含React组件示例

### 11.2 未来版本计划
- **v1.1.0**: 增强实时协作功能
- **v1.2.0**: 添加离线支持和PWA
- **v2.0.0**: 支持插件系统和扩展API

---

## 12. 相关资源

### 12.1 官方资源
- [GitHub仓库](https://github.com/agentos/partdocs-sdk-typescript)
- [API参考文档](https://docs.agentos.dev/api/typescript)
- [示例代码](https://github.com/agentos/partdocs-sdk-examples)

### 12.2 社区资源
- [Discord社区](https://discord.gg/agentos)
- [Stack Overflow标签](https://stackoverflow.com/questions/tagged/agentos-partdocs)
- [博客教程](https://blog.agentos.dev/tag/partdocs)

### 12.3 工具集成
- [VS Code扩展](https://marketplace.visualstudio.com/items?itemName=agentos.partdocs)
- [WebStorm插件](https://plugins.jetbrains.com/plugin/12345-agentos-partdocs)
- [CLI工具](https://www.npmjs.com/package/@agentos/partdocs-cli)

---

## 13. 支持与贡献

### 13.1 获取支持
- **文档**: 查看本文档和API参考
- **问题**: 在GitHub Issues报告问题
- **讨论**: 加入Discord社区讨论
- **邮件**: support@agentos.dev

### 13.2 贡献指南
1. Fork项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

### 13.3 代码规范
- 使用TypeScript严格模式
- 遵循ESLint规则
- 编写单元测试
- 添加类型定义
- 更新文档

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*