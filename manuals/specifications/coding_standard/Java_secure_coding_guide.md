# AgentOS (OpenApex) Java 安全编码指南

**版本**: Doc V1.6  
**备选名称**: OpenApex (开源极境 / 极境OS)  
**更新日期**: 2026-03-25  
**适用范围**: AgentOS Java SDK、所有 Java 语言实现  
**理论基础**: 工程两论（反馈闭环）、系统工程（层次分解）、五维正交系统（系统观、内核观、认知观、工程观、设计美学）、双系统认知理论、安全穹顶（cupolas）防御深度  
**原则映射**: D-1至D-4（安全工程）、S-2（模块化设计）、C-3（认知偏差防护）、E-1（安全内生）、E-8（可测试性）

---

## 一、概述

### 1.1 编制目的

本指南为 AgentOS 项目中的 Java 代码提供安全编码标准。基于项目架构设计原则的五维正交系统，本指南聚焦于安全观维度（D-1至D-4安全工程原则），为开发者提供防止安全漏洞的编码实践。

### 1.2 与 AgentOS 架构的关系

基于架构设计原则**E-1（安全内生原则）**，AgentOS 的安全穹顶（cupolas）采用四层防护体系：

| 层次 | 名称 | 组件路径 | 功能 | Java SDK 实现 |
|------|------|---------|------|--------------|
| D1 | **虚拟工位** | `cupolas/workbench/` | 进程/容器级隔离 | 沙箱 ClassLoader |
| D2 | **权限裁决** | `cupolas/permission/` | YAML 规则引擎 | SecurityManager |
| D3 | **输入净化** | `cupolas/sanitizer/` | 正则过滤 | InputValidator |
| D4 | **审计追踪** | `cupolas/audit/` | 全链路追踪 | AuditLogger |

**防护原则**:
- **纵深防御**: 四层防护相互独立，单层失效不影响整体安全（映射原则：D-3）
- **默认拒绝**: 未经明确允许的访问一律拒绝（映射原则：D-1）
- **失效安全**: 安全机制故障时默认进入安全状态（映射原则：D-4）

Java SDK 作为 AgentOS 的重要组成部分，必须遵循这些安全原则，并在代码层面实现相应的安全机制。

### 1.3 五维正交系统的安全视角

AgentOS的五维正交系统为Java安全编码提供了多层次的理论框架：

#### 1.3.1 系统观（System View）安全
- **垂直分层安全**：Java SDK需遵循S-1（垂直分层）原则，在应用层、服务层、内核层间建立清晰的信任边界
- **模块化安全设计**：基于S-2原则，安全功能模块化，支持独立部署和升级

#### 1.3.2 内核观（Kernel View）安全
- **微内核安全模型**：Java代码需与microkernel.md中定义的微内核安全原语保持一致
- **系统调用保护**：Java Native Interface（JNI）调用必须严格验证，防止非法跨越用户态-内核态边界

#### 1.3.3 认知观（Cognitive View）安全
- **双系统认知偏差防护**：基于C-3原则，Java API设计需考虑开发者认知模式，防止安全配置错误
- **安全代码审查**：利用System 2（慢速路径）深度分析能力进行安全代码审查

#### 1.3.4 工程观（Engineering View）安全
- **安全工程化**：将D-1至D-4安全工程原则转化为具体的Java编码模式
- **安全基础设施**：基于E-1至E-4原则，构建可观测、可审计、可恢复的Java安全基础设施

#### 1.3.5 设计美学（Design Aesthetics）安全
- **安全即美感**：安全的Java代码应是简洁、优雅、易于理解的（映射原则：A-1）
- **细节中的安全**：安全漏洞常隐藏在细节中，必须贯彻A-2（细节关注）原则

### 1.4 双系统认知理论与安全编程

AgentOS的双系统认知理论为Java安全编码提供了心理学基础：

#### 1.4.1 System 1（快速路径）安全
- **直觉安全设计**：常见安全操作应直观易懂，支持快速正确使用
- **默认安全行为**：API默认配置应是安全的，防止疏忽导致的安全漏洞
- **示例**：安全敏感的API应使安全用法成为最自然的选择

#### 1.4.2 System 2（慢速路径）安全
- **深度安全分析**：复杂安全决策需要System 2的深度分析能力
- **安全配置验证**：复杂安全配置需要明确的验证机制和文档
- **示例**：密钥管理系统需要详细的配置向导和安全检查

### 1.5 安全原则

基于架构设计原则 E-1（安全内生原则），本指南遵循以下安全原则：

