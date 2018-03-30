#pragma once
#include "Socket_Lite.h"
#include "defs.h"
#include "spinlock.h"
#include <memory>

namespace SL {
namespace NET {
    class Socket;
    class Context;
    class Listener final : public IListener {
      public:
        Context *Context_;
        std::shared_ptr<Socket> ListenSocket;
        SL::NET::sockaddr ListenSocketAddr;
        Win_IO_Accept_Context Win_IO_Accept_Context_;

        Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr);
        virtual ~Listener();
        virtual void close() override;
        virtual void accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler) override;

        static void handle_accept(bool success, Win_IO_Accept_Context *context);
    };

} // namespace NET
} // namespace SL
