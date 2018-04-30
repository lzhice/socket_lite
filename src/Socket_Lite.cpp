
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
typedef int SOCKLEN_T;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef socklen_t SOCKLEN_T;
#endif

#include <string.h>
#include <string>

namespace SL {
namespace NET {
#ifdef _WIN32
    void CloseSocket(PlatformSocket &handle)
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }
        handle = INVALID_SOCKET;
    }
#else
    void CloseSocket(PlatformSocket &handle)
    {
        if (handle != INVALID_SOCKET) {
            ::close(handle);
        }
        handle = INVALID_SOCKET;
    }
#endif

    const unsigned char *SocketAddr(const sockaddr &s) { return s.SocketImpl; }
    int SocketAddrLen(const sockaddr &s) { return s.SocketImplLen; }
    const std::string &Host(const sockaddr &s) { return s.Host; }
    unsigned short Port(const sockaddr &s) { return s.Port; }
    AddressFamily Family(const sockaddr &s) { return s.Family; }

    sockaddr::sockaddr(const sockaddr &addr) : SocketImplLen(addr.SocketImplLen), Host(addr.Host), Port(addr.Port), Family(addr.Family)
    {
        memcpy(SocketImpl, addr.SocketImpl, sizeof(SocketImpl));
    }
    sockaddr::sockaddr(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family)
    {
        assert(static_cast<size_t>(len) < sizeof(SocketImpl));
        memcpy(SocketImpl, buffer, len);
        SocketImplLen = len;
        Host = host;
        Port = port;
        Family = family;
    }
    const unsigned char *sockaddr::get_SocketAddr() const { return SocketImpl; }
    int sockaddr::get_SocketAddrLen() const { return SocketImplLen; }
    std::string sockaddr::get_Host() const { return Host; }
    unsigned short sockaddr::get_Port() const { return htons(Port); }
    AddressFamily sockaddr::get_Family() const { return Family; }

    std::tuple<StatusCode, std::vector<SL::NET::sockaddr>> getaddrinfo(const char *nodename, PortNumber pServiceName, AddressFamily family)
    {
        ::addrinfo hints = {0};
        ::addrinfo *result(nullptr);
        std::vector<SL::NET::sockaddr> ret;
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE; /* All interfaces */
        auto portstr = std::to_string(pServiceName.value);
        auto s = ::getaddrinfo(nodename, portstr.c_str(), &hints, &result);
        if (s != 0) {
            return std::make_tuple(TranslateError(), ret);
        }
        for (auto ptr = result; ptr != NULL; ptr = ptr->ai_next) {

            char str[INET_ADDRSTRLEN];
            if (ptr->ai_family == AF_INET6 && family == AddressFamily::IPV6) {
                auto so = (::sockaddr_in6 *)ptr->ai_addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                SL::NET::sockaddr tmp((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6);
                ret.push_back(tmp);
            }
            else if (ptr->ai_family == AF_INET && family == AddressFamily::IPV4) {
                auto so = (::sockaddr_in *)ptr->ai_addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                SL::NET::sockaddr tmp((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4);
                ret.push_back(tmp);
            }
        }
        freeaddrinfo(result);
        return std::make_tuple(StatusCode::SC_SUCCESS, ret);
    }
    bool Socket::isopen() const { return handle != INVALID_SOCKET; }
    void Socket::close()
    {
        auto t = handle;
        handle = INVALID_SOCKET;
        if (t != INVALID_SOCKET) {
            CloseSocket(t);
        }
#if !_WIN32
        SocketGetter sg1(*this);
        auto whandler(WriteContext.getCompletionHandler());
        if (whandler) {
            WriteContext.reset();
            sg1.getPendingIO() -= 1;
            whandler(StatusCode::SC_CLOSED, 0);
        }
        auto whandler1(ReadContext.getCompletionHandler());
        if (whandler1) {
            ReadContext.reset();
            sg1.getPendingIO() -= 1;
            whandler1(StatusCode::SC_CLOSED, 0);
        }
#endif
    }
    Socket::Socket(Context &c) : context(c)
    {
        handle = INVALID_SOCKET;
        WriteContext.Context_ = ReadContext.Context_ = &c;
    }
    Socket::Socket(Socket &&sock) : handle(sock.handle), context(sock.context) { sock.handle = INVALID_SOCKET; }
    Socket::~Socket() { close(); }
    void recv(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        if (!socket.isopen())
            return;
        SocketGetter sg(socket);
        auto context = sg.getReadContext();
        context->buffer = buffer;
        context->transfered_bytes = 0;
        context->bufferlen = buffer_size;
        context->Context_ = sg.getContext();
        context->Socket_ = sg.getSocket();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = INTERNAL::IO_OPERATION::IoRead;
#if !_WIN32
        sg.getPendingIO() += 1;
        continue_io(true, context, sg.getPendingIO(), stacklevel, sg.getIOCPHandle());
#else
        continue_io(true, context, sg.getPendingIO());
#endif
    }
    void send(Socket &socket, size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        if (!socket.isopen())
            return;
        thread_local int stacklevel = 0;
        stacklevel += 1;
        SocketGetter sg(socket);
        auto context = sg.getWriteContext();
        context->buffer = buffer;
        context->transfered_bytes = 0;
        context->bufferlen = buffer_size;
        context->Context_ = sg.getContext();
        context->Socket_ = sg.getSocket();
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = INTERNAL::IO_OPERATION::IoWrite;
#if !_WIN32
        sg.getPendingIO() += 1;
        continue_io(true, context, sg.getPendingIO(), stacklevel, sg.getIOCPHandle());
#else
        continue_io(true, context, sg.getPendingIO());
#endif
    }
    StatusCode SocketGetter::bind(sockaddr addr)
    {
        if (socket.handle == INVALID_SOCKET) {
            socket.handle = INTERNAL::Socket(addr.get_Family());
        }
        if (::bind(socket.handle, (::sockaddr *)addr.get_SocketAddr(), addr.get_SocketAddrLen()) == SOCKET_ERROR) {
            return TranslateError();
        }
        return StatusCode::SC_SUCCESS;
    }
    StatusCode SocketGetter::listen(int backlog)
    {
        if (::listen(socket.handle, backlog) == SOCKET_ERROR) {
            return TranslateError();
        }
        return StatusCode::SC_SUCCESS;
    }
    std::optional<SL::NET::sockaddr> Socket::getpeername()
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);

        char str[INET_ADDRSTRLEN];
        if (::getpeername(handle, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4));
            }
            else { // AF_INET6
                auto so = (sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6));
            }
        }
        return std::nullopt;
    }
    std::optional<SL::NET::sockaddr> Socket::getsockname()
    {
        sockaddr_storage addr = {0};
        socklen_t len = sizeof(addr);
        char str[INET_ADDRSTRLEN];
        if (::getsockname(handle, (::sockaddr *)&addr, &len) == 0) {
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                auto so = (::sockaddr_in *)&addr;
                inet_ntop(AF_INET, &so->sin_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in), str, so->sin_port, AddressFamily::IPV4));
            }
            else { // AF_INET6
                auto so = (::sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &so->sin6_addr, str, INET_ADDRSTRLEN);
                return std::optional<SL::NET::sockaddr>(
                    SL::NET::sockaddr((unsigned char *)so, sizeof(sockaddr_in6), str, so->sin6_port, AddressFamily::IPV6));
            }
        }
        return std::nullopt;
    }

    PlatformSocket INTERNAL::Socket(AddressFamily family)
    {
        int typ = SOCK_STREAM;
#if !_WIN32
        typ |= SOCK_NONBLOCK;
#endif
        if (family == AddressFamily::IPV4) {
            return socket(AF_INET, typ, 0);
        }
        else {
            return socket(AF_INET6, typ, 0);
        }
    }
    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_DEBUG(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_DEBUG, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_ACCEPTCONN(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_ACCEPTCONN, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_BROADCAST(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_REUSEADDR(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_KEEPALIVE(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<LingerOption>> INTERNAL::getsockopt_O_LINGER(PlatformSocket handle)
    {
        linger value;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_LINGER, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<LingerOption>({value.l_onoff == 0 ? LingerOptions::LINGER_OFF : LingerOptions::LINGER_ON,
                                                                std::chrono::seconds(value.l_linger)}));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_OOBINLINE(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_OOBINLINE, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<int>> INTERNAL::getsockopt_O_SNDBUF(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<int>(value));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<int>> INTERNAL::getsockopt_O_RCVBUF(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<int>(value));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<std::chrono::seconds>> INTERNAL::getsockopt_O_SNDTIMEO(PlatformSocket handle)
    {
#ifdef _WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<std::chrono::seconds>(value * 1000)); // convert from ms to seconds
        }
#else
        timeval value = {0};
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<std::chrono::seconds>(value.tv_sec)); // convert from ms to seconds
        }
#endif
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<std::chrono::seconds>> INTERNAL::getsockopt_O_RCVTIMEO(PlatformSocket handle)
    {
#ifdef _WIN32
        DWORD value = 0;
        int valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<std::chrono::seconds>(value * 1000)); // convert from ms to seconds
        }
#else
        timeval value = {0};
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<std::chrono::seconds>(value.tv_sec)); // convert from ms to seconds
        }
#endif
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<int>> INTERNAL::getsockopt_O_ERROR(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS, std::optional<int>(value));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<SockOptStatus>> INTERNAL::getsockopt_O_NODELAY(PlatformSocket handle)
    {
        int value = 0;
        SOCKLEN_T valuelen = sizeof(value);
        if (::getsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char *)&value, &valuelen) == 0) {
            return std::make_tuple(StatusCode::SC_SUCCESS,
                                   std::optional<SockOptStatus>(value != 0 ? SockOptStatus::ENABLED : SockOptStatus::DISABLED));
        }
        return std::make_tuple(TranslateError(), std::nullopt);
    }

    std::tuple<StatusCode, std::optional<Blocking_Options>> INTERNAL::getsockopt_O_BLOCKING(PlatformSocket handle)
    {
#ifdef _WIN32
        return std::make_tuple(StatusCode::SC_NOTSUPPORTED, std::nullopt);
#else
        long arg = 0;
        if ((arg = fcntl(handle, F_GETFL, NULL)) < 0) {
            return std::make_tuple(TranslateError(), std::nullopt);
        }
        return std::make_tuple(StatusCode::SC_SUCCESS, (arg & O_NONBLOCK) != 0 ? Blocking_Options::NON_BLOCKING : Blocking_Options::BLOCKING);
#endif
    }
    bool INTERNAL::setsockopt_O_DEBUG(PlatformSocket handle, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_DEBUG, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_BROADCAST(PlatformSocket handle, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_REUSEADDR(PlatformSocket handle, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_KEEPALIVE(PlatformSocket handle, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_LINGER(PlatformSocket handle, LingerOption o)
    {
        linger value;
        value.l_onoff = o.l_onoff == LingerOptions::LINGER_OFF ? 0 : 1;
        value.l_linger = static_cast<unsigned short>(o.l_linger.count());
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_LINGER, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_OOBINLINE(PlatformSocket handle, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_OOBINLINE, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_SNDBUF(PlatformSocket handle, int b)
    {
        int value = b;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_SNDBUF, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_RCVBUF(PlatformSocket handle, int b)
    {
        int value = b;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVBUF, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_SNDTIMEO(PlatformSocket handle, std::chrono::seconds sec)
    {
#ifdef _WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (char *)&value, valuelen) == 0;
#else
        timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return ::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool INTERNAL::setsockopt_O_RCVTIMEO(PlatformSocket handle, std::chrono::seconds sec)
    {
#ifdef WIN32
        DWORD value = static_cast<DWORD>(sec.count() * 1000); // convert to miliseconds for windows
        int valuelen = sizeof(value);
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&value, valuelen) == 0;
#else
        timeval tv = {0};
        tv.tv_sec = static_cast<long>(sec.count());
        return ::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(tv)) == 0;
#endif
    }

    bool INTERNAL::setsockopt_O_NODELAY(PlatformSocket handle, SockOptStatus b)
    {
        int value = b == SockOptStatus::ENABLED ? 1 : 0;
        int valuelen = sizeof(value);
        return ::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (char *)&value, valuelen) == 0;
    }

    bool INTERNAL::setsockopt_O_BLOCKING(PlatformSocket handle, Blocking_Options b)
    {

#ifdef WIN32
        u_long iMode = b == Blocking_Options::BLOCKING ? 0 : 1;
        if (auto nRet = ioctlsocket(handle, FIONBIO, &iMode); nRet != NO_ERROR) {
            return false;
        }
        return true;
#else
        int flags = fcntl(handle, F_GETFL, 0);
        if (flags == 0)
            return false;
        flags = b == Blocking_Options::BLOCKING ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(handle, F_SETFL, flags) == 0) ? true : false;
#endif
    }

    StatusCode TranslateError(int *errcode)
    {
#if _WIN32
        auto originalerr = WSAGetLastError();
        auto errorcode = errcode != nullptr ? *errcode : originalerr;
        switch (errorcode) {
        case WSAECONNRESET:
            return StatusCode::SC_ECONNRESET;
        case WSAETIMEDOUT:
        case WSAECONNABORTED:
            return StatusCode::SC_ETIMEDOUT;
        case WSAEWOULDBLOCK:
            return StatusCode::SC_EWOULDBLOCK;
#else
        auto originalerror = errno;
        auto errorcode = errcode != nullptr ? *errcode : originalerror;
        switch (errorcode) {

#endif

        default:
            return StatusCode::SC_CLOSED;
        };
    } // namespace NET

} // namespace NET
} // namespace SL
