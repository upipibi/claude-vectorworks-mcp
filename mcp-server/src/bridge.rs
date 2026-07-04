use anyhow::{bail, Context, Result};
use serde::{Deserialize, Serialize};
use std::io::{BufRead, BufReader, Read, Write};
use std::os::unix::net::UnixStream;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

/// Path of the Unix domain socket the Vectorworks plugin listens on.
/// Must match the path used by the C++ plugin (see plugin/src/ExtMcpBridge.cpp).
const SOCKET_PATH: &str = "/tmp/vw-mcp-bridge.sock";

/// Hard cap on a single response, so a misbehaving or hostile peer holding the
/// socket cannot stream an endless (newline-less) reply and drive this process
/// out of memory.
const MAX_RESPONSE_BYTES: u64 = 16 * 1024 * 1024; // 16 MiB

static COUNTER: AtomicU64 = AtomicU64::new(0);

#[derive(Serialize)]
struct Request {
    id: String,
    method: String,
    params: serde_json::Value,
}

#[derive(Debug, Deserialize)]
pub struct Response {
    #[allow(dead_code)]
    pub id: String,
    pub success: bool,
    pub data: serde_json::Value,
}

pub fn send(method: &str, params: serde_json::Value) -> Result<Response> {
    let stream = UnixStream::connect(SOCKET_PATH)
        .context("Could not connect to Vectorworks. Is the plugin loaded and the MCP Bridge started?")?;
    stream.set_read_timeout(Some(Duration::from_secs(15)))?;
    stream.set_write_timeout(Some(Duration::from_secs(5)))?;

    let req = Request {
        id: format!("r_{}", COUNTER.fetch_add(1, Ordering::Relaxed)),
        method: method.to_string(),
        params,
    };

    let mut msg = serde_json::to_string(&req)?;
    msg.push('\n');
    (&stream).write_all(msg.as_bytes())?;
    (&stream).flush()?;

    // Bound the read so an unbounded / newline-less response cannot OOM us.
    let mut reader = BufReader::new((&stream).take(MAX_RESPONSE_BYTES));
    let mut line = String::new();
    let n = reader.read_line(&mut line)?;
    if n == 0 {
        bail!("Vectorworks bridge closed the connection before sending a response");
    }
    if line.len() as u64 >= MAX_RESPONSE_BYTES {
        bail!("Response from Vectorworks exceeded {MAX_RESPONSE_BYTES} bytes — aborting");
    }

    serde_json::from_str(line.trim()).context("Failed to parse the response from Vectorworks")
}

pub fn is_connected() -> bool {
    send("ping", serde_json::Value::Null).is_ok()
}
