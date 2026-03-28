# AgentOS (OpenApex) C&C++安全编程指南

## 版本信息

**版本**: Doc V1.6  
**备选名称**: OpenApex (开源极境 / 极境OS)  
**更新日期**: 2026-03-25  
**适用范围**：AgentOS 所有 C/C++代码开发活动  
**理论基础**：工程控制论（信任边界、防御深度）、系统工程（层次分解）、五维正交系统（系统观、内核观、认知观、工程观、设计美学）、双系统认知理论  
**关联规范**：[架构设计原则](../../architecture/folder/architectural_design_principles.md)（映射原则：D-1至D-4安全工程，S-2模块化设计）  
**原则映射**: D-1（最小权限）、D-2（安全隔离）、D-3（纵深防御）、D-4（安全审计）、C-3（认知偏差防护）

---

## 序言：从系统工程视角看安全编程

### 0.1 安全编程的控制论解释

传统安全编程规范往往是一系列孤立的"禁止"和"必须"条款。本文档基于**工程控制论**和**系统工程**思想，将安全编程重新定义为：

**构建和维护系统的信任边界，通过多层防御机制实现失效安全的控制过程。**

- **信任边界**：系统中可信组件与不可信组件间的逻辑分界面
- **防御深度**：不依赖单一防护措施，而是建立多层防护体系
- **反馈闭环**：每个关键操作都应有验证和错误处理机制
- **失效安全**：当系统发生故障时，自动进入安全状态

### 0.2 五维正交系统的安全视角

AgentOS的五维正交系统为安全编程提供了多层次的认知框架：

#### 0.2.1 系统观（System View）安全
- **架构级信任边界**：基于S-1（垂直分层）和S-2（模块化设计）原则，在Atoms、daemon、cupolas、Common四层间建立清晰的信任边界
- **安全域隔离**：cupolas（安全域）层实现零信任架构，严格隔离不同安全级别的计算环境（映射原则：D-2安全隔离）

#### 0.2.2 内核观（Kernel View）安全  
- **微内核安全模型**：遵循microkernel.md中的最小特权原则，内核仅提供最基础的安全原语（映射原则：D-1最小权限）
- **系统调用保护**：syscall.md中定义的受控接口，防止非法跨越用户态-内核态边界

#### 0.2.3 认知观（Cognitive View）安全
- **双系统认知偏差防护**：基于C-3原则，System 1（快速路径）和System 2（慢速路径）都需要内置安全校验机制
- **人为因素考量**：安全机制设计需考虑开发者认知负荷，避免因复杂度导致的安全配置错误

#### 0.2.4 工程观（Engineering View）安全
- **防御深度工程化**：将D-3（纵深防御）原则转化为具体的代码实现模式
- **安全基础设施**：基于E-1至E-4原则，构建可观测、可审计、可恢复的安全基础设施

#### 0.2.5 设计美学（Design Aesthetics）安全
- **安全即美感**：安全的代码应是简洁、清晰、优雅的（映射原则：A-1极简主义）
- **细节中的安全**：安全漏洞常隐藏在细节中，必须贯彻A-2（细节关注）原则

### 0.3 双系统认知理论与安全编程

AgentOS的双系统认知理论为安全编程提供了心理学基础：

#### 0.3.1 System 1（快速路径）安全
- **模式匹配安全**：快速路径依赖直觉和模式匹配，必须确保常见模式的安全性
- **默认安全行为**：快速决策时应有安全的默认行为，防止疏忽导致的安全漏洞
- **示例**：API设计应使安全用法成为最自然、最直观的选择

#### 0.3.2 System 2（慢速路径）安全  
- **深度安全分析**：复杂安全决策需要System 2的深度分析能力
- **安全代码审查**：代码审查是System 2认知过程，需系统化的检查清单和方法
- **示例**：安全关键算法实现需要详细的注释和安全论证

#### 0.3.3 双系统协同安全
- **分层安全检查**：System 1处理常见、简单的安全检查，System 2处理复杂、罕见的安全威胁
- **认知负荷管理**：合理分配安全责任，避免System 1过载或System 2疲劳

### 0.4 本规范的结构

本规范按"数据流 + 生命周期"两个维度组织：

