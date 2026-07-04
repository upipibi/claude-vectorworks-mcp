# Security Policy

## Trust model — read this before using the tool

`claude-vectorworks-mcp` is a **power tool for a trusted, single-user, local
setup**, not a sandboxed or multi-tenant service. Understand these properties
before you run it:

### 1. The bridge is a local, unauthenticated socket

The plugin listens on a Unix domain socket (`/tmp/vw-mcp-bridge.sock`). There is
**no authentication**: any process that can open that socket can send commands
to the running Vectorworks document. Mitigations in place:

- The socket is created with `0600` permissions (owner only), so other local
  users cannot connect.
- It is a Unix domain socket, not a network port — there is no remote exposure.

It is still, by design, a channel that drives your Vectorworks application.
Treat the machine it runs on as trusted.

### 2. `run_script` is arbitrary code execution — off by default

`run_script` runs Python or VectorScript **inside Vectorworks**, with the full
`vs` API. That is arbitrary code execution in the context of your user account.
Because a capable MCP client can be influenced by the content it reads
(including text inside a document you open — i.e. prompt injection), this tool
is **disabled by default**.

To enable it, set `VW_MCP_ENABLE_RUN_SCRIPT=1` in the environment of the MCP
server process. Do this only when you understand and accept the risk. When it is
not set, `run_script` refuses to execute and returns an explanatory message.

### 3. SDK calls run on a background thread (crash / corruption risk)

The socket handler calls the Vectorworks SDK from a background thread. The SDK
is not guaranteed thread-safe for document access, so **writes and scripts can
crash Vectorworks or silently corrupt the open document.**

- Work on a **throwaway copy** of any drawing you care about.
- Save and close real work before using write/script tools.
- A proper fix marshals SDK calls onto Vectorworks' main thread; it is not yet
  implemented. Read-only queries (`get_drawing_info`, `list_layers`) have been
  reliable in practice, but the caveat still applies.

### 4. No binaries are distributed

This repository ships source only. You build the Rust server and the plugin
yourself, so you are running code you compiled. See [NOTICE](NOTICE) for the
licensing reasons (the plugin links proprietary Vectorworks object code).

## Supported configuration

Built and tested on macOS (Apple Silicon) with Vectorworks 2026. Other
platforms or versions are unsupported as shipped and may need code changes.

## Reporting a vulnerability

Please report security issues **privately** — do not open a public issue.
Use GitHub's **"Report a vulnerability"** button on the repository's *Security*
tab (Private Vulnerability Reporting). Include repro steps and impact. You'll
get a response as soon as reasonably possible; this is a hobby project, so
please be patient.