| 原则 | 说明 | 在 AgentOS 中的体现 | 关联原则 |
|------|------|---------------------|---------|
| 最小权限 | 只授予完成任务所需的最小权限 | D2 权限裁决 | K-1 |
| 纵深防御 | 多层安全检查 | cupolas 四层防护 | S-2 |
| 输入验证 | 所有外部输入必须经过验证 | D3 输入净化 | E-1 |
| 安全默认值 | 默认配置应是安全的 | 默认拒绝策略 | E-1 |
| 故障安全 | 发生错误时默认拒绝操作 | 安全机制故障时拒绝访问 | E-6 |
| 可观测性 | 所有安全事件必须可追踪 | D4 审计追踪 | E-2 |

---

## 二、输入验证

### 2.1 通用输入验证

```java
/**
 * 验证输入参数
 *
 * @param taskId 任务 ID
 * @param input 输入数据
 * @throws IllegalArgumentException 当输入无效时
 */
public void processTask(String taskId, byte[] input) {
    // 1. 基本空值检查
    if (taskId == null) {
        throw new IllegalArgumentException("Task ID cannot be null");
    }
    
    // 2. 格式验证
    if (!TASK_ID_PATTERN.matcher(taskId).matches()) {
        throw new IllegalArgumentException("Invalid task ID format: " + taskId);
    }
    
    // 3. 长度验证
    if (taskId.length() > MAX_TASK_ID_LENGTH) {
        throw new IllegalArgumentException("Task ID too long: " + taskId.length());
    }
    
    // 4. 内容验证（二进制数据）
    if (input != null && input.length > MAX_INPUT_SIZE) {
        throw new IllegalArgumentException("Input too large: " + input.length);
    }
    
    // 继续处理...
}

private static final Pattern TASK_ID_PATTERN = Pattern.compile("^[a-zA-Z0-9_-]{1,64}$");
private static final int MAX_TASK_ID_LENGTH = 64;
private static final int MAX_INPUT_SIZE = 10 * 1024 * 1024; // 10MB
```

### 2.2 SQL 注入防护

```java
// 使用参数化查询
public List<Task> findTasksByStatus(String status) {
    // 正确：使用参数化查询
    String sql = "SELECT * FROM tasks WHERE status = ?";
    return jdbcTemplate.query(sql, (rs, rowNum) -> {
        Task task = new Task();
        task.setId(rs.getString("id"));
        task.setStatus(rs.getString("status"));
        return task;
    }, status);
}

// 错误：字符串拼接 SQL
// DO NOT: sql = "SELECT * FROM tasks WHERE status = '" + status + "'";
```

### 2.3 命令注入防护

```java
// 错误：直接执行用户输入
// DO NOT: Runtime.getRuntime().exec("grep " + userInput);

// 正确：使用 API 而非命令行
public List<String> searchLogs(String pattern) {
    // 使用专门的日志查询 API
    return logQueryApi.search(pattern, LogLevel.INFO);
}

// 如果必须执行命令，使用白名单
public void cleanupTempFiles(String directory) {
    // 白名单验证
    Path path = Paths.get(directory).normalize();
    if (!ALLOWED_CLEANUP_DIRS.contains(path.toString())) {
        throw new SecurityException("Directory not allowed: " + directory);
    }
    
    // 使用 FileVisitor 安全删除
    Files.walkFileTree(path, new SimpleFileVisitor<>() {
        @Override
        public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
            Files.delete(file);
            return FileVisitResult.CONTINUE;
        }
    });
}

private static final Set<String> ALLOWED_CLEANUP_DIRS = Set.of("/tmp/agentos", "/var/agentos/cleanup");
```

---

## 三、认证与授权

### 3.1 认证令牌处理

```java
public class AgentAuthenticator {
    private static final Logger logger = LoggerFactory.getLogger(AgentAuthenticator.class);
    
    /**
     * 验证认证令牌
     *
     * @param token 用户提供的令牌
     * @return 认证结果
     */
    public AuthenticationResult authenticate(String token) {
        if (token == null || token.isBlank()) {
            logger.warn("Empty authentication token received");
            return AuthenticationResult.failure("Token is required");
        }
        
        try {
            // 验证 JWT 格式和签名
            JwtParser parser = JwtParserBuilder.builder()
                .setSigningKey(loadSigningKey())
                .build();
            
            Jwt jwt = parser.parseClaimsJws(token);
            Claims claims = jwt.getBody();
            
            // 验证有效期
            Date expiration = claims.getExpiration();
            if (expiration == null || expiration.before(new Date())) {
                logger.warn("Expired token for subject: {}", claims.getSubject());
                return AuthenticationResult.failure("Token expired");
            }
            
            // 验证颁发者
            String issuer = claims.getIssuer();
            if (!EXPECTED_ISSUER.equals(issuer)) {
                logger.warn("Invalid token issuer: {}", issuer);
                return AuthenticationResult.failure("Invalid issuer");
            }
            
            return AuthenticationResult.success(claims.getSubject(), extractRoles(claims));
            
        } catch (JwtException e) {
            logger.warn("Invalid JWT token: {}", e.getMessage());
            return AuthenticationResult.failure("Invalid token");
        }
    }
    
    private Set<String> extractRoles(Claims claims) {
        @SuppressWarnings("unchecked")
        List<String> roles = claims.get("roles", List.class);
        return roles != null ? new HashSet<>(roles) : Collections.emptySet();
    }
    
    private Key loadSigningKey() {
        // 从安全存储加载密钥
        return KeyRepository.getInstance().getSigningKey();
    }
}
```

