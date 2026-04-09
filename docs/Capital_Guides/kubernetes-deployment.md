Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# Kubernetes 部署指南

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**适用场景**: 中大规模生产环境、高可用部署  

---

## 📋 前置条件

| 组件 | 最低版本 | 推荐版本 |
|------|---------|---------|
| Kubernetes | v1.25 | v1.28+ |
| kubectl | v1.25 | 最新稳定版 |
| Helm | v3.12 | 最新稳定版 |
| 节点数 | ≥3 (控制平面+工作节点) | ≥5 (生产环境) |

**硬件要求（每个工作节点）**:

| 规模 | CPU | 内存 | 存储 |
|------|-----|------|------|
| 最小 | 4核 | 16GB | 50GB SSD |
| 推荐 | 8核 | 32GB | 100GB NVMe |

---

## 🚀 快速部署

### 步骤1：添加Helm仓库

```bash
# 添加AgentOS官方Helm仓库
helm repo add spharx https://charts.spharx.cn/agentos
helm repo update

# 查看可用版本
helm search repo spharx/agentos --versions
```

### 步骤2：创建命名空间和Secrets

```bash
# 创建命名空间
kubectl create namespace agentos

# 创建通用Secret（敏感信息）
kubectl create secret generic agentos-secrets \
    --namespace=agentos \
    --from-literal=postgres-password='YourSuperSecureP@ssw0rd2026!' \
    --from-literal=redis-password='YourRedisS3cretK3y2026!' \
    --from-literal=llm-api-key='sk-proj-xxxxxxxxxxxxxxxxxxxxxxxx' \
    --from-literal=openlab-secret-key="$(openssl rand -base64 32)" \
    --from-literal=grafana-admin-password='YourGrafanaAdminP@ss2026!'

# （可选）使用TLS证书
kubectl create secret tls agentos-tls \
    --namespace=agentos \
    --cert=path/to/fullchain.pem \
    --key=path/to/privkey.pem
```

### 步骤3：自定义配置

创建 `production-values.yaml`:

```yaml
global:
  imageRegistry: registry.spharx.cn  # 私有镜像仓库（可选）
  imagePullSecrets:
    - name: registry-credentials      # 镜像拉取凭证
  storageClass: "ssd-premium"        # 存储类名称

kernel:
  replicas: 3                        # 内核实例数（高可用）
  resources:
    requests:
      cpu: "2"
      memory: "2Gi"
    limits:
      cpu: "4"
      memory: "4Gi"

  autoscaling:
    enabled: true
    minReplicas: 3
    maxReplicas: 10
    targetCPUUtilizationPercentage: 70
    targetMemoryUtilizationPercentage: 80

  nodeSelector:
    node-type: compute               # 调度到计算节点

  tolerations:
    - key: "dedicated"
      operator: "Equal"
      value: "agentos"
      effect: "NoSchedule"

postgresql:
  enabled: true
  primary:
    persistence:
      size: 100Gi                    # 数据库存储大小
      storageClass: "ssd-premium"
    resources:
      requests:
        cpu: "1"
        memory: "2Gi"
      limits:
        cpu: "2"
        memory: "4Gi"
    # 主从复制配置（高可用）
    replication:
      enabled: true
      numSynchronousReplicas: 1     # 同步副本数
      applicationName: "agentos"

redis:
  enabled: true
  architecture: standalone          # 或 cluster（大规模时启用）
  master:
    persistence:
      size: 20Gi
      storageClass: "ssd-premium"
  # Redis Cluster模式配置（可选）
  # cluster:
  #   nodes: 6
  #   replicas: 1

daemon:
  llm_d:
    replicas: 2
    resources:
      requests:
        cpu: "1"
        memory: "1Gi"
  sched_d:
    replicas: 2
    resources:
      requests:
        cpu: "500m"
        memory: "512Mi"

openlab:
  enabled: true
  replicas: 2
  ingress:
    enabled: true
    className: "nginx"
    hosts:
      - name: openlab.spharx.cn
        tls:
          secretName: agentos-tls
    tls:
      - hosts:
          - openlab.spharx.cn
        secretName: agentos-tls

monitoring:
  prometheus:
    enabled: true
    retention: "30d"
    persistence:
      size: 50Gi
    resources:
      requests:
        cpu: "500m"
        memory: "512Mi"

  grafana:
    enabled: true
    adminPassword: existingSecret   # 使用之前创建的Secret
    persistence:
      size: 10Gi
    ingress:
      enabled: true
      className: "nginx"
      hosts:
        - name: grafana.spharx.cn
```

### 步骤4：安装AgentOS

```bash
# 安装（dry-run先预览生成的资源）
helm install agentos spharx/agentos \
    --namespace agentos \
    --values production-values.yaml \
    --dry-run=client

# 确认无误后正式安装
helm install agentos spharx/agentos \
    --namespace agentos \
    --values production-values.yaml \
    --timeout 15m \
    --wait

# 查看部署状态
kubectl get pods -n agentos -w

# 查看所有资源
kubectl get all -n agentos
```

---

## 🔒 安全加固

### NetworkPolicy（网络隔离）

