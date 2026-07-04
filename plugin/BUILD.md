# Building the MCP Bridge plugin

The plugin is a Vectorworks **menu extension**. It has no standalone build
system: you build it against your **own licensed copy of the Vectorworks SDK**,
by grafting the source in `plugin/src/` onto the SDK's `ProcessResources`
example (which already ships a working Xcode project for 2026).

> The plugin source depends on the proprietary Vectorworks SDK and is subject to
> the Vectorworks SDK License Agreement — see [../NOTICE](../NOTICE). You must
> obtain the SDK yourself via the [Vectorworks Developer program](https://developer.vectorworks.net/).

## Prerequisites

- Xcode (the plugin here was built with Xcode 26.x against the 2026 SDK).
- The **Vectorworks SDK / SDKExamples** for 2026:
  <https://github.com/VectorworksDeveloper/SDKExamples> (this repo contains the
  SDK headers and libraries plus the example projects).
- Vectorworks 2026 installed (to load and test the plugin).

## Files in this repo

| File | License | Role |
|------|---------|------|
| `src/SocketBridge.h` / `.cpp` | MIT | Unix-socket server + JSON dispatch. No SDK-specific code. |
| `src/ExtMcpBridge.h` / `.cpp` | Vectorworks SDK License | Menu-extension registration + the SDK command handler (`gSDK` calls). |
| `src/ExtMcpBridge.vwstrings` | — | Menu title/category/help strings. **Must stay UTF-16LE.** |

## Steps

1. **Clone the SDK examples** and open the ProcessResources project:
   ```
   SDKExamples/Examples2026/ProcessResources/ProcessResources2026.xcodeproj
   ```
   Confirm it builds unmodified first (see the build command below).

2. **Copy the four source files** from `plugin/src/` into the example's
   extensions folder:
   ```
   SDKExamples/Examples2026/ProcessResources/Source/Extensions/
     SocketBridge.h
     SocketBridge.cpp
     ExtMcpBridge.h
     ExtMcpBridge.cpp
   ```
   Copy `ExtMcpBridge.vwstrings` into the resource strings folder alongside the
   example's existing `.vwstrings`:
   ```
   .../ProcessResources.vwr/Strings/ExtMcpBridge.vwstrings
   ```
   > ⚠️ Preserve the file's **UTF-16LE** encoding when copying. If your editor
   > rewrites it as UTF-8, the menu strings will not load. Verify with
   > `file ExtMcpBridge.vwstrings` → should say "UTF-16, little-endian".

3. **Add the two new `.cpp` files to the Xcode target.** In the project
   navigator, add `SocketBridge.cpp` and `ExtMcpBridge.cpp` to the
   `ProcessResources` target's *Compile Sources* build phase. (You can remove
   the example's original `ExtMenuCollectResourceInfo.cpp/.h`, or leave them —
   but if you leave them, they and this extension will both try to register.)

4. **Register the extension** in `Source/ModuleMain.cpp`. Replace the example's
   include and `REGISTER_Extension<…>` line with:
   ```cpp
   #include "Extensions/ExtMcpBridge.h"
   ...
   REGISTER_Extension<VwMcpBridge::CExtMcpBridge>(
       GROUPID_ExtensionMenu, action, moduleInfo, iid, inOutInterface, cbp, reply );
   ```

5. **Build** (Release, code signing off — you'll ad-hoc sign after):
   ```bash
   cd SDKExamples/Examples2026/ProcessResources
   xcodebuild -project ProcessResources2026.xcodeproj \
     -scheme "ObjectExample Release" -configuration Release \
     CODE_SIGNING_ALLOWED=NO build
   ```
   Output:
   ```
   SDKExamples/Output/2026/_Output/Release/ProcessResources.vwlibrary
   ```
   (You may rename the `.vwlibrary` to something like `MCPBridge.vwlibrary`.)

6. **Ad-hoc sign** the bundle (macOS blocks unsigned/badly-signed code):
   ```bash
   codesign --force --deep --sign - ProcessResources.vwlibrary
   ```

7. **Install** it:
   ```
   ~/Library/Application Support/Vectorworks/2026/Plug-Ins/
   ```

8. **Enable the menu command.** In Vectorworks: Tools → Workspaces → Edit
   Current Workspace… → Menus. Find **MCP → MCP Bridge: Start/Stop** and drag it
   into a menu. Run it to start the socket bridge (you'll get an alert).

## Notes

- **nlohmann/json**: the source includes `"json/nlohmann/json.hpp"`, a
  single-header library bundled with the Vectorworks SDK. If your SDK layout
  differs, adjust the include path. It is not redistributed in this repo.
- **Fork it?** Regenerate the plugin UUID in `ExtMcpBridge.cpp`
  (`IMPLEMENT_VWMenuExtension(...)`) with `uuidgen` so it doesn't collide with
  other installs, and give it your own bundle identifier.
- **Socket path** is `/tmp/vw-mcp-bridge.sock`, defined in both
  `ExtMcpBridge.cpp` (`kSocketPath`) and the Rust server
  (`mcp-server/src/bridge.rs`). If you change it, change both.