### 3.2 权限检查

```java
public class TaskExecutor {
    private final PermissionChecker permissionChecker;
    
    /**
     * 执行任务
     *
     * @param taskId 任务 ID
     * @param userContext 用户上下文
     * @throws SecurityException 当权限不足时
     */
    public void executeTask(String taskId, UserContext userContext) {
        // 1. 验证任务存在
        Task task = taskRepository.findById(taskId);
        if (task == null) {
            throw new IllegalArgumentException("Task not found: " + taskId);
        }
        
        // 2. 权限检查 - 遵循最小权限原则
        if (!permissionChecker.hasPermission(userContext, "task:execute", task.getOwner())) {
            logger.warn("Permission denied for user {} to execute task {}",
                userContext.getUserId(), taskId);
            throw new SecurityException("Permission denied: task:execute");
        }
        
        // 3. 资源配额检查
        if (!resourceQuotaManager.checkQuota(userContext.getUserId(), ResourceType.CPU, task.getRequiredCpu())) {
            throw new ResourceQuotaExceededException("CPU quota exceeded");
        }
        
        // 4. 执行任务...
    }
}
```

---

## 四、加密处理

### 4.1 密钥管理

```java
public class KeyManager {
    private static final String AGENTOS_KEY_ALGORITHM = "AES/GCM/NoPadding";
    private static final int GCM_IV_LENGTH = 12;
    private static final int GCM_TAG_LENGTH = 128;
    
    /**
     * 生成数据加密密钥
     *
     * @param purpose 密钥用途
     * @return DEK（数据加密密钥）
     */
    public SecretKey generateDataKey(String purpose) {
        KeyGenerator keyGen;
        try {
            keyGen = KeyGenerator.getInstance("AES");
            keyGen.init(256);
            return keyGen.generateKey();
        } catch (NoSuchAlgorithmException e) {
            throw new CryptoException("AES not supported", e);
        }
    }
    
    /**
     * 使用 KEK 加密 DEK
     */
    public byte[] wrapKey(SecretKey dek, String userId) {
        SecretKey kek = loadUserKEK(userId);
        Cipher cipher;
        try {
            cipher = Cipher.getInstance(AGENTOS_KEY_ALGORITHM);
            cipher.init(Cipher.WRAP_MODE, kek);
            return cipher.wrap(dek);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException | InvalidKeyException e) {
            throw new CryptoException("Failed to wrap key", e);
        }
    }
    
    /**
     * 安全存储加密密钥
     *
     * @param keyId 密钥标识
     * @param encryptedKey 加密后的密钥
     */
    public void storeEncryptedKey(String keyId, byte[] encryptedKey) {
        // 使用安全的密钥存储服务
        keyStorage.store(keyId, encryptedKey, 
            new KeyAttributes()
                .setOwner(currentUserId())
                .setCreatedAt(Instant.now())
                .setAlgorithm("AES-256-GCM")
        );
    }
}
```

### 4.2 数据加密解密

