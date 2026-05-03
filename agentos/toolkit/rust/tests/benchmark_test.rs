use agentos::client::ClientBuilder;
use agentos::types::*;
use agentos::modules::task::TaskManager;
use agentos::modules::memory::MemoryManager;
use agentos::modules::session::SessionManager;
use agentos::modules::skill::SkillManager;
use std::time::Instant;

fn create_test_client() -> agentos::client::Client {
    ClientBuilder::new()
        .endpoint("http://localhost:18789")
        .timeout(std::time::Duration::from_secs(5))
        .build()
        .expect("Failed to create client")
}

#[cfg(test)]
mod benchmarks {
    use super::*;

    #[test]
    fn bench_task_manager_submit() {
        let client = create_test_client();
        let task_mgr = TaskManager::new(client.clone());

        let start = Instant::now();
        let iterations = 100;
        for i in 0..iterations {
            let _ = task_mgr.submit(
                &format!("benchmark-task-{}", i),
                Some("benchmark payload"),
                None,
            );
        }
        let elapsed = start.elapsed();
        let avg_micros = elapsed.as_micros() / iterations as u128;
        println!("Task submit: {} iterations in {:?}, avg {}µs/call",
                 iterations, elapsed, avg_micros);
        assert!(avg_micros < 500_000, "Task submit too slow: {}µs", avg_micros);
    }

    #[test]
    fn bench_memory_manager_write() {
        let client = create_test_client();
        let mem_mgr = MemoryManager::new(client.clone());

        let start = Instant::now();
        let iterations = 100;
        for i in 0..iterations {
            let _ = mem_mgr.write(
                &format!("benchmark-mem-{}", i),
                "benchmark content for performance testing",
                None,
            );
        }
        let elapsed = start.elapsed();
        let avg_micros = elapsed.as_micros() / iterations as u128;
        println!("Memory write: {} iterations in {:?}, avg {}µs/call",
                 iterations, elapsed, avg_micros);
        assert!(avg_micros < 500_000, "Memory write too slow: {}µs", avg_micros);
    }

    #[test]
    fn bench_memory_manager_search() {
        let client = create_test_client();
        let mem_mgr = MemoryManager::new(client.clone());

        let start = Instant::now();
        let iterations = 50;
        for i in 0..iterations {
            let _ = mem_mgr.search(&format!("query-{}", i), 10, None);
        }
        let elapsed = start.elapsed();
        let avg_micros = elapsed.as_micros() / iterations as u128;
        println!("Memory search: {} iterations in {:?}, avg {}µs/call",
                 iterations, elapsed, avg_micros);
        assert!(avg_micros < 500_000, "Memory search too slow: {}µs", avg_micros);
    }

    #[test]
    fn bench_session_manager_create() {
        let client = create_test_client();
        let sess_mgr = SessionManager::new(client.clone());

        let start = Instant::now();
        let iterations = 50;
        for i in 0..iterations {
            let _ = sess_mgr.create(None, Some(&format!("bench-session-{}", i)));
        }
        let elapsed = start.elapsed();
        let avg_micros = elapsed.as_micros() / iterations as u128;
        println!("Session create: {} iterations in {:?}, avg {}µs/call",
                 iterations, elapsed, avg_micros);
        assert!(avg_micros < 500_000, "Session create too slow: {}µs", avg_micros);
    }

    #[test]
    fn bench_skill_manager_list() {
        let client = create_test_client();
        let skill_mgr = SkillManager::new(client.clone());

        let start = Instant::now();
        let iterations = 50;
        for _ in 0..iterations {
            let _ = skill_mgr.list(None);
        }
        let elapsed = start.elapsed();
        let avg_micros = elapsed.as_micros() / iterations as u128;
        println!("Skill list: {} iterations in {:?}, avg {}µs/call",
                 iterations, elapsed, avg_micros);
        assert!(avg_micros < 500_000, "Skill list too slow: {}µs", avg_micros);
    }

    #[test]
    fn bench_concurrent_task_submit() {
        let client = create_test_client();
        let task_mgr = std::sync::Arc::new(TaskManager::new(client.clone()));

        let start = Instant::now();
        let num_threads = 10;
        let tasks_per_thread = 10;

        let mut handles = vec![];
        for t in 0..num_threads {
            let mgr = task_mgr.clone();
            handles.push(std::thread::spawn(move || {
                for i in 0..tasks_per_thread {
                    let _ = mgr.submit(
                        &format!("concurrent-{}-{}", t, i),
                        Some("concurrent benchmark"),
                        None,
                    );
                }
            }));
        }

        for h in handles {
            let _ = h.join();
        }

        let elapsed = start.elapsed();
        let total_ops = num_threads * tasks_per_thread;
        let ops_per_sec = total_ops as f64 / elapsed.as_secs_f64();
        println!("Concurrent task submit: {} ops in {:?}, {:.0} ops/sec",
                 total_ops, elapsed, ops_per_sec);
        assert!(ops_per_sec > 1.0, "Concurrent throughput too low: {:.0} ops/sec", ops_per_sec);
    }

    #[test]
    fn bench_client_creation() {
        let start = Instant::now();
        let iterations = 1000;
        for _ in 0..iterations {
            let _client = ClientBuilder::new()
                .endpoint("http://localhost:18789")
                .timeout(std::time::Duration::from_secs(5))
                .build();
        }
        let elapsed = start.elapsed();
        let avg_nanos = elapsed.as_nanos() / iterations as u128;
        println!("Client creation: {} iterations in {:?}, avg {}ns/call",
                 iterations, elapsed, avg_nanos);
        assert!(avg_nanos < 1_000_000, "Client creation too slow: {}ns", avg_nanos);
    }
}
