Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 璋冭瘯鎸囧崡

**鐗堟湰**: 1.0.0
**鏈€鍚庢洿鏂?*: 2026-04-06
**鐘舵€?*: 鐢熶骇灏辩华

---

## 馃搵 姒傝堪

鏈寚鍗楁彁渚?AgentOS 绯荤粺鐨勮皟璇曟妧宸с€佸伐鍏蜂娇鐢ㄥ拰甯歌闂璇婃柇鏂规硶銆?
---

## 馃敡 璋冭瘯宸ュ叿绠?
### 1. 鏃ュ織绯荤粺

AgentOS 閲囩敤**缁熶竴鏃ュ織瑙勮寖**锛屾墍鏈夌粍浠惰緭鍑虹粨鏋勫寲鏃ュ織锛?
#### 鏃ュ織鏍煎紡

```json
{
    "timestamp": "2026-04-06T10:00:00.123Z",
    "level": "INFO",
    "logger": "agentos.kernel.ipc",
    "message": "IPC call completed",
    "context": {
        "call_id": "call-abc123",
        "method": "memory.store",
        "duration_ms": 0.85,
        "caller_ip": "127.0.0.1"
    },
    "trace_id": "trace-xyz789",
    "span_id": "span-def012"
}
```

#### 鏃ュ織绾у埆

| 绾у埆 | 鐢ㄩ€?| 绀轰緥 |
|------|------|------|
| **DEBUG** | 璇︾粏璋冭瘯淇℃伅 | 鍑芥暟鍏ュ彛/鍑哄彛銆佸彉閲忓€?|
| **INFO** | 涓€鑸俊鎭?| 璇锋眰澶勭悊銆佺姸鎬佸彉鏇?|
| **WARNING** | 璀﹀憡淇℃伅 | 鎬ц兘闄嶇骇銆侀噸璇曟搷浣?|
| **ERROR** | 閿欒淇℃伅 | 澶勭悊澶辫触銆佸紓甯告崟鑾?|
| **CRITICAL** | 涓ラ噸閿欒 | 绯荤粺宕╂簝銆佹暟鎹崯鍧?|

#### 鏌ョ湅瀹炴椂鏃ュ織

```bash
# 鍐呮牳鏈嶅姟鏃ュ織
journalctl -u agentos-kernel -f --since "5 min ago"

# Docker 瀹瑰櫒鏃ュ織
docker logs -f agentos-kernel-1 --tail 100

# 杩囨护鐗瑰畾绾у埆
docker logs agentos-kernel-1 2>&1 | grep ERROR

# 浣跨敤 jq 瑙ｆ瀽 JSON 鏃ュ織
docker logs agentos-kernel-1 2>&1 | jq 'select(.level == "ERROR")'
```

### 2. 鎬ц兘鍒嗘瀽宸ュ叿

#### GDB 璋冭瘯 (C/C++ 鍐呮牳)

```bash
# 鍚姩 GDB 璋冭瘯鍐呮牳
gdb ./bin/agentos-kernel

(gdb) set pagination off
(gdb) run --config /path/to/config.yaml

# 璁剧疆鏂偣
(gdb) break agentos_ipc_call
(gdb) break agentos_memory_store

# 鎵ц鍒版柇鐐?(gdb) continue

# 鏌ョ湅璋冪敤鏍?(gdb) bt
(gbt) bt full  # 鍖呭惈灞€閮ㄥ彉閲?
# 鏌ョ湅鍙橀噺
(gdb) print *request
(gdb) print response->status_code

# 鍗曟鎵ц
(gdb) next      # 鍗曟锛堜笉杩涘叆鍑芥暟锛?(gdb) step      # 鍗曟锛堣繘鍏ュ嚱鏁帮級
(gdb) finish    # 璺冲嚭褰撳墠鍑芥暟

# 鐩戣鐐癸紙鏁版嵁鏂偣锛?(gdb) watch response->error_code
(gdb) continue  # 褰?error_code 鍙樺寲鏃跺仠姝?```