```java
public class DataEncryptor {
    private final KeyManager keyManager;
    
    /**
     * 加密数据
     *
     * @param plaintext 明文数据
     * @param keyId 密钥 ID
     * @return 加密后的数据（包含 IV 和密文）
     */
    public EncryptedData encrypt(byte[] plaintext, String keyId) {
        // 1. 生成随机 IV
        byte[] iv = new byte[GCM_IV_LENGTH];
        SecureRandom random = new SecureRandom();
        random.nextBytes(iv);
        
        // 2. 加载密钥
        SecretKey key = keyManager.loadKey(keyId);
        
        // 3. 加密
        Cipher cipher;
        try {
            cipher = Cipher.getInstance(AGENTOS_KEY_ALGORITHM);
            GCMParameterSpec gcmSpec = new GCMParameterSpec(GCM_TAG_LENGTH, iv);
            cipher.init(Cipher.ENCRYPT_MODE, key, gcmSpec);
            byte[] ciphertext = cipher.doFinal(plaintext);
            return new EncryptedData(iv, ciphertext);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException | 
                 InvalidKeyException | InvalidAlgorithmParameterException | 
                 IllegalBlockSizeException | BadPaddingException e) {
            throw new CryptoException("Encryption failed", e);
        }
    }
    
    /**
     * 解密数据
     */
    public byte[] decrypt(EncryptedData encrypted, String keyId) {
        SecretKey key = keyManager.loadKey(keyId);
        
        Cipher cipher;
        try {
            cipher = Cipher.getInstance(AGENTOS_KEY_ALGORITHM);
            GCMParameterSpec gcmSpec = new GCMParameterSpec(GCM_TAG_LENGTH, encrypted.getIv());
            cipher.init(Cipher.DECRYPT_MODE, key, gcmSpec);
            return cipher.doFinal(encrypted.getCiphertext());
        } catch (Exception e) {
            throw new CryptoException("Decryption failed", e);
        }
    }
}

public record EncryptedData(byte[] iv, byte[] ciphertext) {
    /**
     * 序列化格式：IV长度(4字节) + IV + 密文
     */
    public byte[] toBytes() {
        ByteBuffer buffer = ByteBuffer.allocate(4 + iv.length + ciphertext.length);
        buffer.putInt(iv.length);
        buffer.put(iv);
        buffer.put(ciphertext);
        return buffer.array();
    }
    
    public static EncryptedData fromBytes(byte[] data) {
        ByteBuffer buffer = ByteBuffer.wrap(data);
        int ivLength = buffer.getInt();
        byte[] iv = new byte[ivLength];
        byte[] ciphertext = new byte[buffer.remaining()];
        buffer.get(iv);
        buffer.get(ciphertext);
        return new EncryptedData(iv, ciphertext);
    }
}
```

---

## 五、安全日志

### 5.1 日志脱敏

```java
public class SafeLogger {
    private static final Pattern SENSITIVE_PATTERN = 
        Pattern.compile("(password|token|secret|key|credential)\\s*[:=]\\s*\\S+", 
                       Pattern.CASE_INSENSITIVE);
    
    /**
     * 记录安全相关事件
     */
    public void logSecurityEvent(SecurityEvent event) {
        // 1. 脱敏敏感字段
        String sanitizedMessage = sanitize(event.getMessage());
        
        // 2. 记录结构化日志
        log.info("SecurityEvent: type={}, userId={}, resource={}, result={}, timestamp={}",
            event.getType(),
            event.getUserId(),
            event.getResource(),
            event.getResult(),
            event.getTimestamp());
    }
    
    /**
     * 脱敏敏感信息
     */
    private String sanitize(String message) {
        if (message == null) return null;
        return SENSITIVE_PATTERN.matcher(message).replaceAll("$1=[REDACTED]");
    }
    
    /**
     * 不记录的内容
     */
    public void logTaskSubmit(TaskSubmitRequest request) {
        // DO NOT: log.info("Submitting task: {}", request); // 可能包含敏感数据
        
        // 正确：只记录必要信息
        log.info("Task submitted: id={}, type={}, priority={}",
            request.getTaskId(),
            request.getType(),
            request.getPriority());
    }
}

public record SecurityEvent(
    SecurityEventType type,
    String userId,
    String resource,
    SecurityResult result,
    Instant timestamp,
    String message
) {}

public enum SecurityEventType {
    AUTH_SUCCESS,
    AUTH_FAILURE,
    PERMISSION_DENIED,
    RESOURCE_ACCESS,
    KEY_OPERATION,
    CONFIG_CHANGE
}
```

### 5.2 审计日志

```java
public class AuditLogger {
    private final AsyncAuditQueue auditQueue;
    
    /**
     * 记录审计日志
     */
    public void audit(AuditRecord record) {
        // 1. 验证审计记录完整性
        if (!record.isValid()) {
            throw new IllegalArgumentException("Invalid audit record");
        }
        
        // 2. 异步写入，不阻塞主流程
        auditQueue.enqueue(record);
    }
    
    /**
     * 记录关键操作
     */
    public void auditKeyOperation(String operator, KeyOperation op, String keyId, boolean success) {
        audit(new AuditRecord.Builder()
            .operator(operator)
            .operation(op.name())
            .resource("key:" + keyId)
            .result(success ? "success" : "failure")
            .timestamp(Instant.now())
            .build());
    }
}

public class AuditRecord {
    private final String operator;
    private final String operation;
    private final String resource;
    private final String result;
    private final Instant timestamp;
    private final Map<String, String> metadata;
    
    public static class Builder {
        private String operator;
        private String operation;
        private String resource;
        private String result;
        private Instant timestamp = Instant.now();
        private Map<String, String> metadata = new HashMap<>();
        
        public Builder operator(String operator) {
            this.operator = Objects.requireNonNull(operator);
            return this;
        }
        
        public Builder operation(String operation) {
            this.operation = Objects.requireNonNull(operation);
            return this;
        }
        
        public Builder resource(String resource) {
            this.resource = Objects.requireNonNull(resource);
            return this;
        }
        
        public Builder result(String result) {
            this.result = Objects.requireNonNull(result);
            return this;
        }
        
        public Builder timestamp(Instant timestamp) {
            this.timestamp = Objects.requireNonNull(timestamp);
            return this;
        }
        
        public AuditRecord build() {
            return new AuditRecord(operator, operation, resource, result, timestamp, metadata);
        }
    }
    
    public boolean isValid() {
        return operator != null && !operator.isBlank()
            && operation != null && !operation.isBlank()
            && timestamp != null;
    }
}
```