```
第 1 部分 基础理论
    └─ 信任边界理论、防御深度策略、失效安全原则
    
第 2 部分 输入验证与数据流控制（横向：数据从外部到内部）
    ├─ 外部数据校验
    ├─ 类型安全与转换
    └─ 容器与索引安全
    
第 3 部分 资源管理与生命周期控制（纵向：对象从创建到销毁）
    ├─ 内存资源管理
    ├─ 文件与句柄管理
    ├─ 对象生命周期
    └─ Lambda 与闭包安全
    
第 4 部分 异常处理与故障恢复
    ├─ 错误检测机制
    ├─ 故障隔离策略
    └─ 系统恢复方法
    
第 5 部分 内核安全编程（特权环境特殊要求）
    └─ 内核态与用户态交互安全
    
附录：系统性风险模式库
```

### 0.5 如何使用本规范

**对于架构师**：
- 理解信任边界划分原则，设计合理的模块接口
- 制定防御深度策略，确定哪些层次需要何种检查

**对于开发者**：
- 在编码时遵循对应章节的具体规则
- 理解每条规则背后的"为什么"（控制论解释）

**对于代码评审者**：
- 检查信任边界是否清晰
- 验证防御措施是否多层次
- 确认失效场景是否有安全处理

---

## 第 1 部分 基础理论

### 1.1 信任边界理论

#### 1.1.1 什么是信任边界

**定义**：信任边界是系统中不同安全级别组件间的逻辑分界面。边界一侧的组件默认被认为是可信的，另一侧则被视为不可信的。

**典型信任边界**：

```
外部世界 │ 边界防护 │ 内部系统
─────────┼──────────┼─────────
网络数据 │ 防火墙   │ 应用逻辑
用户输入 │ 参数校验 │ 业务处理
文件系统 │ 权限检查 │ 数据解析
IPC 通信  │ 接口验证 │ 服务实现
```

#### 1.1.2 信任边界的控制论意义

从控制论角度看，信任边界是**负反馈调节的关键点**：

1. **偏差检测**：在边界处检查输入是否偏离预期
2. **纠正措施**：拒绝非法输入或进行安全转换
3. **反馈回路**：记录边界事件用于系统优化

#### 1.1.3 信任边界划分原则

**原则 1-1【最小信任】**：默认不信任任何跨越边界的数据，除非经过验证。

**原则 1-2【边界清晰】**：每个模块都应明确定义其信任边界，通过接口文档说明。

**原则 1-3【逐级验证】**：数据每跨越一个信任边界，都需要重新验证。

**应用示例**：

```cpp
// 错误示例：信任边界模糊
void ProcessData(const char* data) {
    // 直接使用 data，未验证来源
    parse(data);
}

// 正确示例：明确信任边界
void ProcessData_External(const char* external_data) {
    // 外部数据，严格验证
    if (!ValidateExternalData(external_data)) {
        return;  // 拒绝非法输入
    }
    
    // 验证后转换为内部可信格式
    auto internal_data = ConvertToInternalFormat(external_data);
    
    // 内部数据处理，可适度降低验证强度
    ProcessData_Internal(internal_data);
}

void ProcessData_Internal(const InternalData& data) {
    // 假设数据已在边界处验证
    parse(data);
}
```

### 1.2 防御深度策略

#### 1.2.1 什么是防御深度

**定义**：防御深度（Defense in Depth）是指通过多层防护机制保护系统安全，即使某一层防护失效，其他层仍能提供保护。

#### 1.2.2 防御深度的层次模型

```
层次 5：审计与响应（检测入侵并响应）
    ↑
层次 4：访问控制（限制已认证用户的操作）
    ↑
层次 3：身份认证（验证用户身份）
    ↑
层次 2：输入验证（过滤恶意输入）
    ↑
层次 1：代码安全（无缓冲区溢出等漏洞）
```

#### 1.2.3 防御深度的控制论解释

防御深度本质上是**多重负反馈回路**：

- 每一层都是一个独立的调节器
- 多层叠加提高系统鲁棒性
- 层间信息反馈优化整体防御

**原则 1-4【纵深防御】**：关键安全属性必须由至少两层独立机制保护。

**示例：防止缓冲区溢出**

```cpp
// 层次 1：编译器保护（-fstack-protector）
// 层次 2：使用安全 API（strncpy而非 strcpy）
// 层次 3：运行时检查（数组边界验证）
// 层次 4：ASLR（地址空间随机化）

void SafeStringCopy(char* dest, const char* src, size_t dest_size) {
    // 层次 3：运行时检查
    if (dest == nullptr || src == nullptr || dest_size == 0) {
        return;
    }
    
    // 层次 2：使用安全 API
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';  // 确保 null 终止
}
```

### 1.3 失效安全原则

#### 1.3.1 什么是失效安全

**定义**：失效安全（Fail-Safe）是指系统在发生故障时，能够自动进入预定义的安全状态，而不是进入危险或不可预测的状态。

#### 1.3.2 失效安全的实现策略