#### Valgrind 鍐呭瓨妫€娴?
```bash
# 妫€娴嬪唴瀛樻硠婕?valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./bin/agentos-kernel --config config.yaml

# 妫€娴嬪唴瀛樿闂敊璇?valgrind --tool=memcheck \
         ./bin/agentos-kernel

# 鎬ц兘鍓栨瀽锛圕allgrind锛?valgrind --tool=callgrind ./bin/agentos-kernel
callgrind_annotate callgrind.out.*
```

#### Python 璋冭瘯 (瀹堟姢杩涚▼)

```python
# 浣跨敤 pdb 鏂偣璋冭瘯
import pdb; pdb.set_trace()

# 浣跨敤 ipdb锛堝寮虹増 pdb锛?import ipdb; ipdb.set_trace()

# 杩滅▼璋冭瘯锛堜娇鐢?remote-pdb锛?from remote_pdb import RemotePdb
RemotePdb('0.0.0.0', 4444).set_trace()

# 杩炴帴鍒拌繙绋嬭皟璇曚細璇?# 鍦ㄥ彟涓€涓粓绔? telnet localhost 4444
```

```bash
# 浣跨敤 cProfile 杩涜鎬ц兘鍒嗘瀽
python -m cProfile -s cumtime your_script.py

# 浣跨敤 py-spy 杩涜鏃犱镜鍏ユ€ц兘鍒嗘瀽
py-spy top --pid <PID>
py-spy record -o profile.svg --pid <PID>
```

### 3. 缃戠粶璋冭瘯宸ュ叿

```bash
# 鎶撳彇 IPC 閫氫俊鍖?tcpdump -i lo port 8080 -w ipc_capture.pcap

# 鍒嗘瀽鎶撳寘鏂囦欢
tcpdump -r ipc_capture.pcap -A

# 浣跨敤 Wireshark GUI 鍒嗘瀽
wireshark ipc_capture.pcap

# 娴嬭瘯 API 绔偣
curl -v http://localhost:8080/health

# 鍘嬪姏娴嬭瘯
ab -n 10000 -c 100 http://localhost:8080/api/v1/ping

# WebSocket 璋冭瘯
wscat -c ws://localhost:8080/ws
```

### 4. 鏁版嵁搴撹皟璇?
```bash
# PostgreSQL
psql -U agentos -d agentos

# 鏌ョ湅娲昏穬杩炴帴
SELECT * FROM pg_stat_activity WHERE datname = 'agentos';

# 鏌ョ湅鎱㈡煡璇?SELECT query, calls, total_time, mean_time
FROM pg_stat_statements
ORDER BY total_time DESC LIMIT 20;

# 鍒嗘瀽鏌ヨ璁″垝
EXPLAIN ANALYZE SELECT * FROM memory_records WHERE id = 'xxx';

# Redis
redis-cli

# 鏌ョ湅閿┖闂?INFO keyspace

# 鐩戞帶鍛戒护
MONITOR  # 瀹炴椂鏌ョ湅鎵€鏈夊懡浠?
# 鎱㈡棩蹇?CONFIG GET slowlog-log-slower-than
SLOWLOG GET 10
```

---

## 馃悰 甯歌闂璇婃柇娴佺▼

### 闂鍒嗙被涓庡畾浣?
```
鐢ㄦ埛鎶ュ憡闂
    鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?  缃戠粶灞傞棶棰?    鈹?  搴旂敤灞傞棶棰?    鈹?  鏁版嵁灞傞棶棰?    鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹尖攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹尖攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?鈥?杩炴帴瓒呮椂       鈹?鈥?500 閿欒       鈹?鈥?鏁版嵁搴撹繛鎺ュけ璐? 鈹?鈹?鈥?DNS 瑙ｆ瀽澶辫触   鈹?鈥?鍝嶅簲缂撴參       鈹?鈥?鏌ヨ瓒呮椂       鈹?鈹?鈥?TLS 鎻℃墜澶辫触   鈹?鈥?鍐呭瓨娉勬紡       鈹?鈥?姝婚攣           鈹?鈹?鈥?闃茬伀澧欓樆姝?    鈹?鈥?CPU 100%       鈹?鈥?鏁版嵁涓嶄竴鑷?    鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹粹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹粹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?         鈫?               鈫?               鈫?    tcpdump/wireshap   GDB/profiler     psql/redis-cli
```