---

## 六、错误处理

### 6.1 异常安全

```java
public class SafeTaskProcessor {
    private final ResourceCleanup cleanup;
    
    /**
     * 处理任务
     */
    public ProcessingResult processTask(Task task) {
        ResourceHandle handle = null;
        try {
            // 1. 资源获取
            handle = resourceManager.acquire(task.getRequiredResources());
            
            // 2. 处理任务
            ProcessingResult result = doProcess(task, handle);
            
            // 3. 成功时不额外记录，异常会在 finally 中统一处理
            return result;
            
        } catch (ResourceException e) {
            // 资源相关异常
            logger.warn("Resource error processing task {}: {}", task.getId(), e.getMessage());
            return ProcessingResult.resourceError(e.getMessage());
            
        } catch (ProcessingException e) {
            // 处理异常
            logger.error("Processing failed for task {}: {}", task.getId(), e.getMessage());
            return ProcessingResult.failure(e.getMessage());
            
        } finally {
            // 4. 无论如何都要释放资源
            if (handle != null) {
                try {
                    handle.release();
                } catch (ReleaseException e) {
                    // 记录释放异常，但不抛出，避免掩盖原始异常
                    logger.error("Failed to release resource for task {}: {}", 
                        task.getId(), e.getMessage());
                }
            }
        }
    }
}
```

### 6.2 信息泄露防护

```java
public class ErrorHandler {
    private static final Logger logger = LoggerFactory.getLogger(ErrorHandler.class);
    
    /**
     * 处理错误
     */
    public ErrorResponse handleError(String operation, Exception e) {
        // 1. 记录完整错误信息（内部）
        logger.error("Operation {} failed", operation, e);
        
        // 2. 返回脱敏的错误信息（外部）
        if (e instanceof SecurityException) {
            return ErrorResponse.of("security_error", "Security violation detected");
        }
        
        if (e instanceof ResourceQuotaException) {
            return ErrorResponse.of("quota_exceeded", "Resource quota exceeded");
        }
        
        if (e instanceof ValidationException) {
            return ErrorResponse.of("validation_error", "Input validation failed");
        }
        
        // 未知错误，返回通用消息
        return ErrorResponse.of("internal_error", "An internal error occurred");
    }
}

public record ErrorResponse(String code, String message) {
    // 禁止在错误响应中包含堆栈跟踪、SQL 错误、内部路径等
    
    public static ErrorResponse of(String code, String message) {
        return new ErrorResponse(code, message);
    }
    
    /**
     * 转换为 API 响应
     */
    public Map<String, Object> toApiResponse() {
        return Map.of(
            "success", false,
            "error", Map.of(
                "code", code,
                "message", message
            )
        );
    }
}
```

---

## 七、并发安全

### 7.1 线程安全集合

```java
public class TaskRegistry {
    // 使用线程安全的 ConcurrentHashMap
    private final ConcurrentHashMap<String, TaskState> tasks = new ConcurrentHashMap<>();
    
    /**
     * 注册任务
     */
    public void registerTask(String taskId, TaskState state) {
        tasks.put(taskId, state);
    }
    
    /**
     * 获取任务状态
     */
    public Optional<TaskState> getTaskState(String taskId) {
        return Optional.ofNullable(tasks.get(taskId));
    }
    
    /**
     * 安全更新任务状态
     */
    public boolean updateTaskState(String taskId, Function<TaskState, TaskState> updater) {
        return tasks.computeIfPresent(taskId, (id, current) -> updater.apply(current)) != null;
    }
    
    /**
     * 原子操作
     */
    public TaskState computeIfAbsent(String taskId, Supplier<TaskState> factory) {
        return tasks.computeIfAbsent(taskId, k -> {
            TaskState state = factory.get();
            logger.debug("Created new task state for: {}", taskId);
            return state;
        });
    }
}
```

### 7.2 同步控制

