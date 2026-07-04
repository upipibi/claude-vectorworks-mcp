#include "StdAfx.h"          // Vectorworks SDK precompiled prefix header
#include "ExtMcpBridge.h"

#include "VectorWorks/Scripting/IVectorScriptEngine.h"
#include "VectorWorks/Scripting/IPythonScriptEngine.h"

using namespace VectorWorks;
using namespace VectorWorks::Scripting;
using namespace VwMcpBridge;

// Socket path — must match the Rust MCP server (mcp-server/src/bridge.rs).
static const char* kSocketPath = "/tmp/vw-mcp-bridge.sock";

// =====================================================================================================
// Command handler — dispatches JSON methods to Vectorworks SDK calls.
//
// Runs on the socket worker thread. NOTE: the Vectorworks SDK is not guaranteed
// thread-safe for document access, so writes/scripts can crash or corrupt the
// open document. See SECURITY.md. Work on a throwaway document.
// =====================================================================================================

static nlohmann::json HandleCommand(const nlohmann::json& cmd)
{
    std::string     method = cmd.value( "method", "" );
    nlohmann::json  result;
    result["id"] = cmd.value( "id", "" );

    if ( method == "ping" )
    {
        result["success"] = true;
        result["data"] = "pong";
    }
    else if ( method == "get_drawing_info" )
    {
        MCObjectHandle  hDoc = gSDK->GetDrawingHeader();
        nlohmann::json  info;
        info["has_document"] = (hDoc != nullptr);

        int layerCount = 0;
        for ( MCObjectHandle hLayer = gSDK->FirstMemberObj( hDoc ); hLayer; hLayer = gSDK->NextObject( hLayer ) )
            layerCount++;
        info["layer_count"] = layerCount;

        result["success"] = true;
        result["data"] = info;
    }
    else if ( method == "list_layers" )
    {
        MCObjectHandle  hDoc = gSDK->GetDrawingHeader();
        nlohmann::json  layers = nlohmann::json::array();

        for ( MCObjectHandle hLayer = gSDK->FirstMemberObj( hDoc ); hLayer; hLayer = gSDK->NextObject( hLayer ) )
        {
            short objType = gSDK->GetObjectTypeN( hLayer );
            if ( objType == kLayerNode )
            {
                TXString name;
                gSDK->GetObjectName( hLayer, name );
                nlohmann::json layer;
                layer["name"] = name.GetCharPtr();
                layers.push_back( layer );
            }
        }

        result["success"] = true;
        result["data"] = layers;
    }
    else if ( method == "create_line" )
    {
        auto params = cmd.value( "params", nlohmann::json::object() );
        double x1 = params.value( "x1", 0.0 );
        double y1 = params.value( "y1", 0.0 );
        double x2 = params.value( "x2", 0.0 );
        double y2 = params.value( "y2", 0.0 );

        MCObjectHandle h = gSDK->CreateLine( WorldPt(x1, y1), WorldPt(x2, y2) );
        result["success"] = (h != nullptr);
        result["data"] = h ? "Line created" : "Failed to create line";
    }
    else if ( method == "create_rect" )
    {
        auto params = cmd.value( "params", nlohmann::json::object() );
        double x = params.value( "x", 0.0 );
        double y = params.value( "y", 0.0 );
        double w = params.value( "width", 0.0 );
        double hgt = params.value( "height", 0.0 );

        WorldRect rect( WorldPt(x, y + hgt), WorldPt(x + w, y) );
        MCObjectHandle h = gSDK->CreateRectangle( rect );
        result["success"] = (h != nullptr);
        result["data"] = h ? "Rectangle created" : "Failed";
    }
    else if ( method == "create_circle" )
    {
        auto params = cmd.value( "params", nlohmann::json::object() );
        double cx = params.value( "cx", 0.0 );
        double cy = params.value( "cy", 0.0 );
        double r = params.value( "radius", 0.0 );

        // CreateOval takes a bounding WorldRect.
        WorldRect bounds( WorldPt(cx - r, cy + r), WorldPt(cx + r, cy - r) );
        MCObjectHandle h = gSDK->CreateOval( bounds );
        result["success"] = (h != nullptr);
        result["data"] = h ? "Circle created" : "Failed";
    }
    else if ( method == "run_script" )
    {
        auto params = cmd.value( "params", nlohmann::json::object() );
        std::string script = params.value( "script", "" );
        std::string lang = params.value( "language", "python" );

        if ( lang == "vectorscript" )
        {
            // IVectorScriptEngine::ExecuteScript runs the script; no output is captured here.
            IVectorScriptEnginePtr vsEngine( IID_VectorScriptEngine );
            if ( vsEngine )
            {
                vsEngine->ExecuteScript( TXString( script.c_str() ) );
                result["success"] = true;
                result["data"] = "VectorScript executed (no output captured — use python to get data back).";
            }
            else
            {
                result["success"] = false;
                result["data"] = "VectorScript engine unavailable";
            }
        }
        else
        {
            // Python — the full `vs` API is available. CDefaultPythonLogger
            // captures stdout/stderr so print()ed results come back to the caller.
            IPythonScriptEnginePtr pyEngine( IID_PythonScriptEngine );
            if ( pyEngine )
            {
                CDefaultPythonLogger logger;
                pyEngine->ExecuteScript( TXString( script.c_str() ), &logger );

                nlohmann::json data;
                data["stdout"] = logger.fOutput.GetCharPtr();
                data["stderr"] = logger.fErrors.GetCharPtr();
                result["success"] = (logger.fErrors.GetLength() == 0);
                result["data"] = data;
            }
            else
            {
                result["success"] = false;
                result["data"] = "Python engine unavailable";
            }
        }
    }
    else
    {
        result["success"] = false;
        result["data"] = "Unknown method: " + method;
    }

    return result;
}

