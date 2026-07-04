# claude-vectorworks-mcp

Give an MCP client (such as [Claude Code](https://claude.com/claude-code)) the
ability to **read and drive a running Vectorworks 2026 document** — query the
document, list layers, draw basic shapes, and (optionally) run Python /
VectorScript inside Vectorworks and get the output back.

Adapted from [mako-357/vectorworks-mcp](https://github.com/mako-357/vectorworks-mcp)
(MIT). That project provides a Rust MCP server plus C++ plugin *source* but no
plugin build system, and a couple of code bugs. This repository is a cleaned-up,
security-reviewed, **source-only** version: a hardened Rust server, a refactored
plugin split into an MIT socket layer and a clearly-delineated Vectorworks-SDK
layer, build instructions, and honest documentation of what it is and isn't safe
to do.

> ⚠️ **Read [SECURITY.md](SECURITY.md) before using this.** This tool exposes a
> running Vectorworks document to an MCP client over a **local, unauthenticated
> socket**, and can optionally execute **arbitrary code** inside Vectorworks. It
> is a power tool for a trusted, single-user setup — not a sandbox. The
> script-execution tool is **disabled by default** and must be explicitly
> enabled (see below). SDK access happens on a background thread, so writing to
> a real document can crash or corrupt it — **work on a throwaway copy.**

Not affiliated with, endorsed by, or certified by Vectorworks, Inc.

---

## How it works

```
MCP client (e.g. Claude Code)
        │  stdio (MCP protocol)
        ▼
vectorworks-mcp-server            ← Rust, this repo (mcp-server/)
        │  Unix domain socket  /tmp/vw-mcp-bridge.sock   (newline-delimited JSON)
        ▼
"MCP Bridge" plugin               ← C++ menu extension, this repo (plugin/)
        │  in-process
        ▼
Vectorworks 2026 (VCOM SDK)
```

The Rust server speaks MCP to the client on one side and connects to a Unix
socket on the other. A small Vectorworks plugin listens on that socket and
translates JSON commands into SDK calls inside the running application.

## Tools

| Tool | What it does |
|------|--------------|
| `get_drawing_info` | Whether a document is open, and a layer count. |
| `list_layers` | Names of the design layers in the active document. |
| `create_line` / `create_rect` / `create_circle` | Draw a basic shape (document units). |
| `run_script` | Run Python (default; the full `vs` API, stdout/stderr captured) or VectorScript inside Vectorworks. **Disabled by default** — see below. |

`run_script` with Python is the real workhorse: `import vs`, do anything the
Vectorworks Python API allows, `print()` the results, and they come back to the
client.

## Repository layout

```
mcp-server/          Rust MCP server (builds standalone with cargo)
  Cargo.toml
  Cargo.lock
  src/{main,bridge,server}.rs
plugin/
  src/               C++ plugin source (build against your OWN Vectorworks SDK)
    SocketBridge.{h,cpp}    ← MIT (socket + dispatch), no SDK-specific code
    ExtMcpBridge.{h,cpp}    ← Vectorworks-SDK-derived; subject to the SDK license
    ExtMcpBridge.vwstrings  ← menu strings (UTF-16LE)
  BUILD.md           How to build the plugin against the Vectorworks SDK
LICENSE  NOTICE  SECURITY.md
```

No compiled binaries are shipped (see [NOTICE](NOTICE) for why). You build both
halves yourself.

## Prerequisites

- **macOS on Apple Silicon** and **Vectorworks 2026** (the plugin is built and
  tested there; other platforms/versions will need adjustment).
- **Rust** 1.85+ (edition 2024) — <https://rustup.rs>.
- For the plugin: **Xcode**, and your own licensed copy of the **Vectorworks
  SDK** from the [Vectorworks Developer program](https://developer.vectorworks.net/).
  See [plugin/BUILD.md](plugin/BUILD.md).

## Setup

### 1. Build the Rust MCP server

```bash
cd mcp-server
cargo build --release --locked
# binary: mcp-server/target/release/vectorworks-mcp-server
```

> On Apple Silicon, if you rebuild and copy the binary elsewhere, an incremental
> relink can leave a stale ad-hoc code signature and macOS will `SIGKILL` it on
> launch with no output (the MCP client just reports "failed to connect").
> Re-sign after copying: `codesign --force --sign - <binary>`.

### 2. Build and install the plugin

Follow [plugin/BUILD.md](plugin/BUILD.md). In short: obtain the Vectorworks SDK,
drop `plugin/src/*` into the SDK's `ProcessResources` menu-extension example,
register the extension in `ModuleMain.cpp`, build, ad-hoc sign, and copy the
resulting `.vwlibrary` into:

```
~/Library/Application Support/Vectorworks/2026/Plug-Ins/
```

Then in Vectorworks add the **MCP → MCP Bridge: Start/Stop** command to your
workspace (Tools → Workspaces → Edit Current Workspace…), and run it to start
the socket bridge.

### 3. Register the server with your MCP client

For Claude Code (user scope):

```bash
claude mcp add vectorworks --scope user -- /absolute/path/to/vectorworks-mcp-server
```

To also enable script execution (see the warning below), add the environment
variable:

```bash
claude mcp add vectorworks --scope user \
  -e VW_MCP_ENABLE_RUN_SCRIPT=1 \
  -- /absolute/path/to/vectorworks-mcp-server
```

Restart the client so it picks up the server, then ask it to
`get_drawing_info`.

## Enabling `run_script`

`run_script` executes arbitrary code inside Vectorworks and is **off by
default**. It only runs when the environment variable `VW_MCP_ENABLE_RUN_SCRIPT`
is set to `1` for the server process (see step 3). Enable it only in a trusted,
single-user setup where you understand that anything reaching the MCP client
(including text inside a document you open) could, in principle, drive it. See
[SECURITY.md](SECURITY.md).

## Limitations & known risks

- **Unauthenticated local socket.** Anything able to reach
  `/tmp/vw-mcp-bridge.sock` can drive Vectorworks. The socket is created
  `0600` (this user only), but treat this as a trusted-local-machine tool.
- **`run_script` = arbitrary code execution** inside Vectorworks (by design).
- **Thread-safety.** SDK calls run on the socket's background thread; the
  Vectorworks SDK is not guaranteed thread-safe for document access, so writes
  and scripts can crash or silently corrupt the open document. **Use a
  throwaway copy**, and save/close real work first. A proper fix (marshalling
  SDK calls onto Vectorworks' main thread) is not yet implemented.
- macOS / Apple Silicon / Vectorworks 2026 only, as shipped.

## Credits & license

- Socket-bridge + command-dispatch design derived from
  [mako-357/vectorworks-mcp](https://github.com/mako-357/vectorworks-mcp) (MIT),
  which credits `autocad-mcp`.
- The Rust MCP server uses [`rmcp`](https://crates.io/crates/rmcp).
- The plugin depends on the proprietary **Vectorworks SDK** (your own license
  required) and uses **nlohmann/json** (MIT), supplied by the SDK.

Original code and mako-derived portions are **MIT** (see [LICENSE](LICENSE)).
The Vectorworks-SDK-derived plugin source is subject to the **Vectorworks SDK
License Agreement**, not MIT — see [NOTICE](NOTICE).