```java
public class SharedResourceManager {
    private final Map<String, ReadWriteLock> resourceLocks = new ConcurrentHashMap<>();
    private final Map<String, Resource> resources = new ConcurrentHashMap<>();
    
    /**
     * 读取资源（读锁）
     */
    public <T> T readResource(String resourceId, Function<Resource, T> reader) {
        ReadWriteLock lock = getOrCreateLock(resourceId);
        lock.readLock().lock();
        try {
            Resource resource = resources.get(resourceId);
            if (resource == null) {
                throw new ResourceNotFoundException(resourceId);
            }
            return reader.apply(resource);
        } finally {
            lock.readLock().unlock();
        }
    }
    
    /**
     * 修改资源（写锁）
     */
    public void writeResource(String resourceId, Consumer<Resource> writer) {
        ReadWriteLock lock = getOrCreateLock(resourceId);
        lock.writeLock().lock();
        try {
            Resource resource = resources.get(resourceId);
            if (resource == null) {
                throw new ResourceNotFoundException(resourceId);
            }
            writer.accept(resource);
        } finally {
            lock.writeLock().unlock();
        }
    }
    
    private ReadWriteLock getOrCreateLock(String resourceId) {
        return resourceLocks.computeIfAbsent(resourceId, k -> new ReentrantReadWriteLock());
    }
}
```

---

## 八、依赖安全

### 8.1 依赖管理

```java
// pom.xml - 使用 BOM 管理依赖版本
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>org.agentos</groupId>
            <artifactId>agentos-bom</artifactId>
            <version>${agentos.version}</version>
            <type>pom</type>
            <scope>import</scope>
        </dependency>
    </dependencies>
</dependencyManagement>

// 禁止使用已知漏洞的依赖
// 检查：mvn org.owasp:dependency-check-maven:check
```

### 8.2 安全配置

```java
public class SecurityConfig {
    /**
     * 创建 HttpSecurity 配置
     */
    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            // 禁用 iframe
            .headers(headers -> headers.frameOptions(frame -> frame.deny()))
            
            // CSRF 防护
            .csrf(csrf -> csrf
                .csrfTokenRepository(CookieCsrfTokenRepository.withHttpOnlyFalse())
                .ignoringAntMatchers("/api/public/**"))
            
            // 会话管理
            .sessionManagement(session -> session
                .sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            
            // 认证规则
            .authorizeHttpRequests(auth -> auth
                .antMatchers("/api/public/**").permitAll()
                .antMatchers("/api/admin/**").hasRole("ADMIN")
                .anyRequest().authenticated())
            
            // 异常处理
            .exceptionHandling(ex -> ex
                .authenticationEntryPoint((request, response, authException) -> {
                    response.setStatus(401);
                    response.setContentType("application/json");
                    response.getWriter().write("{\"error\":\"unauthorized\"}");
                })
                .accessDeniedHandler((request, response, accessDeniedException) -> {
                    response.setStatus(403);
                    response.setContentType("application/json");
                    response.getWriter().write("{\"error\":\"forbidden\"}");
                }))
            
            .build();
    }
}
```

---
## 十、AgentOS 模块 Java 安全编码示例

### 10.1 cupolas（安全穹顶）Java 安全实现
cupolas模块实现AgentOS的安全穹顶，Java SDK需要严格遵循其安全模型：

#### 10.1.1 虚拟工位（Virtual Workbench）沙箱
```java
/**
 * 虚拟工位沙箱 - 体现D-2（安全隔离）和S-1（垂直分层）原则
 * 
 * 基于ClassLoader隔离机制，为每个任务创建独立的执行环境。
 * 集成资源限制和权限控制，防止任务间相互影响。
 * 
 * @see cupolas/workbench/ 中的进程隔离原语
 */
public class VirtualWorkbenchSandbox {
    private final SandboxClassLoader classLoader;
    private final ResourceLimiter limiter;
    private final SecurityManager securityManager;
    
    /**
     * 在沙箱中执行任务 - 体现D-1（最小权限）原则
     * 
     * @param task 任务定义
     * @param context 安全上下文
     * @return 执行结果
     */
    public ExecutionResult executeInSandbox(Task task, SecurityContext context) {
        // 1. 创建隔离的ClassLoader
        classLoader = new SandboxClassLoader(
            task.getRequiredLibraries(),
            getSystemClassLoader()
        );
        
        // 2. 设置资源限制
        limiter = new ResourceLimiter(
            task.getCpuQuota(),
            task.getMemoryQuota(),
            task.getDiskQuota()
        );
        
        // 3. 配置安全管理器
        securityManager = new SandboxSecurityManager(
            context.getPermissions(),
            new AuditLogger()
        );
        
        // 4. 在受限环境中执行
        return doExecuteWithRestrictions(task);
    }
    
    private ExecutionResult doExecuteWithRestrictions(Task task) {
        Thread currentThread = Thread.currentThread();
        ClassLoader originalLoader = currentThread.getContextClassLoader();
        SecurityManager originalManager = System.getSecurityManager();
        
        try {
            // 切换ClassLoader
            currentThread.setContextClassLoader(classLoader);
            System.setSecurityManager(securityManager);
            
            // 应用资源限制
            limiter.apply();
            
            // 执行任务
            return task.execute();
            
        } catch (SecurityException e) {
            logAudit("Sandbox security violation: {}", e.getMessage());
            return ExecutionResult.failure("Security violation: " + e.getMessage());
            
        } finally {
            // 恢复原始环境
            currentThread.setContextClassLoader(originalLoader);
            System.setSecurityManager(originalManager);
            limiter.release();
        }
    }
}
```

