/***************************************************************************
 *   Copyright (C) 2022 by MultiCapture team <public.irkutsk@gmail.com>    *
 *                                                                         *
 *   Part of the MultiCapture engine:                                      *
 *   https://github.com/AndreyBarmaley/multi-capture                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <array>
#include <stdexcept>
#include "network_stream.h"

namespace Network
{
    /* BaseStream */
    BaseStream & BaseStream::sendIntLE16(uint16_t v)
    {
        v = SDL_SwapLE16(v);
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendIntLE32(uint32_t v)
    {
        v = SDL_SwapLE32(v);
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendIntLE64(uint64_t v)
    {
        v = SDL_SwapLE64(v);
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendIntBE16(uint16_t v)
    {
        v = SDL_SwapBE16(v);
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendIntBE32(uint32_t v)
    {
        v = SDL_SwapBE32(v);
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendIntBE64(uint64_t v)
    {
        v = SDL_SwapBE64(v);
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendInt8(uint8_t v)
    {
        sendRaw(& v, sizeof(v));
        return *this;
    }

    BaseStream & BaseStream::sendInt16(uint16_t v)
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        return sendIntLE16(v);
#else
        return sendIntBE16(v);
#endif
    }

    BaseStream & BaseStream::sendInt32(uint32_t v)
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        return sendIntLE32(v);
#else
        return sendIntBE32(v);
#endif
    }

    BaseStream & BaseStream::sendInt64(uint64_t v)
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        return sendIntLE64(v);
#else
        return sendIntBE64(v);
#endif
    }

    uint16_t BaseStream::recvIntLE16(void) const
    {
        uint16_t v = 0;
        recvRaw(& v, sizeof(v));
        return SDL_SwapLE16(v);
    }

    uint32_t BaseStream::recvIntLE32(void) const
    {
        uint32_t v = 0;
        recvRaw(& v, sizeof(v));
        return SDL_SwapLE32(v);
    }

    uint64_t BaseStream::recvIntLE64(void) const
    {
        uint64_t v = 0;
        recvRaw(& v, sizeof(v));
        return SDL_SwapLE64(v);
    }

    uint16_t BaseStream::recvIntBE16(void) const
    {
        uint16_t v = 0;
        recvRaw(& v, sizeof(v));
        return SDL_SwapBE16(v);
    }

    uint32_t BaseStream::recvIntBE32(void) const
    {
        uint32_t v = 0;
        recvRaw(& v, sizeof(v));
        return SDL_SwapBE32(v);
    }

    uint64_t BaseStream::recvIntBE64(void) const
    {
        uint64_t v = 0;
        recvRaw(& v, sizeof(v));
        return SDL_SwapBE64(v);
    }

    uint8_t BaseStream::recvInt8(void) const
    {
        uint8_t v = 0;
        recvRaw(& v, sizeof(v));
        return v;
    }

    uint16_t BaseStream::recvInt16(void) const
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        return recvIntLE16();
#else
        return recvIntBE16();
#endif
    }

    uint32_t BaseStream::recvInt32(void) const
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        return recvIntLE32();
#else
        return recvIntBE32();
#endif
    }

    uint64_t BaseStream::recvInt64(void) const
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        return recvIntLE64();
#else
        return recvIntBE64();
#endif
    }

    void BaseStream::recvSkip(size_t length) const
    {
        while(length--)
            recvInt8();
    }

    BaseStream & BaseStream::sendZero(size_t length)
    {
        while(length--)
            sendInt8(0);

        return *this;
    }

    BaseStream & BaseStream::sendData(const std::vector<uint8_t> & v)
    {
        sendRaw(v.data(), v.size());
        return *this;
    }

    BaseStream & BaseStream::sendString(const std::string & str)
    {
        sendRaw(str.data(), str.size());
        return *this;
    }

    std::vector<uint8_t> BaseStream::recvData(void) const
    {
        std::vector<uint8_t> res;
        res.reserve(64);

        while(hasInput())
            res.push_back(recvInt8());

        return res;
    }

    std::vector<uint8_t> BaseStream::recvData(size_t length) const
    {
        std::vector<uint8_t> res(length, 0);
        if(length) recvRaw(res.data(), res.size());
        return res;
    }

    std::string BaseStream::recvString(size_t length) const
    {
        auto buf = recvData(length);
        return std::string(buf.begin(), buf.end());
    }

    /* TCPStream */
    TCPStream::TCPStream(TCPsocket val) : sock(val)
    {
        sdset = SDLNet_AllocSocketSet(1);

        if(sock)
            SDLNet_TCP_AddSocket(sdset, sock);

        buf.reserve(2048);
    }

    TCPStream::~TCPStream()
    {
        close();
    }

    bool TCPStream::open(TCPsocket val)
    {
        if(sock)
            SDLNet_TCP_Close(sock);

        sock = val;

        if(sock)
        {
            SDLNet_TCP_AddSocket(sdset, sock);
            return true;
        }

        return false;
    }

    void TCPStream::close(void)
    {
        if(sdset)
        {
            if(sock) SDLNet_TCP_DelSocket(sdset, sock);

            SDLNet_FreeSocketSet(sdset);
            sdset = nullptr;
        }

        if(sock)
        {
            SDLNet_TCP_Close(sock);
            sock = nullptr;
        }
    }

    bool TCPStream::hasInput(void) const
    {
        return sock && 0 < SDLNet_CheckSockets(sdset, 1) && 0 < SDLNet_SocketReady(sock);
    }

    void TCPStream::recvRaw(void* ptr, size_t len) const
    {
        size_t total = 0;
        while(total < len)
        {
            auto buf = reinterpret_cast<uint8_t*>(ptr);
            int rcv = SDLNet_TCP_Recv(sock, buf + total, len - total);
        
            if(0 >= rcv)
                throw std::runtime_error(SWE::StringFormat("TCPStream::recvRaw: read bytes: %1, expected: %2, error: %3").arg(total).arg(len).arg(SDL_GetError()));

            total += rcv;

            if(total < len)
                SDL_Delay(1);
        }
    }

    void TCPStream::sendRaw(const void* ptr, size_t len)
    {
        if(ptr && len)
        {
            auto it = static_cast<const uint8_t*>(ptr);
            buf.insert(buf.end(), it, it + len);
        }
    }

    void TCPStream::sendFlush(void)
    {
        if(buf.size())
        {
            auto real = SDLNet_TCP_Send(sock, buf.data(), buf.size());

            if(real != buf.size())
                throw std::runtime_error(SWE::StringFormat("TCPStream::sendFlush: write bytes: %1, expected: %2").arg(real).arg(buf.size()));

            buf.clear();
        }
    }

    /* TCPServer */
    TCPsocket TCPServer::accept(void)
    {
        return sock ? SDLNet_TCP_Accept(sock) : nullptr;
    }

    bool TCPServer::listen(int port)
    {
        IPaddress ip;
        
        if(0 > SDLNet_ResolveHost(&ip, nullptr, port))
        {
            ERROR(SDLNet_GetError());
            return false;
        }
        
        close();
        sock = SDLNet_TCP_Open(&ip);
        
        if(! sock)
        {
            ERROR(SDLNet_GetError());
            return false;
        }
                
        return true;
    }

    /* TCPClient */
    TCPClient::TCPClient(const std::string & name, int port)
    {
        connect(name, port);
    }

    bool TCPClient::connect(const std::string & name, int port)
    {
        IPaddress ip;

        if(0 > SDLNet_ResolveHost(&ip, name.size() ? name.c_str() : nullptr, port))
        {
            ERROR(SDLNet_GetError());
            return false;
        }

        return TCPStream::open(SDLNet_TCP_Open(&ip));
    }

    /* TCPClientDebug */
    TCPClientDebug::TCPClientDebug(const std::string & flog) : ofs(flog, std::ofstream::out|std::ofstream::trunc)
    {
    }
    
    void TCPClientDebug::sendFlush(void)
    {
        if(ofs.is_open())
        {
            std::string str = SWE::Tools::buffer2HexString<uint8_t>(buf.data(), buf.size(), 2, ",", false);
            ofs << "> " << str << std::endl;
        }

        TCPClient::sendFlush();
    }

    void TCPClientDebug::recvRaw(void* ptr, size_t len) const
    {
        TCPClient::recvRaw(ptr, len);

        if(ofs.is_open())
        {
            auto buf = static_cast<const uint8_t*>(ptr);
            std::string str = SWE::Tools::buffer2HexString<uint8_t>(buf, len, 2, ",", false);
            ofs << "< " << str << std::endl;
        }
    }

    /* DeflateStream */
    ZlibContext::ZlibContext()
    {
        zalloc = 0;
        zfree = 0;
        opaque = 0;
        total_in = 0;
        total_out = 0;
        avail_in = 0;
        next_in = 0;
        avail_out = 0;
        next_out = 0;
        data_type = Z_BINARY;
        buf.reserve(4 * 1024);
    }

    std::vector<uint8_t> ZlibContext::deflateFlush(bool finish)
    {
        next_in = buf.data();
        avail_in = buf.size();
        std::vector<uint8_t> zip(deflateBound(this, buf.size()));
        next_out = zip.data();
        avail_out = zip.size();
        int prev = total_out;
        int ret = deflate(this, finish ? Z_FINISH : Z_SYNC_FLUSH);

        if(ret < Z_OK)
            throw std::runtime_error(std::string("ZlibContext::deflateFlush: failed code: ").append(std::to_string(ret)));

        size_t zipsz = total_out - prev;
        zip.resize(zipsz);
        buf.clear();
        next_in = nullptr;
        avail_in = 0;
        next_out = nullptr;
        avail_out = 0;

        return zip;
    }

    void ZlibContext::inflateFlush(const std::vector<uint8_t> & zip)
    {
        std::array<uint8_t, 1024> tmp;
        next_in = (Bytef*) zip.data();
        avail_in = zip.size();

        do
        {
            next_out = tmp.data();
            avail_out = tmp.size();
            int ret = inflate(this, Z_NO_FLUSH);

            if(ret < Z_OK)
                throw std::runtime_error(std::string("ZlibContext::inflateFlush: failed code: ").append(std::to_string(ret)));

            buf.insert(buf.end(), tmp.begin(), std::next(tmp.begin(), (tmp.size() - avail_out)));
        }
        while(avail_in > 0);

        next_in = nullptr;
        avail_in = 0;
        next_out = nullptr;
        avail_out = 0;
    }

    /* DeflateStream */
    DeflateStream::DeflateStream(int level)
    {
        if(level < Z_BEST_SPEED || Z_BEST_COMPRESSION < level)
            level = Z_BEST_COMPRESSION;

        auto ptr = new ZlibContext();
        zlib.reset(ptr);
        int ret = deflateInit2(ptr, level, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);

        if(ret < Z_OK)
            throw std::runtime_error(std::string("DeflateStream: init failed, code: ").append(std::to_string(ret)));
    }

    DeflateStream::~DeflateStream()
    {
        deflateEnd(zlib.get());
    }

    void DeflateStream::prepareSize(size_t len)
    {
        if(len < zlib->buf.capacity()) zlib->buf.reserve(len);
    }

    std::vector<uint8_t> DeflateStream::deflateFlush(void) const
    {
        return zlib->deflateFlush();
    }

    void DeflateStream::sendRaw(const void* ptr, size_t len)
    {
        auto buf2 = reinterpret_cast<const uint8_t*>(ptr);
        zlib->buf.insert(zlib->buf.end(), buf2, buf2 + len);
    }

    void DeflateStream::recvRaw(void* ptr, size_t len) const
    {
        throw std::runtime_error("DeflateStream::recvRaw: disabled");
    }

    bool DeflateStream::hasInput(void) const
    {
        throw std::runtime_error("DeflateStream::hasInput: disabled");
    }

    /* InflateStream */
    InflateStream::InflateStream(void)
    {
        auto ptr = new ZlibContext();
        zlib.reset(ptr);
        it = zlib->buf.begin();

        int ret = inflateInit2(ptr, MAX_WBITS);
        if(ret < Z_OK)
            throw std::runtime_error(std::string("InflateStream: init failed, code: ").append(std::to_string(ret)));
    }

    InflateStream::~InflateStream()
    {
        inflateEnd(zlib.get());
    }

    void InflateStream::appendData(const std::vector<uint8_t> & zip)
    {
        if(it != zlib->buf.end())
            zlib->buf.erase(it, zlib->buf.end());
        else
            zlib->buf.clear();

        zlib->inflateFlush(zip);
        // fixed pos
        it = zlib->buf.begin();
    }

    void InflateStream::recvRaw(void* ptr, size_t len) const
    {
        size_t total = std::distance(it, zlib->buf.end());

        if(total < len)
            throw std::runtime_error(SWE::StringFormat("InflateStream::recvRaw: read bytes: %1, expected: %2, buf: %3").arg(total).arg(len).arg(zlib->buf.size()));

        auto buf2 = reinterpret_cast<uint8_t*>(ptr);
        while(len--) *buf2++ = *it++;
    }

    bool InflateStream::hasInput(void) const
    {
        return it != zlib->buf.end();
    }

    void InflateStream::sendRaw(const void* ptr, size_t len)
    {
        throw std::runtime_error("InflateStream::sendRaw: disabled");
    }
}
