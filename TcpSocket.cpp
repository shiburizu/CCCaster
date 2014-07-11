#include "Event.h"
#include "TcpSocket.h"
#include "Log.h"
#include "Util.h"
#include "Protocol.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <cassert>

using namespace std;

TcpSocket::TcpSocket ( Socket::Owner *owner, uint16_t port ) : Socket ( IpAddrPort ( "", port ), Protocol::TCP )
{
    this->owner = owner;
    this->state = State::Listening;
    Socket::init();
    EventManager::get().addSocket ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const IpAddrPort& address ) : Socket ( address, Protocol::TCP )
{
    this->owner = owner;
    this->state = State::Connecting;
    Socket::init();
    EventManager::get().addSocket ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, int fd, const IpAddrPort& address ) : Socket ( address, Protocol::TCP )
{
    this->owner = owner;
    this->state = State::Connected;
    this->fd = fd;
    EventManager::get().addSocket ( this );
}

TcpSocket::TcpSocket ( Socket::Owner *owner, const SocketShareData& data ) : Socket ( data.address, Protocol::TCP )
{
    assert ( data.protocol == Protocol::TCP );

    this->owner = owner;
    this->state = data.state;
    this->readBuffer = data.readBuffer;
    this->readPos = data.readPos;

    assert ( data.info->iSocketType == SOCK_STREAM );
    assert ( data.info->iProtocol == IPPROTO_TCP );

    this->fd = WSASocket ( data.info->iAddressFamily, SOCK_STREAM, IPPROTO_TCP, data.info.get(), 0, 0 );

    if ( this->fd == INVALID_SOCKET )
    {
        WindowsError err = WSAGetLastError();
        LOG_SOCKET ( this, "WSASocket failed %s", err );
        this->fd = 0;
        throw err;
    }

    EventManager::get().addSocket ( this );
}

TcpSocket::~TcpSocket()
{
    disconnect();
}

void TcpSocket::disconnect()
{
    EventManager::get().removeSocket ( this );
    Socket::disconnect();
}

SocketPtr TcpSocket::listen ( Socket::Owner *owner, uint16_t port )
{
    return SocketPtr ( new TcpSocket ( owner, port ) );
}

SocketPtr TcpSocket::connect ( Socket::Owner *owner, const IpAddrPort& address )
{
    return SocketPtr ( new TcpSocket ( owner, address ) );
}

SocketPtr TcpSocket::accept ( Socket::Owner *owner )
{
    if ( !isServer() )
        return 0;

    sockaddr_storage sas;
    int saLen = sizeof ( sas );

    int newFd = ::accept ( fd, ( sockaddr * ) &sas, &saLen );

    if ( newFd == INVALID_SOCKET )
    {
        LOG_SOCKET ( this, "accept failed: %s", WindowsError ( WSAGetLastError() ) );
        return 0;
    }

    return SocketPtr ( new TcpSocket ( owner, newFd, ( sockaddr * ) &sas ) );
}

bool TcpSocket::send ( SerializableMessage *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ) );
}

bool TcpSocket::send ( SerializableSequence *message, const IpAddrPort& address )
{
    return send ( MsgPtr ( message ) );
}

bool TcpSocket::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    string buffer = Serializable::encode ( msg );

    LOG ( "Encoded '%s' to [ %u bytes ]", msg, buffer.size() );

    if ( !buffer.empty() && buffer.size() < 256 )
        LOG ( "Base64 : %s", toBase64 ( buffer ) );

    return Socket::send ( &buffer[0], buffer.size() );
}

SocketPtr TcpSocket::shared ( Socket::Owner *owner, const SocketShareData& data )
{
    if ( data.protocol != Protocol::TCP )
        return 0;

    return SocketPtr ( new TcpSocket ( owner, data ) );
}

void TcpSocket::acceptEvent()
{
    if ( owner )
        owner->acceptEvent ( this );
    else
        accept ( 0 ).reset();
}

void TcpSocket::connectEvent()
{
    state = State::Connected;
    if ( owner )
        owner->connectEvent ( this );
}

void TcpSocket::disconnectEvent()
{
    Socket::Owner *owner = this->owner;
    disconnect();
    if ( owner )
        owner->disconnectEvent ( this );
}

void TcpSocket::readEvent ( const MsgPtr& msg, const IpAddrPort& address )
{
    if ( owner )
        owner->readEvent ( this, msg, address );
}