#### 10.1.2 权限裁决（Permission Arbiter）
```java
/**
 * 权限裁决引擎 - 体现D-1（最小权限）和D-4（安全审计）原则
 * 
 * 基于YAML规则引擎实现细粒度访问控制。
 * 支持动态策略更新和实时审计。
 * 
 * @see cupolas/permission/ 中的规则引擎
 */
@Service
public class PermissionArbiter {
    private final RuleEngine ruleEngine;
    private final AuditTrail auditTrail;
    
    /**
     * 裁决访问请求 - 体现防御深度（D-3）原则
     * 
     * 多层验证：
     * 1. 主体身份验证
     * 2. 资源权限检查
     * 3. 上下文策略验证
     * 4. 风险评估
     */
    public AccessDecision decide(AccessRequest request) {
        // 层次1：主体验证
        if (!validateSubject(request.getSubject())) {
            logAudit("Invalid subject: {}", request.getSubject());
            return AccessDecision.deny("Invalid subject");
        }
        
        // 层次2：资源权限检查
        Permission required = request.getRequiredPermission();
        if (!hasPermission(request.getSubject(), required)) {
            logAudit("Permission denied: subject={}, resource={}, permission={}",
                request.getSubject(), request.getResource(), required);
            return AccessDecision.deny("Insufficient permission");
        }
        
        // 层次3：上下文策略验证
        Context context = request.getContext();
        if (!evaluateContextPolicy(context)) {
            logAudit("Context policy violation: {}", context);
            return AccessDecision.deny("Context policy violation");
        }
        
        // 层次4：风险评估
        RiskAssessment risk = assessRisk(request);
        if (risk.getLevel() == RiskLevel.HIGH) {
            logAudit("High risk access denied: risk={}", risk.getScore());
            return AccessDecision.deny("High risk access");
        }
        
        // 授予访问权限
        logAudit("Access granted: subject={}, resource={}", 
            request.getSubject(), request.getResource());
        return AccessDecision.grant(risk.getScore());
    }
}
```

### 10.2 daemon（守护层）Java 安全实现
Backs模块作为系统服务，需实现可靠的安全通信和资源管理：

#### 10.2.1 安全IPC客户端
```java
/**
 * 安全IPC客户端 - 体现E-3（通信基础设施）和D-3（纵深防御）原则
 * 
 * 实现与Atoms模块的安全进程间通信。
 * 集成消息加密、身份验证和完整性保护。
 * 
 * @see ipc.md 中的安全通信协议
 */
public class SecureIpcClient {
    private final CryptoService crypto;
    private final AuthService auth;
    private final IpcChannel channel;
    
    /**
     * 发送安全消息 - 体现防御深度原则
     */
    public SecureResponse sendSecure(SecureRequest request) {
        // 层次1：请求签名
        byte[] signature = crypto.sign(request.getPayload());
        request.setSignature(signature);
        
        // 层次2：消息加密
        EncryptedMessage encrypted = crypto.encrypt(
            request.toBytes(),
            getSessionKey()
        );
        
        // 层次3：身份验证
        AuthToken token = auth.getCurrentToken();
        if (!auth.isValid(token)) {
            throw new SecurityException("Authentication token expired");
        }
        
        // 层次4：发送并验证响应
        RawResponse raw = channel.send(encrypted);
        SecureResponse response = parseResponse(raw);
        
        // 验证响应完整性
        if (!crypto.verify(response.getSignature(), response.getPayload())) {
            throw new SecurityException("Response integrity check failed");
        }
        
        return response;
    }
}
```

### 10.3 bases（公共库层）Java 安全实现
Common模块提供跨层安全基础设施：

