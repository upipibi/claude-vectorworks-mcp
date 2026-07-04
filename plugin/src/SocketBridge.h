#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SocketBridge — a minimal Unix-domain-socket server that receives
// newline-delimited JSON commands and dispatches them to a handler.
//
// Derived from mako-357/vectorworks-mcp (MIT). See NOTICE.
// This file contains no Vectorworks SDK code and is covered by the project's
// MIT license.
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <functional>
#include <thread>
#include <atomic>

#include "json/nlohmann/json.hpp"

namespace VwMcpBridge
{
    // A handler receives a parsed JSON command and returns a JSON response.
    // NOTE: it is invoked on the socket's background thread.
    using CommandHandler = std::function<nlohmann::json(const nlohmann::json&)>;

    class SocketBridge
    {
    public:
        static SocketBridge&    Instance();

        bool                    Start(const std::string& socketPath);
        void                    Stop();
        void                    SetHandler(CommandHandler handler);
        bool                    IsRunning() const { return m_running; }

    private:
        SocketBridge() = default;
        ~SocketBridge();

        void                    ServerLoop();
        void                    HandleClient(int clientFd);

        std::string             m_socketPath;
        int                     m_serverFd = -1;
        std::atomic<bool>       m_running{false};
        std::thread             m_serverThread;
        CommandHandler          m_handler;
    };
}
