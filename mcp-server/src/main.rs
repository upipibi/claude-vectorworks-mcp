mod bridge;
mod server;

use anyhow::Result;
use rmcp::ServiceExt;
use tracing_subscriber::EnvFilter;

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_ansi(false)
        .with_env_filter(EnvFilter::from_default_env().add_directive("info".parse()?))
        .init();

    tracing::info!("Vectorworks MCP server starting...");

    if bridge::is_connected() {
        tracing::info!("Vectorworks bridge: connected");
    } else {
        tracing::warn!("Vectorworks bridge: not connected (is the plugin loaded and the MCP Bridge started?)");
    }

    let server = server::VwMcpServer::new();
    let service = server.serve(rmcp::transport::stdio()).await?;

    tracing::info!("Vectorworks MCP server running on stdio");
    service.waiting().await?;
    Ok(())
}