#### 10.3.1 统一密钥管理服务
```java
/**
 * 统一密钥管理服务 - 体现E-1（基础设施）和D-4（安全审计）原则
 * 
 * 提供密钥生成、存储、轮换和审计的完整生命周期管理。
 * 支持多租户隔离和硬件安全模块（HSM）集成。
 */
@Service
public class UnifiedKeyManager {
    private final KeyStorage keyStorage;
    private final KeyGenerator keyGenerator;
    private final AuditLogger auditLogger;
    
    /**
     * 生成并存储密钥 - 体现密钥生命周期管理
     */
    public KeyMetadata generateKey(KeySpec spec, String owner) {
        // 1. 生成密钥
        SecretKey key = keyGenerator.generate(spec);
        
        // 2. 加密存储
        byte[] encrypted = encryptKey(key, getMasterKey());
        String keyId = UUID.randomUUID().toString();
        
        // 3. 安全存储
        keyStorage.store(keyId, encrypted, new StorageOptions()
            .setEncryptionAlgorithm("AES-256-GCM")
            .setOwner(owner)
            .setCreatedAt(Instant.now())
        );
        
        // 4. 审计日志
        auditLogger.logKeyOperation(owner, KeyOperation.GENERATE, keyId, true);
        
        return new KeyMetadata(keyId, spec, owner);
    }
    
    /**
     * 密钥轮换策略 - 体现E-2（可维护性）原则
     */
    @Scheduled(fixedRate = 30 * 24 * 60 * 60 * 1000) // 每月轮换
    public void rotateExpiringKeys() {
        List<KeyMetadata> expiring = keyStorage.findKeysExpiringSoon();
        for (KeyMetadata metadata : expiring) {
            try {
                // 生成新密钥
                KeyMetadata newKey = generateKey(metadata.getSpec(), metadata.getOwner());
                
                // 迁移数据
                migrateData(metadata.getKeyId(), newKey.getKeyId());
                
                // 标记旧密钥为已轮换
                keyStorage.markAsRotated(metadata.getKeyId(), newKey.getKeyId());
                
                auditLogger.logKeyOperation("system", KeyOperation.ROTATE, 
                    metadata.getKeyId(), true);
                
            } catch (Exception e) {
                auditLogger.logKeyOperation("system", KeyOperation.ROTATE, 
                    metadata.getKeyId(), false);
                logger.error("Failed to rotate key: {}", metadata.getKeyId(), e);
            }
        }
    }
}
```

### 10.4 Atoms（原子层）Java Native Interface (JNI) 安全
Atoms模块通过JNI提供原生接口，需要特殊的安全考虑：

#### 10.4.1 安全JNI绑定
```java
/**
 * 安全JNI绑定 - 体现K-1（最小特权）和D-2（安全隔离）原则
 * 
 * 封装与Atoms微内核的本地调用，实现严格参数验证和边界检查。
 * 防止缓冲区溢出和类型混淆攻击。
 */
public class SecureKernelBinding {
    static {
        // 加载本地库
        System.loadLibrary("agentos_kernel");
    }
    
    // 本地方法声明
    private native long nativeAllocateMemory(long size, int numaNode);
    private native int nativeScheduleTask(long taskHandle, int priority);
    
    /**
     * 安全内存分配 - 体现M-3（拓扑优化）和防御深度原则
     */
    public MemoryHandle allocateMemory(long size, int numaNode) {
        // 层次1：参数验证
        if (size <= 0 || size > MAX_ALLOC_SIZE) {
            throw new IllegalArgumentException("Invalid allocation size: " + size);
        }
        
        // 层次2：NUMA节点验证
        if (numaNode < -1 || numaNode >= getNumaNodeCount()) {
            throw new IllegalArgumentException("Invalid NUMA node: " + numaNode);
        }
        
        // 层次3：权限检查
        if (!checkMemoryPermission(size)) {
            throw new SecurityException("Insufficient memory permission");
        }
        
        // 调用本地方法
        long nativeHandle = nativeAllocateMemory(size, numaNode);
        if (nativeHandle == 0) {
            throw new RuntimeException("Memory allocation failed");
        }
        
        return new MemoryHandle(nativeHandle, size);
    }
    
    /**
     * 清理本地资源 - 体现资源确定性原则
     */
    @Override
    protected void finalize() throws Throwable {
        try {
            releaseNativeResources();
        } finally {
            super.finalize();
        }
    }
}
```

---
## 十一、参考文献

1. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
2. **OWASP Top 10**: https://owasp.org/www-project-top-ten/
3. **Java Secure Coding Guidelines**: https://wiki.sei.cmu.edu/confluence/display/java
4. **Spring Security Documentation**: https://docs.spring.io/spring-security/site/docs/current/reference/html5/
5. **AgentOS 核心架构文档**:
   - [coreloopthree.md](../../architecture/folder/coreloopthree.md)
   - [memoryrovol.md](../../architecture/folder/memoryrovol.md)
   - [microkernel.md](../../architecture/folder/microkernel.md)
   - [ipc.md](../../architecture/folder/ipc.md)
   - [syscall.md](../../architecture/folder/syscall.md)
   - [logging_system.md](../../architecture/folder/logging_system.md)
   - [Security_design_guide.md](../Security_design_guide.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."