```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: agentos-network-policy
  namespace: agentos
spec:
  podSelector: {}
  policyTypes:
    - Ingress
    - Egress
  ingress:
    # 允许命名空间内通信
    - from:
        - podSelector: {}
    # 允许Prometheus抓取metrics
    - from:
        - namespaceSelector:
            matchLabels:
              name: monitoring
        podSelector:
          matchLabels:
            app: prometheus
  egress:
    # 允许访问DNS
    - to:
        - namespaceSelector: {}
          podSelector:
            matchLabels:
              k8s-app: kube-dns
      ports:
        - protocol: UDP
          port: 53
    # 允许访问外部LLM API
    - to:
        - namespaceSelector: {}
      ports:
        - protocol: TCP
          port: 443
```

### PodSecurityPolicy / Pod Security Standards

```yaml
# 使用Restricted级别的Pod安全标准
apiVersion: v1
kind: Namespace
metadata:
  name: agentos
  labels:
    pod-security.kubernetes.io/enforce: restricted
    pod-security.kubernetes.io/audit: restricted
    pod-security.kubernetes.io/warn: restricted
```

### RBAC权限控制

```yaml
# 最小权限原则：仅授予必要的ServiceAccount权限
apiVersion: rbac.authorization.k8s.io/v1
kind: Role
metadata:
  name: agentos-pod-reader
  namespace: agentos
rules:
  - apiGroups: [""]
    resources: ["pods", "pods/log"]
    verbs: ["get", "list"]
---
apiVersion: rbac.authorization.k8s.io/v1
kind: RoleBinding
metadata:
  name: agentos-pod-reader-binding
  namespace: agentos
subjects:
  - kind: ServiceAccount
    name: agentos-kernel
    namespace: agentos
roleRef:
  kind: Role
  name: agentos-pod-reader
  apiGroup: rbac.authorization.k8s.io
```

---

## ⚙️ 运维操作

### 扩缩容

```bash
# 手动扩缩内核实例
kubectl scale deployment agentos-kernel -n agentos --replicas=5

# 启用HPA自动扩缩容
kubectl autoscale deployment agentos-kernel \
    -n agentos \
    --cpu-percent=70 \
    --min=3 \
    --max=20

# 查看HPA状态
kubectl get hpa -n agentos
```

### 滚动更新

```bash
# 更新到新版本
helm upgrade agentos spharx/agentos \
    --namespace agentos \
    --values production-values.yaml \
    --set global.image.tag=v1.1.0

# 查看更新状态
kubectl rollout status deployment/agentos-kernel -n agentup

# 回滚到上一版本
kubectl rollout undo deployment/agentos-kernel -n agentos

# 查看历史版本
kubectl rollout history deployment/agentos-kernel -n agentos
```

### 备份恢复

```bash
# === 备份 ===

# PostgreSQL备份（使用pg_dump到S3/MinIO）
kubectl exec -it agentos-postgresql-0 -n agentos -- \
    pg_dump -U agentos -d agentos | gzip > backup-$(date +%Y%m%d).sql.gz

# Redis备份（RDB快照）
kubectl exec -it agentos-redis-master-0 -n agentos -- \
    redis-cli --rdb /data/dump-$(date +%Y%m%d).rdb

# PVC备份（使用Velero）
velero backup create agentos-full-backup \
    --namespace velero \
    --include-namespaces agentos \
    --wait


# === 恢复 ===

# 从SQL恢复PostgreSQL
gunzip -c backup-YYYYMMDD.sql.gz | kubectl exec -i agentos-postgresql-0 -n agentos -- \
    psql -U agentos -d agentos

# 从Velero恢复全量备份
velero restore create agentos-full-restore \
    --from-backup agentos-full-backup \
    --namespace velero \
    --wait
```

### 故障排查

```bash
# 查看Pod事件
kubectl describe pod <pod-name> -n agentos

# 查看容器日志
kubectl logs <pod-name> -n agentos --previous  # 前一个容器的日志
kubectl logs <pod-name> -c kernel-container -n agentos --tail=100

# 进入容器调试
kubectl exec -it <pod-name> -n agentos -- /bin/bash

# 端口转发本地调试
kubectl port-forward svc/agentos-kernel 8080:8080 -n agentos

# 检查资源配额
kubectl describe resourcequotas -n agentos

# 检查PVC状态
kubectl get pvc -n agentos
kubectl describe pvc <pvc-name> -n agentos
```

---

## 📊 性能调优

### 内核参数优化

```yaml
# 在DaemonSet或Deployment中添加initContainer进行内核调优
initContainers:
  - name: sysctl
    image: busybox:latest
    command:
      - /bin/sh
      - -c
      - |
        sysctl -w net.core.somaxconn=65535
        sysctl -w net.ipv4.tcp_max_syn_backlog=65535
        sysctl -w net.core.netdev_max_backlog=65535
        sysctl -w vm.swappiness=10
        sysctl -w fs.file-max=2097152
    securityContext:
      privileged: true
```

### JVM/Go运行时调优（如适用）

```yaml
env:
  - name: GOGC
    value: "100"           # GC触发阈值（默认100，可降低以减少GC停顿）
  - name: GOMAXPROCS
    value: "4"             # 最大并发CPU数
  - name: GOMEMLIMIT_HARD
    value: "3Gi"           # Go内存硬限制（需要Go 1.19+）
```

---

## 🔗 相关文档

- [**Docker部署指南**](../guides/deployment.md) — Docker Compose部署方案
- [**监控运维**](monitoring.md) — Prometheus+Grafana详细配置
- [**Helm Chart完整文档**](https://charts.spharx.cn/agentos) — 所有Chart值说明
- [**Kubernetes官方文档**](https://kubernetes.io/docs/) — K8s权威参考

---

> *"Kubernetes不是万能的，但它是云原生时代的操作系统。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