### 璇婃柇妫€鏌ユ竻鍗?
#### 蹇€熷仴搴锋鏌?
```bash
#!/bin/bash
# agentos-diagnose.sh - 蹇€熻瘖鏂剼鏈?
echo "=== AgentOS 绯荤粺璇婃柇 ==="
echo ""

echo "[1] 鏈嶅姟鐘舵€佹鏌?
systemctl status agentos-kernel || docker ps | grep agentos

echo ""
echo "[2] 绔彛鐩戝惉妫€鏌?
netstat -tlnp | grep -E '(8080|8001|8002|5432|6379)' || ss -tlnp

echo ""
echo "[3] 璧勬簮浣跨敤鎯呭喌"
free -h
df -h /app/data
top -bn1 | head -15

echo ""
echo "[4] 鏃ュ織閿欒缁熻"
journalctl -u agentos-kernel --since "1 hour ago" | grep -c ERROR
docker logs agentos-kernel-1 2>&1 | tail -50 | grep -i error

echo ""
echo "[5] 鏁版嵁搴撹繛鎺ユ鏌?
pg_isready -h localhost -p 5432 -U agentos
redis-cli ping

echo ""
echo "[6] IPC 寤惰繜娴嬭瘯"
time curl -s http://localhost:8080/health > /dev/null

echo ""
echo "[7] 鍐呭瓨绱㈠紩鐘舵€?
curl -s http://localhost:8080/api/v1/memory/stats | jq .

echo ""
echo "=== 璇婃柇瀹屾垚 ==="
```

### 鍒嗗眰璇婃柇鏂规硶

#### 绗竴灞傦細缃戠粶灞?
```bash
# 1. 杩為€氭€ф祴璇?ping kernel-hostname
telnet kernel-hostname 8080

# 2. DNS 瑙ｆ瀽
nslookup kernel-hostname
dig kernel-hostname

# 3. 闃茬伀澧欐鏌?iptables -L -n | grep 8080
ufw status

# 4. TLS 璇佷功妫€鏌?openssl s_client -connect localhost:443 -servername agentos
```

#### 绗簩灞傦細搴旂敤灞?
```bash
# 1. 杩涚▼鐘舵€?ps aux | grep agentos
top -p $(pgrep -f agentos)

# 2. 绾跨▼鐘舵€?pstree -p $(pgrep -f agentos)
ls /proc/<PID>/task/

# 3. 鏂囦欢鎻忚堪绗?ls -la /proc/<PID>/fd/
lsof -p <PID>

# 4. 鍐呭瓨鏄犲皠
cat /proc/<PID>/maps
pmap <PID>

# 5. 鐜鍙橀噺
cat /proc/<PID>/environ | tr '\0' '\n'
```

#### 绗笁灞傦細鍐呮牳灞?(Syscall)

```bash
# 浣跨敤 strace 璺熻釜绯荤粺璋冪敤
strace -f -e trace=network,write -p <PID>

# 缁熻绯荤粺璋冪敤棰戠巼
strace -c -p <PID>

# 浣跨敤 ltrace 璺熻釜搴撳嚱鏁拌皟鐢?ltrace -p <PID>

# 浣跨敤 perf 杩涜鎬ц兘鍒嗘瀽
perf top -p <PID>
perf record -g -p <PID> -- sleep 30
perf report
```

---

## 馃搳 鎬ц兘鐡堕鍒嗘瀽

### CPU 鐡堕

**鐥囩姸**: CPU 浣跨敤鐜囨寔缁?> 80%锛屽搷搴斿欢杩熷鍔?
```bash
# 璇嗗埆 CPU 瀵嗛泦鍨嬭繘绋?top -o %CPU

# 浣跨敤 perf 鍒嗘瀽鐑偣鍑芥暟
perf record -g -a -- sleep 30
perf report --stdio | head -100

# 鐏劙鍥剧敓鎴?perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

**浼樺寲鏂瑰悜**:
- 绠楁硶浼樺寲锛堥檷浣庢椂闂村鏉傚害锛?- 缂撳瓨鐑偣鏁版嵁
- 寮傛 I/O 鏇夸唬鍚屾闃诲
- 骞惰鍖栬绠楀瘑闆嗕换鍔?
### 鍐呭瓨鐡堕

**鐥囩姸**: OOM Killer 瑙﹀彂锛岄绻?GC/swap

```bash
# 鍐呭瓨浣跨敤鍒嗘瀽
free -h
vmstat 1 10
cat /proc/meminfo

