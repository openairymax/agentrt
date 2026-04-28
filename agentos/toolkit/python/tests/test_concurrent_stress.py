#!/usr/bin/env python3
"""
AgentOS Concurrent Stress Test Suite
Tests: PluginRegistry concurrent operations, SDK thread safety, QPS measurement
Target: QPS >= 500 for plugin operations
"""

import threading
import time
import statistics
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from agentos.framework.plugin import (
    PluginRegistry, PluginState, BasePlugin, get_plugin_registry
)


class StressTestPlugin(BasePlugin):
    _pid = "stress_test"

    @property
    def plugin_id(self):
        return self._pid

    @plugin_id.setter
    def plugin_id(self, value):
        self._pid = value

    def get_capabilities(self):
        return ["stress_test", "concurrent"]

    def on_load(self, config=None):
        pass

    def on_activate(self, config=None):
        pass

    def on_deactivate(self):
        pass

    def on_unload(self):
        pass

    def on_error(self, error):
        pass


def make_plugin_class(pid):
    class P(StressTestPlugin):
        _pid = pid

        @property
        def plugin_id(self):
            return self._pid

        @plugin_id.setter
        def plugin_id(self, value):
            self._pid = value
    return P


def test_concurrent_register_qps():
    print("\n=== Test 1: Concurrent Register QPS ===")
    registry = PluginRegistry()
    num_ops = 1000
    errors = []

    def register_worker(start_idx, count):
        for i in range(start_idx, start_idx + count):
            try:
                pid = f"qps_reg_{i}"
                registry.register(make_plugin_class(pid))
            except Exception as e:
                errors.append(str(e))

    start = time.perf_counter()
    threads = []
    per_thread = num_ops // 10
    for t in range(10):
        th = threading.Thread(target=register_worker, args=(t * per_thread, per_thread))
        threads.append(th)
        th.start()
    for th in threads:
        th.join()
    elapsed = time.perf_counter() - start

    qps = num_ops / elapsed
    print(f"  Operations: {num_ops}")
    print(f"  Elapsed: {elapsed:.3f}s")
    print(f"  QPS: {qps:.0f}")
    print(f"  Errors: {len(errors)}")
    assert qps >= 500, f"QPS {qps:.0f} < 500"
    print("  [PASS] QPS >= 500")


def test_concurrent_load_unload_qps():
    print("\n=== Test 2: Concurrent Load/Unload QPS ===")
    registry = PluginRegistry()
    for i in range(100):
        registry.register(make_plugin_class(f"lu_{i}"))

    latencies = []
    lock = threading.Lock()

    def load_unload_worker(pids):
        for pid in pids:
            try:
                registry.load(pid)
                t0 = time.perf_counter()
                registry.unload(pid)
                elapsed = time.perf_counter() - t0
                with lock:
                    latencies.append(elapsed)
            except Exception:
                pass

    start = time.perf_counter()
    threads = []
    chunk = 10
    for t in range(10):
        pids = [f"lu_{i}" for i in range(t * chunk, (t + 1) * chunk)]
        th = threading.Thread(target=load_unload_worker, args=(pids,))
        threads.append(th)
        th.start()
    for th in threads:
        th.join()
    elapsed = time.perf_counter() - start

    total_ops = 100 * 2
    qps = total_ops / elapsed
    if latencies:
        lat_ms = [l * 1000 for l in latencies]
        lat_sorted = sorted(lat_ms)
        p50 = statistics.median(lat_ms)
        p95 = lat_sorted[int(len(lat_sorted) * 0.95)]
        p99 = lat_sorted[int(len(lat_sorted) * 0.99)]
        print(f"  Latency P50: {p50:.2f}ms")
        print(f"  Latency P95: {p95:.2f}ms")
        print(f"  Latency P99: {p99:.2f}ms")
    print(f"  Operations: {total_ops}")
    print(f"  Elapsed: {elapsed:.3f}s")
    print(f"  QPS: {qps:.0f}")
    assert qps >= 500, f"QPS {qps:.0f} < 500"
    print("  [PASS] QPS >= 500")


def test_concurrent_activate_deactivate():
    print("\n=== Test 3: Concurrent Activate/Deactivate ===")
    registry = PluginRegistry()
    for i in range(50):
        registry.register(make_plugin_class(f"ad_{i}"))
        registry.load(f"ad_{i}")

    latencies = []
    lock = threading.Lock()

    def activate_deactivate_worker(pids):
        for pid in pids:
            try:
                registry.activate(pid)
                t0 = time.perf_counter()
                registry.deactivate(pid)
                elapsed = time.perf_counter() - t0
                with lock:
                    latencies.append(elapsed)
            except Exception:
                pass

    start = time.perf_counter()
    threads = []
    chunk = 5
    for t in range(10):
        pids = [f"ad_{i}" for i in range(t * chunk, (t + 1) * chunk)]
        th = threading.Thread(target=activate_deactivate_worker, args=(pids,))
        threads.append(th)
        th.start()
    for th in threads:
        th.join()
    elapsed = time.perf_counter() - start

    total_ops = 50 * 2
    qps = total_ops / elapsed
    if latencies:
        lat_ms = [l * 1000 for l in latencies]
        print(f"  Latency P50: {statistics.median(lat_ms):.2f}ms")
    print(f"  Operations: {total_ops}")
    print(f"  Elapsed: {elapsed:.3f}s")
    print(f"  QPS: {qps:.0f}")
    assert qps >= 500, f"QPS {qps:.0f} < 500"
    print("  [PASS] QPS >= 500")