**策略 1：默认拒绝**

```cpp
// 错误示例：默认允许
bool CheckPermission(User* user, Resource* res) {
    if (user->role == ADMIN) return true;
    if (user->role == MANAGER) return true;
    // 忘记检查普通用户，默认允许
    return true;  // 危险！
}

// 正确示例：默认拒绝
bool CheckPermission_Safe(User* user, Resource* res) {
    if (user == nullptr || res == nullptr) {
        return false;  // 参数异常，默认拒绝
    }
    
    if (user->role == ADMIN) return true;
    if (user->role == MANAGER) return true;
    
    return false;  // 未明确允许，默认拒绝
}
```

**策略 2：异常安全**

```cpp
// 错误示例：异常导致资源泄漏
void ProcessFile(const char* filename) {
    FILE* fp = fopen(filename, "r");
    Data* data = new Data();
    
    ParseFile(fp, data);  // 可能抛出异常
    // 如果上面抛出异常，fp 和 data 都未释放
    
    fclose(fp);
    delete data;
}

// 正确示例：RAII 保证异常安全
void ProcessFile_Safe(const char* filename) {
    std::ifstream file(filename);  // RAII 封装
    auto data = std::make_unique<Data>();  // 智能指针
    
    ParseFile(file, data.get());  // 即使抛出异常也会自动清理
    // file 自动关闭，data 自动释放
}
```

**策略 3：事务回滚**

```cpp
// 错误示例：部分成功导致不一致
void UpdateBalance(Account& acc, double amount) {
    acc.Debit(amount);   // 扣款成功
    LogTransaction(...); // 日志失败（如磁盘满）
    // 结果：钱扣了但没记录，数据不一致
}

// 正确示例：事务保证一致性
void UpdateBalance_Transaction(Account& acc, double amount) {
    BeginTransaction();
    try {
        acc.Debit(amount);
        LogTransaction(...);
        CommitTransaction();  // 全部成功才提交
    } catch (...) {
        RollbackTransaction();  // 任一失败都回滚
        throw;
    }
}
```

---

## 第 2 部分 输入验证与数据流控制

### 2.1 外部数据校验

#### 规则 2-1【边界校验】：对所有跨越信任边界的外部数据进行合法性校验

**控制论解释**：这是信任边界处的负反馈调节器，检测并纠正偏差。

**外部数据来源包括但不限于**：
- 网络数据（socket、HTTP 请求等）
- 用户输入（命令行、GUI 输入等）
- 文件系统（配置文件、用户上传文件等）
- 环境变量
- IPC 通信（管道、消息队列、共享内存等）
- API 参数
- 全局变量（可能被其他模块修改）

**校验内容包括但不限于**：
- 数据类型和格式
- 数据长度和范围
- 字符集和白名单验证
- 业务规则约束

**【反例】**
```cpp
void Foo(const unsigned char* buffer, size_t len) {
    // buffer 可能为空指针，不保证以'\0'结尾
    const char* s = reinterpret_cast<const char*>(buffer);
    size_t nameLen = strlen(s);  // 可能越界读取
    std::string name(s, nameLen);
    Foo2(name);
}
```

**【正例】**
```cpp
void Foo(const unsigned char* buffer, size_t len) {
    // 必须做参数合法性校验
    if (buffer == nullptr || len == 0 || len >= MAX_BUFFER_LEN) {
        return;  // 错误处理
    }

    const char* s = reinterpret_cast<const char*>(buffer);
    size_t nameLen = strnlen(s, len);  // 使用 strnlen 缓解读越界风险
    if (nameLen == len) {
        return;  // 没有找到结束符，错误处理
    }
    std::string name(s, nameLen);
    Foo2(name);
}
```

#### 规则 2-2【数组索引校验】：外部数据作为数组索引时必须校验范围

**【反例】**
```cpp
struct Dev {
    int id;
    char name[MAX_NAME_LEN];
};

static Dev devs[DEV_NUM];

int SetDevId(size_t index, int id) {
    if (index > DEV_NUM) {  // 差一错误
        return -1;
    }
    devs[index].id = id;  // index == DEV_NUM 时越界
    return 0;
}
```

**【正例】**
```cpp
int SetDevId_Safe(size_t index, int id) {
    if (index >= DEV_NUM) {  // 正确的边界检查
        return -1;
    }
    devs[index].id = id;
    return 0;
}
```

### 2.2 整数运算安全

#### 规则 2-3【有符号整数防溢出】：确保有符号整数运算不溢出

**控制论解释**：整数溢出会破坏系统的数值稳定性，必须在运算前进行范围调节。

