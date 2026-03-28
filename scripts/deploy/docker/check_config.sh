# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Docker 配置检查脚�?
# 验证所有配置文件和环境的正确�?

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

ERRORS=0
WARNINGS=0

# 打印检查结�?
print_check() {
    local status=$1
    local message=$2
    
    case $status in
        "OK")
            echo -e "${GREEN}[✓]${NC} $message"
            ;;
        "WARNING")
            echo -e "${YELLOW}[!]${NC} $message"
            ((WARNINGS++))
            ;;
        "ERROR")
            echo -e "${RED}[✗]${NC} $message"
            ((ERRORS++))
            ;;
        "INFO")
            echo -e "${BLUE}[i]${NC} $message"
            ;;
    esac
}

echo "=========================================="
echo "  AgentOS Docker 配置检�?
echo "=========================================="
echo ""

# 1. 检�?Docker 环境
print_check "INFO" "检�?Docker 环境..."

if command -v docker &> /dev/null; then
    print_check "OK" "Docker 已安装：$(docker --version)"
else
    print_check "ERROR" "Docker 未安�?
fi

if docker info &> /dev/null 2>&1; then
    print_check "OK" "Docker 服务运行正常"
else
    print_check "ERROR" "Docker 服务未运�?
fi

if command -v docker-compose &> /dev/null || command -v docker compose &> /dev/null; then
    print_check "OK" "Docker Compose 已安�?
else
    print_check "ERROR" "Docker Compose 未安�?
fi

echo ""

# 2. 检查文件结�?
print_check "INFO" "检查文件结�?.."

files=(
    "Dockerfile.kernel"
    "Dockerfile.service"
    "docker-compose.yml"
    "build.sh"
    "README.md"
    ".env.example"
    "Makefile"
)

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        print_check "OK" "$file 存在"
    else
        print_check "ERROR" "$file 缺失"
    fi
done

echo ""

# 3. 检�?.env 文件
print_check "INFO" "检查环境变量配�?.."

if [ -f ".env" ]; then
    print_check "OK" ".env 文件存在"
    
    # 检查必要的变量
    if grep -q "OPENAI_API_KEY=" .env && ! grep -q "OPENAI_API_KEY=sk-your-openai-api-key-here" .env; then
        print_check "OK" "OPENAI_API_KEY 已配�?
    else
        print_check "WARNING" "OPENAI_API_KEY 未配置或使用默认�?
    fi
    
    if grep -q "POSTGRES_PASSWORD=" .env; then
        password=$(grep "POSTGRES_PASSWORD=" .env | cut -d '=' -f 2)
        if [ "$password" == "agentos_secure_password_123" ]; then
            print_check "WARNING" "POSTGRES_PASSWORD 使用默认值，建议修改"
        else
            print_check "OK" "POSTGRES_PASSWORD 已配�?
        fi
    else
        print_check "ERROR" "POSTGRES_PASSWORD 未配�?
    fi
else
    print_check "WARNING" ".env 文件不存在，将使�?.env.example 默认�?
fi

echo ""

# 4. 检查端口占�?
print_check "INFO" "检查端口占用情�?.."

ports=(8080 8081 8082 8083 8084)
for port in "${ports[@]}"; do
    if command -v lsof &> /dev/null; then
        if lsof -i :$port &> /dev/null; then
            print_check "WARNING" "端口 $port 被占�?
        else
            print_check "OK" "端口 $port 可用"
        fi
    elif command -v netstat &> /dev/null; then
        if netstat -tuln | grep -q ":$port "; then
            print_check "WARNING" "端口 $port 被占�?
        else
            print_check "OK" "端口 $port 可用"
        fi
    else
        print_check "INFO" "无法检查端�?$port（缺�?lsof/netstat�?
        break
    fi
done

echo ""

# 5. 检�?Docker 配置
print_check "INFO" "检�?Docker 配置..."