# 鍐呭瓨娉勬紡妫€娴?valgrind --leak-check=full ./program
massif-visualizer massif.out.*

# 瀵硅薄鍒嗛厤璺熻釜锛圥ython锛?import tracemalloc
tracemalloc.start()
# ... code ...
snapshot = tracemalloc.take_snapshot()
for stat in snapshot.statistics(10):
    print(stat)
```

**浼樺寲鏂瑰悜**:
- 淇鍐呭瓨娉勬紡
- 浼樺寲鏁版嵁缁撴瀯
- 澧炲姞鍐呭瓨姹犲鐢?- 娴佸紡澶勭悊鏇夸唬鍏ㄩ噺鍔犺浇

### I/O 鐡堕

**鐥囩姸**: 楂?iowait锛岀鐩樺埄鐢ㄧ巼鎺ヨ繎 100%

```bash
# I/O 鐘舵€佺洃鎺?iostat -xz 1
iotop

# 纾佺洏 I/O 杩借釜
sudo biosnoop -t 10  # bcc-tools
blktrace -d /dev/sda -o -

# 鏂囦欢绯荤粺寤惰繜
fileslower 1.0  # bcc-tools
```

**浼樺寲鏂瑰悜**:
- 澧炲姞 SSD 缂撳瓨灞?- 鎵归噺鍐欏叆鏇夸唬鍗曟潯鍐欏叆
- 寮傛 I/O
- 璇诲啓鍒嗙
- 鍒嗗尯琛ㄤ紭鍖?
### 閿佺珵浜?
**鐥囩姸**: 澶氱嚎绋嬫€ц兘涓嶅崌鍙嶉檷

```bash
# 閿佺珵浜夋娴?perf lock contention -a -p <PID>

# pthread 閿佸垎鏋?pthread_mutex_stats  # 鑷畾涔夊伐鍏锋垨 GDB

# Go 鍗忕▼璋冨害鍒嗘瀽
go tool trace trace.out
```

**浼樺寲鏂瑰悜**:
- 鍑忓皬閿佺矑搴?- 鏃犻攣鏁版嵁缁撴瀯
- 璇诲啓閿佹浛浠ｄ簰鏂ラ攣
- 鍑忓皯涓寸晫鍖鸿寖鍥?
---

## 馃幆 鐗瑰畾鍦烘櫙璋冭瘯

### 鍦烘櫙 1: IPC 閫氫俊鏁呴殰

```bash
# Step 1: 楠岃瘉绔彛鐩戝惉
ss -tlnp | grep 8080

# Step 2: 娴嬭瘯鍩烘湰杩為€氭€?curl -v http://localhost:8080/health

# Step 3: 鏌ョ湅 IPC 鏃ュ織
journalctl -u agentos-kernel | grep ipc | tail -50

# Step 4: 鎶撳寘鍒嗘瀽
tcpdump -i lo port 8080 -A -vvv

# Step 5: 妫€鏌ュ叡浜唴瀛?ipcs -m
ipcrm -M <shmid>  # 娓呯悊娈嬬暀鍏变韩鍐呭瓨
```

### 鍦烘櫙 2: 璁板繂绯荤粺鎬ц兘涓嬮檷

```bash
# L1: 妫€鏌?SQLite 鎬ц兘
sqlite3 /app/data/memory/l1/l1_index.db "ANALYZE;"
sqlite3 /app/data/memory/l1/l1_index.db "PRAGMA integrity_check;"

# L2: 妫€鏌?FAISS 绱㈠紩鐘舵€?python -c "
import faiss
index = fa.read_index('/app/data/memory/l2/faiss.index')
print(f'Index size: {index.ntotal}')
print(f'Is trained: {index.is_trained}')
"

