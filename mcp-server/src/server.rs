use rmcp::handler::server::router::tool::ToolRouter;
use rmcp::handler::server::wrapper::Parameters;
use rmcp::model::{ServerCapabilities, ServerInfo};
use rmcp::{schemars, tool, tool_handler, tool_router, ServerHandler};
use schemars::JsonSchema;
use serde::Deserialize;
use serde_json::json;

use crate::bridge;

/// Environment variable that must equal "1" to allow `run_script`.
/// `run_script` executes arbitrary code inside Vectorworks, so it is off by
/// default and only enabled by an explicit, informed opt-in. See SECURITY.md.
const RUN_SCRIPT_ENABLE_VAR: &str = "VW_MCP_ENABLE_RUN_SCRIPT";

fn run_script_enabled() -> bool {
    std::env::var(RUN_SCRIPT_ENABLE_VAR).as_deref() == Ok("1")
}

#[derive(Clone)]
pub struct VwMcpServer {
    #[allow(dead_code)]
    tool_router: ToolRouter<Self>,
}

impl VwMcpServer {
    pub fn new() -> Self {
        let tool_router = Self::tool_router();
        Self { tool_router }
    }
}

// --- Input schemas ---

#[derive(Debug, Deserialize, JsonSchema)]
pub struct EmptyInput {}

#[derive(Debug, Deserialize, JsonSchema)]
pub struct CreateLineInput {
    pub x1: f64,
    pub y1: f64,
    pub x2: f64,
    pub y2: f64,
}

#[derive(Debug, Deserialize, JsonSchema)]
pub struct CreateRectInput {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
}

#[derive(Debug, Deserialize, JsonSchema)]
pub struct CreateCircleInput {
    pub cx: f64,
    pub cy: f64,
    pub radius: f64,
}

#[derive(Debug, Deserialize, JsonSchema)]
pub struct RunScriptInput {
    #[schemars(description = "Script source. For python, the full `vs` API is available (import vs); use print() to return data.")]
    pub script: String,
    #[schemars(description = "'python' (default, captures stdout/stderr) or 'vectorscript' (runs, no output captured)")]
    pub language: Option<String>,
}

fn call(method: &str, params: serde_json::Value) -> String {
    match bridge::send(method, params) {
        Ok(r) if r.success => {
            serde_json::to_string_pretty(&r.data).unwrap_or_else(|_| r.data.to_string())
        }
        Ok(r) => format!("Error: {}", r.data),
        Err(e) => format!("Connection error: {}", e),
    }
}

#[tool_router]
impl VwMcpServer {
    #[tool(name = "get_drawing_info", description = "Get information about the active Vectorworks document (whether a document is open, and a layer count).")]
    async fn get_drawing_info(&self, Parameters(_): Parameters<EmptyInput>) -> String {
        call("get_drawing_info", json!({}))
    }

    #[tool(name = "list_layers", description = "List the design layers in the active Vectorworks document.")]
    async fn list_layers(&self, Parameters(_): Parameters<EmptyInput>) -> String {
        call("list_layers", json!({}))
    }

    #[tool(name = "create_line", description = "Draw a line from (x1, y1) to (x2, y2) in document units.")]
    async fn create_line(&self, Parameters(i): Parameters<CreateLineInput>) -> String {
        call("create_line", json!({"x1": i.x1, "y1": i.y1, "x2": i.x2, "y2": i.y2}))
    }

    #[tool(name = "create_rect", description = "Draw a rectangle with bottom-left corner (x, y) and the given width and height, in document units.")]
    async fn create_rect(&self, Parameters(i): Parameters<CreateRectInput>) -> String {
        call("create_rect", json!({"x": i.x, "y": i.y, "width": i.width, "height": i.height}))
    }

    #[tool(name = "create_circle", description = "Draw a circle centered at (cx, cy) with the given radius, in document units.")]
    async fn create_circle(&self, Parameters(i): Parameters<CreateCircleInput>) -> String {
        call("create_circle", json!({"cx": i.cx, "cy": i.cy, "radius": i.radius}))
    }

    #[tool(
        name = "run_script",
        description = "Run a Python (default) or VectorScript script inside Vectorworks and return its output. DISABLED BY DEFAULT: it executes arbitrary code inside Vectorworks. Enable by setting the environment variable VW_MCP_ENABLE_RUN_SCRIPT=1 for this server (see SECURITY.md). Trusted environment only."
    )]
    async fn run_script(&self, Parameters(i): Parameters<RunScriptInput>) -> String {
        if !run_script_enabled() {
            return format!(
                "run_script is disabled. It executes arbitrary code inside Vectorworks, so it is off by default. \
                 To enable it, set the environment variable {RUN_SCRIPT_ENABLE_VAR}=1 for this MCP server (see SECURITY.md)."
            );
        }
        call(
            "run_script",
            json!({"script": i.script, "language": i.language.unwrap_or_else(|| "python".into())}),
        )
    }
}

#[tool_handler]
impl ServerHandler for VwMcpServer {
    fn get_info(&self) -> ServerInfo {
        let mut info = ServerInfo::new(ServerCapabilities::builder().enable_tools().build());
        info.instructions = Some(
            "Vectorworks MCP server. Reads and drives a running Vectorworks 2026 document: \
             query document info, list layers, draw basic shapes, and (only if explicitly enabled) run scripts."
                .into(),
        );
        info
    }
}