**【反例】**
```cpp
unsigned char* content = ...;
size_t contentSize = ...;
int totalLen = ...;      // 外部数据
int skipLen = ...;       // 外部数据

std::vector<unsigned char> dest;
// totalLen - skipLen 可能整数溢出
std::copy_n(&content[skipLen], totalLen - skipLen, std::back_inserter(dest));
```

**【正例】**
```cpp
unsigned char* content = ...;
size_t contentSize = ...;
size_t totalLen = ...;   // 使用 size_t
size_t skipLen = ...;

if (skipLen >= totalLen || totalLen > contentSize) {
    return;  // 错误处理
}

std::vector<unsigned char> dest;
std::copy_n(&content[skipLen], totalLen - skipLen, std::back_inserter(dest));
```

#### 规则 2-4【无符号整数防回绕】：确保无符号整数运算不回绕

**【反例】**
```cpp
size_t totalLen = ...;
size_t readLen = 0;
size_t pktLen = ParsePktLen();  // 外部数据

if (readLen + pktLen > totalLen) {  // 加法可能回绕
    return;  // 错误处理
}
```

**【正例】**
```cpp
size_t totalLen = ...;
size_t readLen = 0;
size_t pktLen = ParsePktLen();

if (pktLen > totalLen - readLen) {  // 改为减法，避免回绕
    return;  // 错误处理
}
```

#### 规则 2-5【除零检查】：确保除法和余数运算不会导致除零错误

**【正例】**
```cpp
size_t a = ReadSize();  // 外部数据
if (a == 0) {
    return;  // 错误处理
}
size_t b = 1000 / a;
size_t c = 1000 % a;
```

### 2.3 类型安全

#### 规则 2-6【避免 reinterpret_cast】：避免使用 reinterpret_cast 进行类型转换

**解释**：`reinterpret_cast` 破坏类型安全性，是不安全的转换。

#### 规则 2-7【避免 const_cast】：避免使用 const_cast 移除 const 属性

**解释**：修改 const 对象会导致未定义行为。

### 2.4 容器与迭代器安全

#### 规则 2-8【容器索引校验】：外部数据用于容器索引时必须确保在有效范围内

**【正例】**
```cpp
std::vector<char> c{'A', 'B', 'C', 'D'};
int index = GetExternalIndex();

// 方法 1：使用 at() 自动检查边界
try {
    std::cout << c.at(index) << std::endl;
} catch (const std::out_of_range& e) {
    // 越界异常处理
}

// 方法 2：手动检查边界
if (index < 0 || static_cast<size_t>(index) >= c.size()) {
    return;  // 错误处理
}
std::cout << c[index] << std::endl;
```

---

## 第 3 部分 资源管理与生命周期控制

### 3.1 内存资源管理

#### 规则 3-1【内存申请校验】：内存申请前必须对申请大小进行合法性校验

**控制论解释**：防止资源耗尽攻击，是系统稳定性的负反馈保护。

**【反例】**
```cpp
int DoSomething(size_t size) {
    char* buffer = new char[size];  // size 未校验
    // ...
    delete[] buffer;
}
```

**【正例】**
```cpp
int DoSomething_Safe(size_t size) {
    if (size == 0 || size > FOO_MAX_LEN) {
        return -1;  // 错误处理
    }
    char* buffer = new char[size];
    // ...
    delete[] buffer;
}
```

#### 规则 3-2【new/delete 配对使用】：new 和 delete 配对使用，new[]和 delete[]配对使用

#### 规则 3-3【敏感信息清理】：内存中的敏感信息使用完毕后立即清 0

**控制论解释**：及时清除敏感信息是减少系统"熵增"的必要措施。

**【正例】**
```cpp
char password[MAX_PWD_LEN];
if (!GetPassword(password, sizeof(password))) {
    return;
}
if (!VerifyPassword(password)) {
    return;
}

// 使用完毕后立即清零
(void)memset(password, 0, sizeof(password));
```

**注意**：防止编译器优化使清零代码无效：
```cpp
// 某些编译器可能优化掉 memset，可使用以下方法防止
volatile char* pwd_ptr = password;
for (size_t i = 0; i < sizeof(password); ++i) {
    pwd_ptr[i] = 0;
}
```

### 3.2 对象生命周期管理

#### 规则 3-4【成员变量初始化】：类的成员变量必须显式初始化

**【正例】**
```cpp
class Message {
public:
    void Process() { /* ... */ }

private:
    uint32_t msgId{0};
    size_t msgLength{0};
    unsigned char* msgBuffer{nullptr};
    std::string someIdentifier;  // 具有默认构造函数，不需显式初始化
};
```

