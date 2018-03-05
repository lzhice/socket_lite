#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL
{
namespace NET
{
class Socket;
class Context;
class Listener final : public IListener
{
public:
#ifdef WIN32
    LPFN_ACCEPTEX AcceptEx_ = nullptr;
    char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
#endif //  WIN32

    Context *Context_=nullptr;
    std::shared_ptr<Socket> ListenSocket;
    SL::NET::sockaddr ListenSocketAddr;
    Win_IO_Accept_Context Win_IO_Accept_Context_;

    Listener(Context *context, std::shared_ptr<ISocket> &&socket, const sockaddr &addr);
    virtual ~Listener();
    virtual void close() override;
    virtual void async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler) override;
};

} // namespace NET
} // namespace SL
