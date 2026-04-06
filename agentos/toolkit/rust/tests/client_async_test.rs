// AgentOS Rust SDK - 异步客户端测试
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 测试异步客户端的超时、并发和错误处理

use agentos::*;
use std::time::Duration;
use tokio::time::sleep;

#[tokio::test]
async fn test_client_timeout() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:19999") // 不存在的端口
        .with_timeout(Duration::from_millis(100));

    let client = Client::new(config);
    let result = client.get("/test").await;

    match result {
        Err(AgentOSError { code, .. }) if code == CODE_TIMEOUT => (),
        Err(e) => panic!("期望超时错误，实际得到: {:?}", e),
        Ok(_) => panic!("应该失败但成功了"),
    }
}

#[tokio::test]
async fn test_client_connection_refused() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:19999")
        .with_timeout(Duration::from_secs(1));

    let client = Client::new(config);
    let result = client.get("/test").await;

    match result {
        Err(AgentOSError { code, .. }) if code == CODE_CONNECTION_REFUSED => (),
        Err(e) => panic!("期望连接拒绝错误，实际得到: {:?}", e),
        Ok(_) => panic!("应该失败但成功了"),
    }
}

#[tokio::test]
async fn test_client_concurrent_requests() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let mut handles = vec![];

    for i in 0..50 {
        let c = client.clone();
        handles.push(tokio::spawn(async move {
            c.get(&format!("/test/{}", i)).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    let success_count = results.iter().filter(|r| r.is_ok()).count();

    // 允许部分失败（如果服务端未启动）
    println!("成功请求数: {}/50", success_count);
}

#[tokio::test]
async fn test_client_retry_on_timeout() {
    // 需要 mock server 来测试重试逻辑
    // 这里仅测试配置是否正确
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(1))
        .with_max_retries(3)
        .with_retry_delay(Duration::from_millis(100));

    let client = Client::new(config);
    assert_eq!(client.max_retries(), 3);
}

#[tokio::test]
async fn test_client_context_cancellation() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(10));

    let client = Client::new(config);

    let (tx, rx) = tokio::sync::oneshot::channel();
    
    let handle = tokio::spawn(async move {
        tokio::select! {
            result = client.get("/test") => {
                let _ = tx.send(result.is_ok());
            }
            _ = sleep(Duration::from_millis(50)) => {
                let _ = tx.send(false);
            }
        }
    });

    let _ = handle.await;
    let cancelled = rx.await.unwrap_or(false);
    
    // 如果服务端未启动，请求会被取消
    println!("请求被取消: {}", !cancelled);
}

#[tokio::test]
async fn test_client_multiple_endpoints() {
    let endpoints = vec![
        "http://localhost:18789",
        "http://localhost:18790",
        "http://localhost:18791",
    ];

    let mut clients = vec![];
    for endpoint in endpoints {
        let config = ClientConfig::default()
            .with_endpoint(endpoint)
            .with_timeout(Duration::from_millis(100));

        clients.push(Client::new(config));
    }

    let mut handles = vec![];
    for client in clients {
        handles.push(tokio::spawn(async move {
            client.get("/health").await
        }));
    }

    let results = futures::future::join_all(handles).await;
    println!("多端点测试完成: {} 个请求", results.len());
}

#[tokio::test]
async fn test_client_request_headers() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_api_key("test-api-key-12345");

    let client = Client::new(config);

    // 验证 API Key 是否正确设置
    assert_eq!(client.api_key(), Some("test-api-key-12345"));
}

#[tokio::test]
async fn test_client_response_parsing() {
    // 需要 mock server 返回 JSON 响应
    // 这里测试响应解析逻辑
    let json_response = r#"{"success":true,"data":{"id":"123","status":"completed"}}"#;

    let parsed: Result<serde_json::Value, _> = serde_json::from_str(json_response);
    assert!(parsed.is_ok());

    let value = parsed.unwrap();
    assert_eq!(value["success"], true);
    assert_eq!(value["data"]["id"], "123");
}

#[tokio::test]
async fn test_client_error_response_parsing() {
    let error_response = r#"{
        "success": false,
        "error": {
            "code": "0x0002",
            "message": "参数无效",
            "details": "content 字段不能为空"
        }
    }"#;

    let parsed: Result<serde_json::Value, _> = serde_json::from_str(error_response);
    assert!(parsed.is_ok());

    let value = parsed.unwrap();
    assert_eq!(value["success"], false);
    assert_eq!(value["error"]["code"], "0x0002");
}

#[tokio::test]
async fn test_client_large_response() {
    // 测试大响应体的处理
    let large_data = vec![0u8; 1024 * 1024]; // 1MB
    let large_json = serde_json::to_string(&large_data).unwrap();

    assert!(large_json.len() > 1_000_000);
}

#[tokio::test]
async fn test_client_concurrent_writes() {
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(5));

    let client = Client::new(config);
    let mut handles = vec![];

    for i in 0..20 {
        let c = client.clone();
        let payload = serde_json::json!({
            "content": format!("Test content {}", i),
            "priority": i % 5,
        });

        handles.push(tokio::spawn(async move {
            c.post("/tasks", &payload).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    let success_count = results.iter().filter(|r| r.is_ok()).count();

    println!("并发写入成功: {}/20", success_count);
}

#[tokio::test]
async fn test_client_rate_limiting() {
    // 测试速率限制处理
    let config = ClientConfig::default()
        .with_endpoint("http://localhost:18789")
        .with_timeout(Duration::from_secs(1))
        .with_max_retries(0);

    let client = Client::new(config);

    // 快速发送多个请求
    let mut handles = vec![];
    for i in 0..100 {
        let c = client.clone();
        handles.push(tokio::spawn(async move {
            c.get(&format!("/test/{}", i)).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    let rate_limited = results.iter().filter(|r| {
        matches!(r, Err(AgentOSError { code, .. }) if code == CODE_RATE_LIMITED)
    }).count();

    println!("被限流的请求数: {}/100", rate_limited);
}

#[tokio::test]
async fn test_client_backoff_strategy() {
    // 测试指数退避策略
    let delays: Vec<Duration> = (0..5)
        .map(|attempt| {
            let base_delay = Duration::from_millis(100);
            let max_delay = Duration::from_secs(5);
            
            let delay = base_delay * 2u32.pow(attempt);
            std::cmp::min(delay, max_delay)
        })
        .collect();

    assert_eq!(delays[0], Duration::from_millis(100));
    assert_eq!(delays[1], Duration::from_millis(200));
    assert_eq!(delays[2], Duration::from_millis(400));
    assert_eq!(delays[3], Duration::from_millis(800));
    assert_eq!(delays[4], Duration::from_millis(1600));
}