#### 规则 3-5【三/五/零法则】：明确需要实现哪些特殊成员函数

**三之法则**：若需要析构函数、拷贝构造函数、拷贝赋值操作符之一，则需要全部三个。

**五之法则**：若需要移动语义，则需要五个特殊成员函数（三法则 + 移动构造 + 移动赋值）。

**零之法则**：如果不需要专门处理资源所有权，则不应该有自定义的特殊成员函数。

#### 规则 3-6【虚析构函数】：通过基类指针释放派生类时，基类析构函数必须为虚函数

**【反例】**
```cpp
class Base {
public:
    ~Base() { }  // 非虚析构函数
    virtual std::string GetVersion() = 0;
};

class Derived : public Base {
public:
    ~Derived() override { 
        delete[] numbers;  // 可能不会被调用
    }
private:
    int* numbers;
};

void Foo() {
    Base* base = new Derived();
    delete base;  // 只调用 Base 的析构函数，造成资源泄漏
}
```

**【正例】**
```cpp
class Base {
public:
    virtual ~Base() { }  // 虚析构函数
    virtual std::string GetVersion() = 0;
};
```

#### 规则 3-7【避免切片】：对象赋值或初始化避免切片操作

**【正例】**
```cpp
// 使用指针或引用避免切片
void Foo(const Base& base) {
    base.Fun();  // 多态调用
}

// 如需切片，显式表达意图
Base SliceToBase(const Derived& d) {
    return Base(d);  // 显式构造
}
```

### 3.3 Lambda 与闭包安全

#### 规则 3-8【Lambda 捕获】：当 lambda 会逃逸出函数外面时，禁止按引用捕获局部变量

**【反例】**
```cpp
void Foo() {
    int local = 0;
    threadPool.QueueWork([&] { Process(local); });  // 按引用捕获，local 可能失效
}
```

**【正例】**
```cpp
void Foo() {
    int local = 0;
    threadPool.QueueWork([local] { Process(local); });  // 按值捕获，安全
}
```

---

## 第 4 部分 异常处理与故障恢复

### 4.1 异常抛出规范

#### 规则 4-1【抛对象本身】：抛异常时，抛对象本身而不是指针

**【反例】**
```cpp
throw new SomeException("error");  // 抛指针，回收责任不明确
```

**【正例】**
```cpp
throw SomeException("error");  // 抛对象本身
```

#### 规则 4-2【禁止从析构函数抛异常】：禁止从析构函数中抛出异常

**解释**：析构函数默认 noexcept，抛异常会导致 std::terminate。

### 4.2 格式化字符串安全

#### 规则 4-3【禁止外部可控格式串】：调用格式化函数时，format 参数禁止受外部数据控制

**【反例】**
```cpp
std::string msg = GetMsg();
syslog(priority, msg.c_str());  // 格式化字符串漏洞
```

**【正例】**
```cpp
std::string msg = GetMsg();
syslog(priority, "%s", msg.c_str());  // 使用%s 转换符
```

### 4.3 命令注入防护

#### 规则 4-4【禁止外部数据作为进程启动参数】：禁止外部可控数据作为 system/exec 等函数的参数

**【反例】**
```cpp
std::string cmd = GetCmdFromRemote();
system(cmd.c_str());  // 命令注入漏洞
```

**【正例】**
```cpp
// 优先使用库函数实现功能
bool WriteDataToFile(const std::string& dstFilePath, const std::string& srcFilePath) {
    std::ifstream srcFile(srcFilePath, std::ios::binary);
    std::ofstream dstFile(dstFilePath, std::ios::binary);
    // 直接文件操作，不调用系统命令
}

// 如必须调用命令，使用 exec 系列并参数化
pid_t pid = fork();
if (pid == 0) {
    execle("/bin/some_tool", "some_tool", fileName.c_str(), nullptr, envp);
    _Exit(-1);
}
```

---

## 第 5 部分 内核安全编程

### 5.1 内核 mmap 安全

#### 规则 5-1【mmap 参数校验】：内核 mmap 接口实现中，确保对映射起始地址和大小进行合法性校验

**【错误代码】**
```c
static int incorrect_mmap(struct file *file, struct vm_area_struct *vma) {
    unsigned long size = vma->vm_end - vma->vm_start;
    // 错误：未对映射起始地址、空间大小做合法性校验
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
        return EFAULT;
    }
    return EOK;
}
```

