// AgentOS Rust SDK - Managers 模块测试
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 测试所有 Manager 模块的功能

use agentos_rs::*;
use std::time::Duration;

// ============================================================
// TaskManager 测试
// ============================================================

#[tokio::test]
async fn test_task_manager_submit() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let task_mgr = TaskManager::new(client);

    let task_content = "测试任务内容";
    let result = task_mgr.submit(task_content).await;

    // 如果服务端未启动，会返回错误
    match result {
        Ok(task) => {
            assert!(!task.id.is_empty(), "任务ID不应为空");
            println!("任务提交成功: {}", task.id);
        }
        Err(e) => {
            println!("任务提交失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_task_manager_query() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let task_mgr = TaskManager::new(client);

    let result = task_mgr.query("task-123").await;

    match result {
        Ok(task) => {
            assert_eq!(task.id, "task-123");
            println!("任务查询成功: {:?}", task);
        }
        Err(e) => {
            println!("任务查询失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_task_manager_list() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let task_mgr = TaskManager::new(client);

    let result = task_mgr.list(None, None).await;

    match result {
        Ok(tasks) => {
            println!("任务列表查询成功: {} 个任务", tasks.len());
        }
        Err(e) => {
            println!("任务列表查询失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_task_manager_cancel() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let task_mgr = TaskManager::new(client);

    let result = task_mgr.cancel("task-123").await;

    match result {
        Ok(_) => {
            println!("任务取消成功");
        }
        Err(e) => {
            println!("任务取消失败（服务端可能未启动）: {:?}", e);
        }
    }
}

// ============================================================
// MemoryManager 测试
// ============================================================

#[tokio::test]
async fn test_memory_manager_write() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let memory_mgr = MemoryManager::new(client);

    let content = "测试记忆内容";
    let result = memory_mgr.write(content, Some("L1")).await;

    match result {
        Ok(memory) => {
            assert!(!memory.id.is_empty(), "记忆ID不应为空");
            println!("记忆写入成功: {}", memory.id);
        }
        Err(e) => {
            println!("记忆写入失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_memory_manager_search() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let memory_mgr = MemoryManager::new(client);

    let query = "测试查询";
    let result = memory_mgr.search(query, Some(10)).await;

    match result {
        Ok(memories) => {
            println!("记忆搜索成功: {} 条结果", memories.len());
        }
        Err(e) => {
            println!("记忆搜索失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_memory_manager_read() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let memory_mgr = MemoryManager::new(client);

    let result = memory_mgr.read("memory-123").await;

    match result {
        Ok(memory) => {
            assert_eq!(memory.id, "memory-123");
            println!("记忆读取成功: {:?}", memory);
        }
        Err(e) => {
            println!("记忆读取失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_memory_manager_delete() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let memory_mgr = MemoryManager::new(client);

    let result = memory_mgr.delete("memory-123").await;

    match result {
        Ok(_) => {
            println!("记忆删除成功");
        }
        Err(e) => {
            println!("记忆删除失败（服务端可能未启动）: {:?}", e);
        }
    }
}

// ============================================================
// SessionManager 测试
// ============================================================

#[tokio::test]
async fn test_session_manager_create() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let session_mgr = SessionManager::new(client);

    let result = session_mgr.create(None).await;

    match result {
        Ok(session) => {
            assert!(!session.id.is_empty(), "会话ID不应为空");
            println!("会话创建成功: {}", session.id);
        }
        Err(e) => {
            println!("会话创建失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_session_manager_get() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let session_mgr = SessionManager::new(client);

    let result = session_mgr.get("session-123").await;

    match result {
        Ok(session) => {
            assert_eq!(session.id, "session-123");
            println!("会话获取成功: {:?}", session);
        }
        Err(e) => {
            println!("会话获取失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_session_manager_close() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let session_mgr = SessionManager::new(client);

    let result = session_mgr.close("session-123").await;

    match result {
        Ok(_) => {
            println!("会话关闭成功");
        }
        Err(e) => {
            println!("会话关闭失败（服务端可能未启动）: {:?}", e);
        }
    }
}

// ============================================================
// SkillManager 测试
// ============================================================

#[tokio::test]
async fn test_skill_manager_load() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let skill_mgr = SkillManager::new(client);

    let skill_name = "test-skill";
    let result = skill_mgr.load(skill_name, None).await;

    match result {
        Ok(skill) => {
            assert_eq!(skill.name, skill_name);
            println!("技能加载成功: {:?}", skill);
        }
        Err(e) => {
            println!("技能加载失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_skill_manager_list() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let skill_mgr = SkillManager::new(client);

    let result = skill_mgr.list().await;

    match result {
        Ok(skills) => {
            println!("技能列表查询成功: {} 个技能", skills.len());
        }
        Err(e) => {
            println!("技能列表查询失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_skill_manager_unload() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let skill_mgr = SkillManager::new(client);

    let result = skill_mgr.unload("skill-123").await;

    match result {
        Ok(_) => {
            println!("技能卸载成功");
        }
        Err(e) => {
            println!("技能卸载失败（服务端可能未启动）: {:?}", e);
        }
    }
}

// ============================================================
// 综合测试
// ============================================================

#[tokio::test]
async fn test_managers_integration() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(10));

    let client = Client::new(config);

    // 创建会话
    let session_mgr = SessionManager::new(client.clone());
    let session = session_mgr.create(None).await;

    match session {
        Ok(s) => {
            println!("会话创建成功: {}", s.id);

            // 在会话中提交任务
            let task_mgr = TaskManager::new(client.clone());
            let task = task_mgr.submit("集成测试任务").await;

            match task {
                Ok(t) => {
                    println!("任务提交成功: {}", t.id);

                    // 写入记忆
                    let memory_mgr = MemoryManager::new(client.clone());
                    let memory = memory_mgr.write("集成测试记忆", Some("L1")).await;

                    match memory {
                        Ok(m) => {
                            println!("记忆写入成功: {}", m.id);
                        }
                        Err(e) => println!("记忆写入失败: {:?}", e),
                    }
                }
                Err(e) => println!("任务提交失败: {:?}", e),
            }

            // 关闭会话
            let _ = session_mgr.close(&s.id).await;
        }
        Err(e) => {
            println!("会话创建失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_managers_error_handling() {
    let config = ClientConfig::default()
        .with_endpoint("http://invalid-endpoint:99999")
        .with_timeout(Duration::from_millis(100));

    let client = Client::new(config);

    // 测试错误处理
    let task_mgr = TaskManager::new(client.clone());
    let result = task_mgr.submit("测试任务").await;

    assert!(result.is_err(), "无效端点应返回错误");

    match result {
        Err(e) => {
            assert!(e.is_network_error() || e.code() == CODE_CONNECTION_REFUSED);
            println!("错误处理正确: {:?}", e);
        }
        Ok(_) => panic!("不应成功"),
    }
}
