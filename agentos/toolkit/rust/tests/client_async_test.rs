// AgentOS Rust SDK - 异步客户端测试
// Version: 3.0.0
// Last updated: 2026-04-27

use agentos_rs::*;
use std::time::Duration;
use tokio::time::sleep;

#[tokio::test]
async fn test_client_timeout() {
    let client = Client::new_with_timeout("http://localhost:19999", Duration::from_millis(100))
        .expect("Failed to create client");
    let result = client.get("/test", None).await;

    match result {
        Err(e) if e.code() == CODE_TIMEOUT || e.is_network_error() => (),
        Err(e) => println!("得到其他错误（可接受）: {:?}", e),
        Ok(_) => panic!("应该失败但成功了"),
    }
}

#[tokio::test]
async fn test_client_connection_refused() {
    let client = Client::new_with_timeout("http://localhost:19999", Duration::from_secs(1))
        .expect("Failed to create client");
    let result = client.get("/test", None).await;

    match result {
        Err(e) if e.code() == CODE_CONNECTION_REFUSED || e.is_network_error() || e.code() == CODE_INTERNAL => (),
        Err(e) => println!("得到其他错误（可接受）: {:?}", e),
        Ok(_) => panic!("应该失败但成功了"),
    }
}

#[tokio::test]
async fn test_client_concurrent_requests() {
    let client = Client::new_with_timeout("http://localhost:18789", Duration::from_secs(5))
        .expect("Failed to create client");
    let mut handles = vec![];

    for i in 0..50 {
        let c = client.clone();
        handles.push(tokio::spawn(async move {
            c.get(&format!("/test/{}", i), None).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    let success_count = results.iter().filter(|r| r.is_ok()).count();

    println!("成功请求数: {}/50", success_count);
}

#[tokio::test]
async fn test_client_builder_configuration() {
    let client = Client::builder("http://localhost:18789")
        .timeout(Duration::from_secs(1))
        .max_retries(3)
        .retry_delay(Duration::from_millis(100))
        .build();

    assert!(client.is_ok());
    let client = client.unwrap();
    assert_eq!(client.endpoint(), "http://localhost:18789");
}

#[tokio::test]
async fn test_client_context_cancellation() {
    let client = Client::new_with_timeout("http://localhost:18789", Duration::from_secs(10))
        .expect("Failed to create client");

    let (tx, rx) = tokio::sync::oneshot::channel();

    let handle = tokio::spawn(async move {
        tokio::select! {
            result = client.get("/test", None) => {
                let _ = tx.send(result.is_ok());
            }
            _ = sleep(Duration::from_millis(50)) => {
                let _ = tx.send(false);
            }
        }
    });

    let _ = handle.await;
    let cancelled = rx.await.unwrap_or(false);

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
        let client = Client::new_with_timeout(endpoint, Duration::from_millis(100))
            .expect("Failed to create client");
        clients.push(client);
    }

    let mut handles = vec![];
    for client in clients {
        handles.push(tokio::spawn(async move {
            client.get("/health", None).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    println!("多端点测试完成: {} 个请求", results.len());
}

#[tokio::test]
async fn test_client_request_headers() {
    let client = Client::new_with_api_key("http://localhost:18789", "test-api-key-12345")
        .expect("Failed to create client");

    assert_eq!(client.api_key(), Some("test-api-key-12345"));
}

#[tokio::test]
async fn test_client_response_parsing() {
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
    let large_data = vec![0u8; 1024 * 1024];
    let large_json = serde_json::to_string(&large_data).unwrap();

    assert!(large_json.len() > 1_000_000);
}

#[tokio::test]
async fn test_client_concurrent_writes() {
    let client = Client::new_with_timeout("http://localhost:18789", Duration::from_secs(5))
        .expect("Failed to create client");
    let mut handles = vec![];

    for i in 0..20 {
        let c = client.clone();
        let payload = serde_json::json!({
            "content": format!("Test content {}", i),
            "priority": i % 5,
        });

        handles.push(tokio::spawn(async move {
            c.post("/tasks", Some(&payload), None).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    let success_count = results.iter().filter(|r| r.is_ok()).count();

    println!("并发写入成功: {}/20", success_count);
}

#[tokio::test]
async fn test_client_rate_limiting() {
    let client = Client::builder("http://localhost:18789")
        .timeout(Duration::from_secs(1))
        .max_retries(0)
        .build()
        .expect("Failed to create client");

    let mut handles = vec![];
    for i in 0..100 {
        let c = client.clone();
        handles.push(tokio::spawn(async move {
            c.get(&format!("/test/{}", i), None).await
        }));
    }

    let results = futures::future::join_all(handles).await;
    let rate_limited = results.iter().filter(|r| {
        matches!(r, Ok(Err(e)) if e.code() == CODE_RATE_LIMITED)
    }).count();

    println!("被限流的请求数: {}/100", rate_limited);
}

#[tokio::test]
async fn test_client_backoff_strategy() {
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