**【正确代码】**
```c
static int correct_mmap(struct file *file, struct vm_area_struct *vma) {
    unsigned long size = vma->vm_end - vma->vm_start;
    // 添加校验函数，验证映射起始地址、空间大小是否合法
    if (!valid_mmap_phys_addr_range(vma->vm_pgoff, size)) {
        return EINVAL;
    }
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
        return EFAULT;
    }
    return EOK;
}
```

### 5.2 内核态与用户态交互

#### 规则 5-2【使用专用函数】：内核程序中必须使用内核专用函数读写用户态缓冲区

**禁用函数列表**：memcpy()、bcopy()、memmove()、strcpy()、strncpy()、strcat()、strncat()、sprintf()、vsprintf()、snprintf()、vsnprintf()、sscanf()、vsscanf()

**【错误代码】**
```c
ssize_t incorrect_show(struct file *file, char __user *buf, size_t size, loff_t *data) {
    // 错误：直接引用用户态传入指针，如果 buf 为 NULL 则空指针异常
    return snprintf(buf, size, "%ld\n", debug_level);
}
```

**【正确代码】**
```c
ssize_t correct_show(struct file *file, char __user *buf, size_t size, loff_t *data) {
    int ret = 0;
    char level_str[MAX_STR_LEN] = {0};
    snprintf(level_str, MAX_STR_LEN, "%ld \n", debug_level);
    if(strlen(level_str) >= size) {
        return EFAULT;
    }
    // 使用专用函数 copy_to_user()
    ret = copy_to_user(buf, level_str, strlen(level_str)+1);
    return ret;
}
```

#### 规则 5-3【copy_from_user 长度校验】：必须对 copy_from_user() 拷贝长度进行校验

#### 规则 5-4【copy_to_user 初始化】：必须对 copy_to_user() 拷贝的数据进行初始化

### 5.3 内核异常处理

#### 规则 5-5【禁用 BUG_ON】：禁止在异常处理中使用 BUG_ON 宏

**【正确代码】**
```c
static unsigned int is_modem_set_timer_busy(special_timer *smem_ptr) {
    if (smem_ptr == NULL) {
        printk(KERN_EMERG"%s:smem_ptr NULL!\n", __FUNCTION__);
        return 1;  // 返回错误码，不使用 BUG_ON
    }
    // ...
}
```

#### 规则 5-6【禁止休眠函数】：在中断处理程序或持有自旋锁的进程上下文代码中，禁止使用会引起进程休眠的函数

#### 规则 5-7【防止栈溢出】：合理使用内核栈，防止内核栈溢出

---

## 附录 A：系统性风险模式库

### A.1 缓冲区溢出模式

**模式描述**：向固定大小缓冲区写入超出其容量的数据

**触发条件**：
- 未校验输入长度
- 使用不安全的字符串函数（strcpy, sprintf 等）
- 数组索引越界

**防护措施**：
- 层次 1：使用安全 API（strncpy, snprintf）
- 层次 2：显式边界检查
- 层次 3：编译器保护（-fstack-protector）
- 层次 4：运行时保护（ASLR, DEP）

### A.2 整数溢出模式

**模式描述**：整数运算结果超出类型表示范围

**触发条件**：
- 外部数据参与算术运算
- 未检查运算结果范围
- 混合使用有符号和无符号类型

**防护措施**：
- 层次 1：输入数据范围验证
- 层次 2：使用更宽类型（size_t 代替 int）
- 层次 3：运算前检查边界条件
- 层次 4：使用安全数学库

### A.3 使用后释放模式（Use-After-Free）

**模式描述**：访问已释放的内存

**触发条件**：
- 指针释放后未置nullptr
- 多个指针共享同一内存
- 异常导致提前退出

**防护措施**：
- 层次 1：释放后立即置 nullptr
- 层次 2：使用智能指针（unique_ptr, shared_ptr）
- 层次 3：RAII 资源管理
- 层次 4：静态分析工具检测

### A.4 命令注入模式

**模式描述**：外部数据被当作命令执行

**触发条件**：
- system()/popen() 使用外部数据
- shell 解释执行拼接的命令串

**防护措施**：
- 层次 1：避免使用 system()
- 层次 2：使用 exec 系列参数化调用
- 层次 3：白名单验证命令
- 层次 4：最小权限原则

---

## 附录 B：与其他规范的引用关系