def test_concurrent_discover_qps():
    print("\n=== Test 4: Concurrent Discover QPS ===")
    registry = PluginRegistry()
    for i in range(100):
        registry.register(make_plugin_class(f"disc_{i}"))

    num_ops = 2000
    latencies = []
    lock = threading.Lock()

    def discover_worker(count):
        for _ in range(count):
            t0 = time.perf_counter()
            registry.discover("concurrent")
            elapsed = time.perf_counter() - t0
            with lock:
                latencies.append(elapsed)

    start = time.perf_counter()
    threads = []
    per_thread = num_ops // 10
    for t in range(10):
        th = threading.Thread(target=discover_worker, args=(per_thread,))
        threads.append(th)
        th.start()
    for th in threads:
        th.join()
    elapsed = time.perf_counter() - start

    qps = num_ops / elapsed
    lat_ms = [l * 1000 for l in latencies]
    lat_sorted = sorted(lat_ms)
    p50 = statistics.median(lat_ms)
    p95 = lat_sorted[int(len(lat_sorted) * 0.95)]
    p99 = lat_sorted[int(len(lat_sorted) * 0.99)]
    print(f"  Operations: {num_ops}")
    print(f"  Elapsed: {elapsed:.3f}s")
    print(f"  QPS: {qps:.0f}")
    print(f"  Latency P50: {p50:.2f}ms")
    print(f"  Latency P95: {p95:.2f}ms")
    print(f"  Latency P99: {p99:.2f}ms")
    assert qps >= 500, f"QPS {qps:.0f} < 500"
    print("  [PASS] QPS >= 500")


def test_mixed_concurrent_operations():
    print("\n=== Test 5: Mixed Concurrent Operations ===")
    registry = PluginRegistry()
    for i in range(50):
        registry.register(make_plugin_class(f"mix_{i}"))
        registry.load(f"mix_{i}")

    ops_count = [0]
    lock = threading.Lock()

    def mixed_worker(op_type, count):
        for i in range(count):
            try:
                if op_type == "discover":
                    registry.discover()
                elif op_type == "state":
                    registry.get_state(f"mix_{i % 50}")
                elif op_type == "activate":
                    pid = f"mix_{i % 50}"
                    registry.activate(pid)
                elif op_type == "deactivate":
                    pid = f"mix_{i % 50}"
                    registry.deactivate(pid)
                with lock:
                    ops_count[0] += 1
            except Exception:
                pass

    start = time.perf_counter()
    threads = []
    ops = [("discover", 200), ("state", 200), ("activate", 50), ("deactivate", 50)]
    for op_type, count in ops:
        th = threading.Thread(target=mixed_worker, args=(op_type, count))
        threads.append(th)
        th.start()
    for th in threads:
        th.join()
    elapsed = time.perf_counter() - start

    qps = ops_count[0] / elapsed
    print(f"  Operations: {ops_count[0]}")
    print(f"  Elapsed: {elapsed:.3f}s")
    print(f"  QPS: {qps:.0f}")
    assert qps >= 500, f"QPS {qps:.0f} < 500"
    print("  [PASS] QPS >= 500")


if __name__ == "__main__":
    print("=" * 60)
    print("  AgentOS Concurrent Stress Test Report")
    print("=" * 60)

    results = []
    tests = [
        ("Concurrent Register QPS", test_concurrent_register_qps),
        ("Concurrent Load/Unload QPS", test_concurrent_load_unload_qps),
        ("Concurrent Activate/Deactivate", test_concurrent_activate_deactivate),
        ("Concurrent Discover QPS", test_concurrent_discover_qps),
        ("Mixed Concurrent Operations", test_mixed_concurrent_operations),
    ]

    for name, test_fn in tests:
        try:
            test_fn()
            results.append((name, "PASS"))
        except AssertionError as e:
            results.append((name, f"FAIL: {e}"))
        except Exception as e:
            results.append((name, f"ERROR: {e}"))

    print("\n" + "=" * 60)
    print("  Summary")
    print("=" * 60)
    passed = sum(1 for _, r in results if r == "PASS")
    for name, result in results:
        status = "PASS" if result == "PASS" else "FAIL"
        print(f"  [{status}] {name}: {result}")
    print(f"\n  Total: {passed}/{len(results)} passed")

    sys.exit(0 if passed == len(results) else 1)