# 验证 docker-compose.yml 语法
if $DOCKER_COMPOSE manager > /dev/null 2>&1; then
    print_check "OK" "docker-compose.yml 语法正确"
else
    print_check "ERROR" "docker-compose.yml 语法错误"
fi

# 检查镜像版�?
if grep -q "1.0.0.5" docker-compose.yml; then
    print_check "OK" "镜像版本正确 (1.0.0.5)"
else
    print_check "WARNING" "镜像版本可能不是最新的"
fi

echo ""

# 6. 检查资源限�?
print_check "INFO" "检查资源配�?.."

cpu_limit=$(grep -A 5 "deploy:" docker-compose.yml | grep "cpus:" | head -1 | awk '{print $2}' | tr -d "'")
memory_limit=$(grep -A 5 "deploy:" docker-compose.yml | grep "memory:" | head -1 | awk '{print $2}')

if [ ! -z "$cpu_limit" ]; then
    print_check "OK" "CPU 限制�?cpu_limit"
else
    print_check "WARNING" "未设�?CPU 限制"
fi

if [ ! -z "$memory_limit" ]; then
    print_check "OK" "内存限制�?memory_limit"
else
    print_check "WARNING" "未设置内存限�?
fi

echo ""

# 7. 检查健康检查配�?
print_check "INFO" "检查健康检查配�?.."

services=("agentos-kernel" "agentos-services" "redis" "postgres")
for service in "${services[@]}"; do
    if grep -A 10 "$service:" docker-compose.yml | grep -q "healthcheck:"; then
        print_check "OK" "$service 配置了健康检�?
    else
        print_check "WARNING" "$service 未配置健康检�?
    fi
done

echo ""

# 8. 检查数据卷配置
print_check "INFO" "检查数据卷配置..."

if grep -q "volumes:" docker-compose.yml; then
    print_check "OK" "数据卷已配置"
    
    # 检查持久化数据�?
    if grep -q "redis-data:" docker-compose.yml && grep -q "postgres-data:" docker-compose.yml; then
        print_check "OK" "数据库持久化已配�?
    else
        print_check "WARNING" "数据库持久化配置可能不完�?
    fi
else
    print_check "ERROR" "未配置数据卷"
fi

echo ""

# 9. 检查网络安�?
print_check "INFO" "检查网络安全配�?.."

if grep -q "networks:" docker-compose.yml; then
    print_check "OK" "网络配置已定�?
else
    print_check "WARNING" "未定义自定义网络"
fi

# 检查是否使用非 root 用户
if grep -q "user:" docker-compose.yml; then
    print_check "OK" "配置了非 root 用户"
else
    print_check "INFO" "使用默认用户（建议在 Dockerfile 中创建非 root 用户�?
fi

echo ""

# 10. 检查脚本权�?
print_check "INFO" "检查脚本权�?.."

scripts=("build.sh" "quickstart.sh")
for script in "${scripts[@]}"; do
    if [ -f "$script" ]; then
        if [ -x "$script" ]; then
            print_check "OK" "$script 可执�?
        else
            print_check "WARNING" "$script 不可执行，需�?chmod +x"
        fi
    fi
done

echo ""

# 总结
echo "=========================================="
echo "  检查完�?
echo "=========================================="
echo ""
echo "错误数：$ERRORS"
echo "警告数：$WARNINGS"
echo ""

if [ $ERRORS -gt 0 ]; then
    print_check "ERROR" "发现 $ERRORS 个错误，请修复后再运�?
    exit 1
elif [ $WARNINGS -gt 0 ]; then
    print_check "WARNING" "发现 $WARNINGS 个警告，建议检查配�?
    echo ""
    echo "虽然有警告，但您可以继续�?
    echo "  构建镜像�?/build.sh all release"
    echo "  快速启动：./quickstart.sh"
else
    print_check "OK" "所有检查通过�?
    echo ""
    echo "您可以安全地执行以下命令�?
    echo "  构建镜像�?/build.sh all release"
    echo "  快速启动：./quickstart.sh"
fi

echo ""