# L3: 鍥炬暟鎹簱鏌ヨ璁″垝
psql -U agentos -d agentos_graph -c "
EXPLAIN ANALYZE SELECT * FROM relations WHERE source = 'entity-001';
"

# L4: 鎸栨帢浠诲姟鐘舵€?curl -s http://localhost:8080/api/v1/memory/l4/status | jq .
```

### 鍦烘櫙 3: Agent 琛屼负寮傚父

```bash
# Step 1: 鏌ョ湅 Agent 鐘舵€?curl http://localhost:8080/api/v1/agents/{agent_id}/status | jq .

# Step 2: 鏌ョ湅瀵硅瘽鍘嗗彶
curl http://localhost:8080/api/v1/agents/{agent_id}/sessions/latest | jq .

# Step 3: 鏌ョ湅 Agent 鏃ュ織
docker logs agent-agent-{id}-1 2>&1 | tail -100

# Step 4: 妫€鏌ヨ蹇嗗唴瀹?curl "http://localhost:8080/api/v1/memory/search?query=recent&agent_id={agent_id}" | jq .

# Step 5: LLM 璋冪敤杩借釜
journalctl -u llm_d | grep {agent_id} | tail -50
```

---

## 馃洜锔?璋冭瘯鏈€浣冲疄璺?
### 1. 鍙噸鐜扮殑鏈€灏忕ず渚?
鍦ㄦ姤鍛?Bug 鍓嶏紝鍒涘缓鍙噸鐜扮殑鏈€灏忎唬鐮佺ず渚嬶細

```python
# minimal_repro.py
"""鏈€灏忓彲閲嶇幇绀轰緥"""
from agentos import AgentOSClient

def main():
    client = AgentOSClient(base_url="http://localhost:8080")
    agent = client.create_agent(AgentConfig(name="debug-test"))

    # 杩欎竴姝ヨЕ鍙?bug
    response = agent.chat("Specific input that triggers the bug")
    print(response)  # Unexpected behavior here

if __name__ == "__main__":
    main()
```

### 2. 浜屽垎娉曞畾浣嶉棶棰?
閫氳繃娉ㄩ噴鎺変竴鍗婁唬鐮佹潵蹇€熷畾浣嶉棶棰樺尯鍩燂細

```python
def complex_function():
    part_a()  # 娉ㄩ噴鎺夎繖鍗婇儴鍒?    part_b()  # 淇濈暀杩欏崐閮ㄥ垎
    # 濡傛灉 bug 娑堝け锛岃鏄庨棶棰樺湪 part_a
```

### 3. 鏃ュ織澧炲己鎶€宸?
涓存椂娣诲姞璇︾粏鏃ュ織甯姪瀹氫綅闂锛?
```python
import logging
logger = logging.getLogger(__name__)

def suspicious_function(x, y):
    logger.debug(f"Entering with x={x}, y={y}")
    result = compute(x, y)
    logger.debug(f"Result: {result}, type: {type(result)}")
    return result
```

### 4. 鏂█寮忛槻寰＄紪绋?
娣诲姞杩愯鏃舵柇瑷€鎹曡幏寮傚父鐘舵€侊細

```python
def process_data(data):
    assert isinstance(data, list), f"Expected list, got {type(data)}"
    assert len(data) > 0, "Data should not be empty"
    assert all(isinstance(item, dict) for item in data), "All items must be dicts"
    # ... processing logic
```

---

## 馃摎 鐩稿叧鏂囨。

- **[娴嬭瘯鎸囧崡](testing.md)** 鈥?娴嬭瘯绛栫暐涓庡伐鍏?- **[鏁呴殰鎺掓煡](../troubleshooting/common-issues.md)** 鈥?甯歌闂瑙ｅ喅鏂规
- **[缁熶竴鏃ュ織绯荤粺](../../agentos/docs/architecture/logging_system.md)** 鈥?鏃ュ織瑙勮寖
- **[鍐呮牳璋冧紭鎸囧崡](../../agentos/docs/guides/kernel_tuning.md)** 鈥?鎬ц兘璋冧紭

---

**漏 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