| 引用文档 | 关系说明 |
|---------|---------|
| [架构设计原则](../../architecture/folder/architectural_design_principles.md) | 本规范是安全工程原则（D-1至D-4）在C/C++编码层面的具体落实，同时体现S-2（模块化设计）和C-3（认知偏差防护）原则 |
| [代码门禁要求](../code_gate_requirements_v2.md) | 本规范的要求通过代码门禁的CodeCheck进行自动化检查 |
| [日志打印规范](./Log_guide.md) | 错误处理和异常记录应遵循日志规范，支持OpenTelemetry集成 |
| [统一术语表](../TERMINOLOGY.md) | 本规范使用的术语定义和解释 |
| [核心架构文档](../../architecture/folder/) | 与本规范密切相关的架构文档：<br>- microkernel.md（内核安全编程）<br>- ipc.md（进程间通信安全）<br>- syscall.md（系统调用安全）<br>- memoryrovol.md（内存安全与进化机制） |

---

## 附录 C：快速索引

### 按规则编号

- **2-1** 边界校验
- **2-2** 数组索引校验
- **2-3** 有符号整数防溢出
- **2-4** 无符号整数防回绕
- **2-5** 除零检查
- **2-6** 避免 reinterpret_cast
- **2-7** 避免 const_cast
- **2-8** 容器索引校验
- **3-1** 内存申请校验
- **3-2** new/delete 配对使用
- **3-3** 敏感信息清理
- **3-4** 成员变量初始化
- **3-5** 三/五/零法则
- **3-6** 虚析构函数
- **3-7** 避免切片
- **3-8** Lambda 捕获
- **4-1** 抛对象本身
- **4-2** 禁止从析构函数抛异常
- **4-3** 禁止外部可控格式串
- **4-4** 禁止外部数据作为进程启动参数
- **5-1** mmap 参数校验
- **5-2** 使用专用函数
- **5-3** copy_from_user 长度校验
- **5-4** copy_to_user 初始化
- **5-5** 禁用 BUG_ON
- **5-6** 禁止休眠函数
- **5-7** 防止栈溢出

### 按主题分类

**输入验证**：2-1, 2-2, 2-8  
**整数安全**：2-3, 2-4, 2-5  
**类型安全**：2-6, 2-7  
**内存管理**：3-1, 3-2, 3-3  
**对象生命周期**：3-4, 3-5, 3-6, 3-7  
**Lambda 安全**：3-8  
**异常处理**：4-1, 4-2  
**格式化安全**：4-3  
**命令注入**：4-4  
**内核安全**：5-1, 5-2, 5-3, 5-4, 5-5, 5-6, 5-7

---
## 附录 D：AgentOS 特定模块安全指南

### D.1 Atoms（原子层）安全编程
Atoms模块实现微内核核心功能，安全要求最为严格：

#### D.1.1 内存管理安全（映射原则：M-3 拓扑优化）
```c
/**
 * @brief NUMA感知内存分配 - 必须防御的内存安全威胁
 * 
 * 安全要求：
 * 1. 大小校验：防止整数溢出导致分配异常大小（规则2-3）
 * 2. 边界对齐：确保内存按HUGEPAGE对齐，防止侧信道攻击
 * 3. NUMA节点验证：防止非法节点访问（信任边界验证）
 * 
 * @security 此函数被coreloopthree调度循环频繁调用，必须极致优化且安全
 */
void* atoms_mem_alloc_numa_safe(size_t size, int numa_node, uint32_t flags) {
    // 规则3-1：内存申请校验
    if (size == 0 || size > MAX_NUMA_ALLOC_SIZE) {
        log_security("Invalid allocation size: %zu", size);
        return NULL;
    }
    
    // 规则2-3：防止整数溢出
    size_t aligned_size = ALIGN_UP(size, HUGEPAGE_SIZE);
    if (aligned_size < size) { // 检查对齐后溢出
        return NULL;
    }
    
    // NUMA节点信任边界验证
    if (numa_node < -1 || numa_node >= numa_num_configured_nodes()) {
        log_security("Invalid NUMA node: %d", numa_node);
        return NULL;
    }
    
    // 实际分配逻辑...
}
```

#### D.1.2 任务调度安全（映射原则：C-2 认知优化）
- **System 1/System 2路径安全**：快速路径和慢速路径都需内置安全校验
- **任务隔离**：不同安全级别的任务必须严格隔离（D-2安全隔离）
- **调度策略验证**：防止恶意任务破坏调度器状态

### D.2 daemon（守护层）安全编程
Backs模块作为系统服务，需强调可靠性和抗攻击性：

