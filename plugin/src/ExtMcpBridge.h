#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Derived from the Vectorworks SDK "ProcessResources" menu-extension sample.
// Subject to the Vectorworks SDK License Agreement — NOT the MIT license that
// covers the rest of this project. Requires your own licensed copy of the
// Vectorworks SDK to build. See NOTICE and plugin/BUILD.md.
// ─────────────────────────────────────────────────────────────────────────────

#include "SocketBridge.h"

namespace VwMcpBridge
{
    using namespace VWFC::PluginSupport;

    // Menu-extension event sink: toggles the socket bridge on/off.
    class CExtMcpBridge_EventSink : public VWMenu_EventSink
    {
    public:
        CExtMcpBridge_EventSink(IVWUnknown* parent);
        virtual                 ~CExtMcpBridge_EventSink();

        virtual void            DoInterface();
    };

    // The menu-extension object itself.
    class CExtMcpBridge : public VWExtensionMenu
    {
        DEFINE_VWMenuExtension;
    public:
        CExtMcpBridge(CallBackPtr cbp);
        virtual                 ~CExtMcpBridge();
    };
}
