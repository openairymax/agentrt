# AgentOS Gateway 安全最佳实践

**版本**: v1.0.0  
**更新日期**: 2026-04-05  
**适用模块**: AgentOS Gateway (协议转换层)

---

## 📋 目录

1. [威胁模型](#威胁模型)
2. [安全配置](#安全配置)
3. [安全清单](#安全清单)
4. [已知限制](#已知限制)
5. [事件响应](#事件响应)
6. [安全审计](#安全审计)

---

## 🔍 威胁模型

Gateway 作为协议转换层，面临的主要威胁包括：

### 1. 拒绝服务攻击 (DoS)

#### 风险描述
- 大量请求耗尽系统资源（CPU、内存、连接数）
- 超大请求体导致内存溢出
- 慢速连接占用连接池

#### 缓解措施

**✅ 已实施**:
- ✅ **请求体大小限制**: 默认 1MB（可配置 `GATEWAY_MAX_REQUEST_SIZE`）
- ✅ **速率限制**: 基于令牌桶算法（可配置 `GATEWAY_RATE_LIMIT_ENABLED`）
- ✅ **连接数限制**: libmicrohttpd 内置限制（默认 1000）
- ✅ **超时机制**: HTTP 连接超时 30 秒

**配置示例**:
```bash
# 请求大小限制
GATEWAY_MAX_REQUEST_SIZE=1048576  # 1MB

# 速率限制
GATEWAY_RATE_LIMIT_ENABLED=true
GATEWAY_RATE_LIMIT_RPS=100        # 每秒 100 请求
GATEWAY_RATE_LIMIT_RPM=6000       # 每分钟 6000 请求

# 连接限制（通过环境变量或配置文件）
GATEWAY_CONNECTION_LIMIT=1000
GATEWAY_HTTP_TIMEOUT=30
```

**监控指标**:
- `gateway_rate_limit_rejected_total` - 速率限制拒绝数
- `gateway_http_active_connections` - 活跃连接数
- `gateway_http_requests_total` - 总请求数

---

### 2. 跨站请求伪造 (CSRF)

#### 风险描述
- 恶意网站诱导用户浏览器发送跨域请求
- 利用用户已登录状态执行未授权操作

#### 缓解措施

**✅ 已实施**:
- ✅ **CORS 白名单**: 生产环境必须配置允许的来源列表
- ✅ **Origin 头验证**: 动态反射验证，不使用通配符 `*`
- ✅ **预检请求缓存**: 减少 OPTIONS 请求频率

**配置示例**:
```bash
# 生产环境（必须配置）
GATEWAY_CORS_MODE=prod
GATEWAY_CORS_ORIGINS=https://your-domain.com,https://api.your-domain.com

# 开发环境（仅本地开发使用）
GATEWAY_CORS_MODE=dev
```

**⚠️ 重要**: 
- 生产环境**绝对不要**使用 `GATEWAY_CORS_MODE=dev`
- 定期审查白名单，移除不再信任的域名

---

### 3. 注入攻击

#### 风险描述
- JSON-RPC 注入恶意内容
- 参数类型混淆攻击
- 路径遍历攻击

#### 缓解措施

**✅ 已实施**:
- ✅ **JSON 格式验证**: `jsonrpc_validate_request()` 严格验证
- ✅ **参数类型检查**: cJSON 类型验证
- ✅ **输入长度限制**: 请求体大小限制
- ✅ **路径规范化**: URL 路由处理

**代码示例**:
```c
// jsonrpc.c - 严格验证
int jsonrpc_validate_request(const cJSON* json) {
    /* 检查必需字段 */
    if (!cJSON_HasObjectItem(json, "jsonrpc") ||
        !cJSON_HasObjectItem(json, "method") ||
        !cJSON_HasObjectItem(json, "id")) {
        return -1;
    }
    
    /* 验证 jsonrpc 版本 */
    const cJSON* jsonrpc = cJSON_GetObjectItemCaseSensitive(json, "jsonrpc");
    if (!cJSON_IsString(jsonrpc) || 
        strcmp(jsonrpc->valuestring, "2.0") != 0) {
        return -2;
    }
    
    /* ... 更多验证 */
}
```

---

### 4. 信息泄露

#### 风险描述
- 错误消息暴露内部实现细节
- 日志记录敏感信息
- 监控指标泄露架构信息

#### 缓解措施

**✅ 已实施**:
- ✅ **标准化错误响应**: 不暴露内部错误堆栈
- ✅ **日志级别控制**: 生产环境使用 `info` 级别
- ✅ **敏感信息脱敏**: 不在日志中记录完整请求体

**配置建议**:
```bash
# 生产环境日志配置
GATEWAY_LOG_LEVEL=info
GATEWAY_LOG_FORMAT=json  # 结构化日志便于审计
```

---

## 🔒 安全配置

### 生产环境检查清单

#### 必须配置项

- [ ] **CORS 白名单模式**
  ```bash
  GATEWAY_CORS_MODE=prod
  GATEWAY_CORS_ORIGINS=https://trusted-domain1.com,https://trusted-domain2.com
  ```

- [ ] **请求大小限制**
  ```bash
  GATEWAY_MAX_REQUEST_SIZE=1048576  # 1MB
  ```

- [ ] **速率限制启用**
  ```bash
  GATEWAY_RATE_LIMIT_ENABLED=true
  GATEWAY_RATE_LIMIT_RPS=100
  GATEWAY_RATE_LIMIT_RPM=6000
  ```

- [ ] **日志级别**
  ```bash
  GATEWAY_LOG_LEVEL=info
  ```

#### 推荐配置项

- [ ] **连接超时**
  ```bash
  GATEWAY_HTTP_TIMEOUT=30
  ```

- [ ] **连接数限制**
  ```bash
  GATEWAY_CONNECTION_LIMIT=1000
  ```

- [ ] **结构化日志**
  ```bash
  GATEWAY_LOG_FORMAT=json
  ```

---

### 开发环境配置

```bash
# .env.dev 示例
GATEWAY_CORS_MODE=dev                    # 允许所有来源（仅开发！）
GATEWAY_RATE_LIMIT_ENABLED=false         # 禁用速率限制
GATEWAY_MAX_REQUEST_SIZE=10485760        # 10MB（便于调试）
GATEWAY_LOG_LEVEL=debug                  # 详细日志
```

**⚠️ 警告**: 开发环境配置**绝对不能**用于生产！

---

## ✅ 安全清单

### 部署前检查

- [ ] **CORS 配置**
  - [ ] 生产环境使用白名单模式
  - [ ] 白名单仅包含信任域名
  - [ ] 未使用通配符 `*`

- [ ] **速率限制**
  - [ ] 已启用速率限制
  - [ ] 阈值设置合理（根据业务需求）
  - [ ] 已配置监控告警

- [ ] **资源限制**
  - [ ] 请求体大小限制已配置
  - [ ] 连接数限制已配置
  - [ ] 超时时间已配置

- [ ] **日志配置**
  - [ ] 生产环境使用 `info` 级别
  - [ ] 使用结构化日志格式
  - [ ] 日志存储安全（加密、访问控制）

- [ ] **网络安全**
  - [ ] TLS/HTTPS 已在反向代理层配置
  - [ ] 防火墙规则已配置
  - [ ] 仅暴露必要端口

- [ ] **监控告警**
  - [ ] Prometheus 指标已配置
  - [ ] Grafana 监控面板已导入
  - [ ] 告警规则已设置

---

### 定期审计

- [ ] **每月检查**
  - [ ] 审查 CORS 白名单
  - [ ] 分析速率限制拒绝日志
  - [ ] 检查异常请求模式

- [ ] **每季度检查**
  - [ ] 更新依赖库（cJSON, libmicrohttpd, libwebsockets）
  - [ ] 审查安全日志
  - [ ] 进行性能基准测试

- [ ] **每年检查**
  - [ ] 全面安全审计
  - [ ] 威胁模型更新
  - [ ] 灾难恢复演练

---

## ⚠️ 已知限制

### 1. 无内置认证授权

**说明**: Gateway 职责仅为协议转换，认证授权由 `cupolas/permission` 安全穹顶负责。

**影响**: 
- Gateway 层不验证用户身份
- 不检查操作权限

**建议**: 
- 在反向代理层（Nginx/Envoy）添加 JWT 验证
- 或通过自定义回调实现认证逻辑

**示例**:
```c
// 自定义认证回调
int auth_callback(const char* request_json, char** response_json, void* user_data) {
    /* 1. 解析请求，提取 token */
    /* 2. 验证 token 有效性 */
    /* 3. 如果无效，返回 401 */
    /* 4. 如果有效，调用 gateway_rpc_handle_request */
}

// 设置回调
gateway_set_handler(gw, auth_callback, NULL);
```

---

### 2. 无传输加密

**说明**: Gateway 本身不处理 TLS/HTTPS，应在反向代理层处理。

**建议架构**:
```
客户端 (HTTPS) → Nginx/Envoy (TLS 终结) → Gateway (HTTP)
```

**Nginx 配置示例**:
```nginx
server {
    listen 443 ssl http2;
    server_name api.example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    
    location / {
        proxy_pass http://gateway:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

---

### 3. 速率限制为可选功能

**说明**: 速率限制默认禁用，需显式启用。

**原因**: 
- 不同业务场景需求差异大
- 避免误伤正常用户

**建议**: 
- **公开服务**: 必须启用
- **内网服务**: 根据需求评估
- **开发环境**: 可以禁用

---

## 🚨 事件响应

### 发现安全事件时

#### 1. 立即响应

```bash
# 查看实时日志
docker-compose logs -f gateway | grep -E "(ERROR|WARN|rate.limit)"

# 查看 Prometheus 指标
curl http://localhost:8080/metrics | grep -E "(rejected|failed)"

# 检查当前连接数
curl http://localhost:8080/metrics | grep active_connections
```

#### 2. 临时缓解

```bash
# 调整速率限制（更严格）
docker exec agentos-gateway sh -c \
  "export GATEWAY_RATE_LIMIT_RPS=50 && export GATEWAY_RATE_LIMIT_RPM=3000"

# 或临时禁用 CORS（紧急情况）
docker exec agentos-gateway sh -c \
  "export GATEWAY_CORS_MODE=prod && export GATEWAY_CORS_ORIGINS=''"

# 重启服务使配置生效
docker-compose restart gateway
```

#### 3. 深入分析

```bash
# 导出最近 1 小时日志
docker-compose logs --since 1h gateway > gateway_incident.log

# 分析高频 IP
cat gateway_incident.log | grep -oE "([0-9]{1,3}\.){3}[0-9]{1,3}" | \
  sort | uniq -c | sort -rn | head -20

# 查找错误模式
cat gateway_incident.log | grep ERROR | \
  awk -F'ERROR' '{print $2}' | sort | uniq -c | sort -rn
```

#### 4. 记录与报告

- 记录事件时间线
- 统计影响范围（请求数、用户数）
- 分析根本原因
- 制定改进措施
- 更新威胁模型和监控规则

---

## 🔬 安全审计

### 自动化审计工具

#### 静态分析

```bash
# 编译选项（启用所有警告）
cmake -DCMAKE_BUILD_TYPE=Debug ..
make CFLAGS="-Wall -Wextra -Wpedantic -Werror"

# 使用 cppcheck
make cppcheck

# 使用 clang-tidy
clang-tidy src/**/*.c -- -Iinclude
```

#### 依赖扫描

```bash
# 检查 cJSON 漏洞
# 访问 https://github.com/DaveGamble/cJSON/releases 查看最新版本

# 检查 libmicrohttpd 漏洞
# 访问 https://www.gnu.org/software/libmicrohttpd/

# 检查 libwebsockets 漏洞
# 访问 https://github.com/warmcat/libwebsockets/releases
```

### 手动审计要点

#### 代码审计

- [ ] 所有公共 API 的 NULL 检查
- [ ] 内存分配后的返回值检查
- [ ] 数组边界验证
- [ ] 整数溢出防护
- [ ] 并发访问的原子操作
- [ ] 资源释放的完整性

#### 配置审计

- [ ] 环境变量默认值安全性
- [ ] CORS 白名单合理性
- [ ] 速率限制阈值适当性
- [ ] 日志级别合规性

---

## 📚 参考资源

### 安全标准

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CWE/SANS Top 25](https://cwe.mitre.org/top25/)
- [NIST Cybersecurity Framework](https://www.nist.gov/cyberframework)

### 相关文档

- [Gateway 部署指南](deploy/README.md)
- [架构设计文档](../../docs/architecture/overview.md)
- [API 接口文档](README.md)

### 联系支持

- **安全漏洞报告**: security@spharx.io
- **GitHub Issues**: https://github.com/spharx/agentos/issues
- **社区论坛**: https://community.agentos.io

---

**© 2026 SPHARX Ltd. All Rights Reserved.**  
*"From data intelligence emerges."*

**最后更新**: 2026-04-05  
**下次审查**: 2026-07-05（季度审查）