#### D.2.1 IPC通信安全（映射原则：E-3 通信基础设施）
```c
/**
 * @brief 安全IPC消息处理 - 必须遵循的防御深度策略
 * 
 * 防御层次：
 * 1. 消息边界验证（规则2-1）
 * 2. 权限检查（D-1最小权限）
 * 3. 资源限制（防止资源耗尽）
 * 4. 审计日志（D-4安全审计）
 * 
 * @see ipc.md中的安全通信协议
 */
int backs_ipc_process_message_safe(ipc_message_t* msg) {
    // 层次1：输入验证（规则2-1）
    if (msg == NULL || !validate_ipc_message(msg)) {
        log_security("Invalid IPC message");
        return IPC_ERR_INVALID;
    }
    
    // 层次2：权限检查
    if (!check_ipc_permission(msg->sender, msg->operation)) {
        log_audit("Unauthorized IPC attempt: sender=%d, op=%d", 
                  msg->sender, msg->operation);
        return IPC_ERR_PERMISSION;
    }
    
    // 层次3：资源限制
    if (current_ipc_resources() > MAX_IPC_RESOURCES) {
        return IPC_ERR_RESOURCE;
    }
    
    // 层次4：处理并审计
    int result = process_ipc_message(msg);
    log_audit("IPC processed: sender=%d, op=%d, result=%d",
              msg->sender, msg->operation, result);
    return result;
}
```

### D.3 cupolas（安全穹顶）安全编程
cupolas模块实现零信任安全模型，要求最高级别的安全保证：

#### D.3.1 能力（Capability）安全
- **最小权限原则**（D-1）：每个能力仅授予必要权限
- **能力传递验证**：能力传递时必须验证传递链
- **能力撤销机制**：支持即时能力撤销

#### D.3.2 形式化验证集成
```c
/**
 * @brief 形式化验证的安全策略检查
 * 
 * 使用形式化方法验证安全策略的正确性。
 * 集成模型检查器，确保无状态机违规。
 * 
 * @formal 此函数的安全属性已通过Coq验证
 * @see Security_design_guide.md中的形式化安全模型
 */
bool domes_verify_security_policy(policy_t* policy) {
    // 形式化验证前置条件
    if (!precondition_holds(policy)) {
        return false;
    }
    
    // 模型检查
    if (!model_check_policy(policy)) {
        log_security("Policy model check failed");
        return false;
    }
    
    // 运行时验证
    return runtime_enforce_policy(policy);
}
```

### D.4 bases（公共库层）安全编程
Common模块提供跨层安全基础设施：

#### D.4.1 密码学安全
- **算法选择**：使用经过验证的密码学原语（AES-GCM、ChaCha20-Poly1305）
- **密钥管理**：实现安全的密钥生命周期管理
- **侧信道防护**：防御时序攻击和缓存侧信道

#### D.4.2 安全随机数生成
```c
/**
 * @brief 安全随机数生成 - 必须防御的熵源攻击
 * 
 * 安全要求：
 * 1. 混合多个熵源（硬件RNG、jitter entropy等）
 * 2. 定期重新播种
 * 3. 防止输出预测
 * 
 * @cryptographic 此函数用于生成密码学密钥，必须最高安全级别
 */
void common_secure_random(void* buffer, size_t size) {
    // 规则3-1：内存申请校验
    if (buffer == NULL || size == 0 || size > MAX_RANDOM_SIZE) {
        log_security("Invalid random request: size=%zu", size);
        return;
    }
    
    // 混合多个熵源
    mix_entropy_sources();
    
    // 生成随机数
    generate_random(buffer, size);
    
    // 规则3-3：敏感信息清理（临时熵池）
    cleanup_entropy_pool();
}
```

### D.5 跨模块安全交互
重要跨模块接口必须实现额外的安全防护：

```c
/**
 * @brief 核心三循环安全集成点（关键安全接口）
 * 
 * 此接口连接coreloopthree调度器与microkernel任务管理。
 * 必须实现的安全机制：
 * 1. 双向身份验证（调用者与被调用者）
 * 2. 请求完整性保护（HMAC签名）
 * 3. 抗重放攻击（Nonce机制）
 * 4. 速率限制（防止DoS）
 * 
 * @security_critical 此接口被标记为安全关键路径
 * @performance 必须高效实现，不影响调度延迟
 */
int secure_cross_module_schedule(task_t* task) {
    // 层次1：请求验证
    if (!verify_task_signature(task)) {
        log_security("Invalid task signature");
        return SCHEDULE_ERR_SECURITY;
    }
    
    // 层次2：抗重放检查
    if (is_replay_attack(task->nonce)) {
        log_security("Replay attack detected");
        return SCHEDULE_ERR_REPLAY;
    }
    
    // 层次3：权限检查
    if (!check_schedule_permission(task)) {
        log_audit("Unauthorized schedule attempt");
        return SCHEDULE_ERR_PERMISSION;
    }
    
    // 实际调度逻辑
    return schedule_task(task);
}
```
