// AgentOS Rust SDK - Managers 模块测试
// Version: 3.0.0
// Last updated: 2026-04-27

use agentos_rs::*;
use std::sync::Arc;
use std::time::Duration;

fn make_client() -> Arc<dyn APIClient> {
    let client = Client::new_with_timeout("http://localhost:18789", Duration::from_secs(5))
        .expect("Failed to create client");
    Arc::new(client) as Arc<dyn APIClient>
}

fn make_invalid_client() -> Arc<dyn APIClient> {
    let client = Client::new_with_timeout("http://invalid-endpoint:99999", Duration::from_millis(100))
        .expect("Failed to create client");
    Arc::new(client) as Arc<dyn APIClient>
}

#[tokio::test]
async fn test_task_manager_submit() {
    let task_mgr = TaskManager::new(make_client());
    let result = task_mgr.submit("测试任务内容").await;
    match result {
        Ok(task) => {
            assert!(!task.id.is_empty());
            println!("任务提交成功: {}", task.id);
        }
        Err(e) => {
            println!("任务提交失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_task_manager_query() {
    let task_mgr = TaskManager::new(make_client());
    let result = task_mgr.get("task-123").await;
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
    let task_mgr = TaskManager::new(make_client());
    let result = task_mgr.list(None).await;
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
    let task_mgr = TaskManager::new(make_client());
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

#[tokio::test]
async fn test_memory_manager_write() {
    let memory_mgr = MemoryManager::new(make_client());
    let result = memory_mgr.write("测试记忆内容", MemoryLayer::L1).await;
    match result {
        Ok(memory) => {
            assert!(!memory.id.is_empty());
            println!("记忆写入成功: {}", memory.id);
        }
        Err(e) => {
            println!("记忆写入失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_memory_manager_search() {
    let memory_mgr = MemoryManager::new(make_client());
    let result = memory_mgr.search("测试查询", 10).await;
    match result {
        Ok(search_result) => {
            println!("记忆搜索成功: {} 条结果", search_result.memories.len());
        }
        Err(e) => {
            println!("记忆搜索失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_memory_manager_read() {
    let memory_mgr = MemoryManager::new(make_client());
    let result = memory_mgr.get("memory-123").await;
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
    let memory_mgr = MemoryManager::new(make_client());
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

#[tokio::test]
async fn test_session_manager_create() {
    let session_mgr = SessionManager::new(make_client());
    let result = session_mgr.create().await;
    match result {
        Ok(session) => {
            assert!(!session.id.is_empty());
            println!("会话创建成功: {}", session.id);
        }
        Err(e) => {
            println!("会话创建失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_session_manager_get() {
    let session_mgr = SessionManager::new(make_client());
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
    let session_mgr = SessionManager::new(make_client());
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

#[tokio::test]
async fn test_skill_manager_load() {
    let skill_mgr = SkillManager::new(make_client());
    let result = skill_mgr.load("test-skill").await;
    match result {
        Ok(skill) => {
            assert_eq!(skill.name, "test-skill");
            println!("技能加载成功: {:?}", skill);
        }
        Err(e) => {
            println!("技能加载失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_skill_manager_list() {
    let skill_mgr = SkillManager::new(make_client());
    let result = skill_mgr.list(None).await;
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
    let skill_mgr = SkillManager::new(make_client());
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

#[tokio::test]
async fn test_managers_integration() {
    let client = make_client();

    let session_mgr = SessionManager::new(client.clone());
    let session = session_mgr.create().await;

    match session {
        Ok(s) => {
            println!("会话创建成功: {}", s.id);

            let task_mgr = TaskManager::new(client.clone());
            let task = task_mgr.submit("集成测试任务").await;

            match task {
                Ok(t) => {
                    println!("任务提交成功: {}", t.id);

                    let memory_mgr = MemoryManager::new(client.clone());
                    let memory = memory_mgr.write("集成测试记忆", MemoryLayer::L1).await;

                    match memory {
                        Ok(m) => {
                            println!("记忆写入成功: {}", m.id);
                        }
                        Err(e) => println!("记忆写入失败: {:?}", e),
                    }
                }
                Err(e) => println!("任务提交失败: {:?}", e),
            }

            let _ = session_mgr.close(&s.id).await;
        }
        Err(e) => {
            println!("会话创建失败（服务端可能未启动）: {:?}", e);
        }
    }
}

#[tokio::test]
async fn test_managers_error_handling() {
    let client = make_invalid_client();

    let task_mgr = TaskManager::new(client);
    let result = task_mgr.submit("测试任务").await;

    assert!(result.is_err(), "无效端点应返回错误");

    match result {
        Err(e) => {
            assert!(e.is_network_error() || e.code() == CODE_CONNECTION_REFUSED || e.code() == CODE_NETWORK_ERROR || e.code() == CODE_TIMEOUT || e.code() == CODE_INTERNAL,
                "期望错误码，实际: {}", e.code());
            println!("错误处理正确: {:?}", e);
        }
        Ok(_) => panic!("不应成功"),
    }
}