// =====================================================================================================
// Menu extension registration
// =====================================================================================================

namespace VwMcpBridge
{
    static SMenuDef     gMenuDef = {
        /*Needs*/           EMenuEnableFlags::DocIsActive,
        /*NeedsNot*/        EMenuEnableFlags::None,
        /*Title*/           {"ExtMcpBridge", "title"},
        /*Category*/        {"ExtMcpBridge", "category"},
        /*HelpText*/        {"ExtMcpBridge", "help"},
        /*VersionCreated*/  31,
        /*VersionModified*/ 31,
        /*VersionRetired*/  0,
        /*OverrideHelpID*/  ""
    };
}

// UUID is unique to this plugin (regenerate with `uuidgen` if you fork it).
IMPLEMENT_VWMenuExtension(
    /*Extension class*/ CExtMcpBridge,
    /*Event sink*/      CExtMcpBridge_EventSink,
    /*Universal name*/  "MCP Bridge",
    /*Version*/         1,
    /*UUID*/            0x82f6df29, 0x2d56, 0x43dc, 0xb4, 0x0d, 0x62, 0x7f, 0x0c, 0x6f, 0x2f, 0x4a);


CExtMcpBridge::CExtMcpBridge(CallBackPtr cbp)
    : VWExtensionMenu( cbp, gMenuDef )
{
}

CExtMcpBridge::~CExtMcpBridge()
{
}


CExtMcpBridge_EventSink::CExtMcpBridge_EventSink(IVWUnknown* parent)
    : VWMenu_EventSink( parent )
{
}

CExtMcpBridge_EventSink::~CExtMcpBridge_EventSink()
{
}

// =====================================================================================================
// Menu command: toggle the MCP Bridge socket server on/off
// =====================================================================================================

void CExtMcpBridge_EventSink::DoInterface()
{
    SocketBridge& bridge = SocketBridge::Instance();

    if ( bridge.IsRunning() )
    {
        bridge.Stop();
        gSDK->AlertInform( "MCP Bridge stopped.", "", false );
    }
    else
    {
        bridge.SetHandler( HandleCommand );

        if ( bridge.Start( kSocketPath ) )
        {
            gSDK->AlertInform( "MCP Bridge started.", "Vectorworks can now be driven by the MCP server.", false );
        }
        else
        {
            gSDK->AlertInform( "Failed to start MCP Bridge.", "", false );
        }
    }
}
