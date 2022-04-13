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

#ifndef _STORAGE_NETWORK_
#define _STORAGE_NETWORK_

#include <vector>
#include <string>
#include <memory>
#include <fstream>

#include <zlib.h>

#include "SDL_net.h"
#include "libswe.h"

namespace Network
{
    /// @brief: network stream interface
    class BaseStream
    {
    public:
        BaseStream() {}
        virtual ~BaseStream() {}

        BaseStream &     sendIntBE16(uint16_t v);
        BaseStream &     sendIntBE32(uint32_t v);
        BaseStream &     sendIntBE64(uint64_t v);

        BaseStream &     sendIntLE16(uint16_t v);
        BaseStream &     sendIntLE32(uint32_t v);
        BaseStream &     sendIntLE64(uint64_t v);

        BaseStream &     sendInt8(uint8_t);
        BaseStream &     sendInt16(uint16_t);
        BaseStream &     sendInt32(uint32_t);
        BaseStream &     sendInt64(uint64_t);

        BaseStream &     sendZero(size_t);
        BaseStream &     sendData(const std::vector<uint8_t> &);

        virtual void     sendFlush(void) {}
        virtual void     sendRaw(const void*, size_t) = 0;
        virtual bool     hasInput(void) const = 0;

        uint16_t         recvIntBE16(void) const;
        uint32_t         recvIntBE32(void) const;
        uint64_t         recvIntBE64(void) const;

        uint16_t         recvIntLE16(void) const;
        uint32_t         recvIntLE32(void) const;
        uint64_t         recvIntLE64(void) const;

        uint8_t          recvInt8(void) const;
        uint16_t         recvInt16(void) const;
        uint32_t         recvInt32(void) const;
        uint64_t         recvInt64(void) const;

        void                 recvSkip(size_t) const;
        std::vector<uint8_t> recvData(void) const;
        std::vector<uint8_t> recvData(size_t) const;
        virtual void         recvRaw(void*, size_t) const = 0;

        BaseStream &         sendString(const std::string &);
        std::string          recvString(size_t) const;
    };

    class TCPStream : public BaseStream
    {
    protected:
        TCPsocket            sock;
        SDLNet_SocketSet     sdset;
        std::vector<uint8_t> buf;

    public:
        TCPStream(TCPsocket val = nullptr);
        ~TCPStream();

        bool        open(TCPsocket val);
        void        close(void);

        bool        hasInput(void) const override;
        void        sendRaw(const void*, size_t) override;
        void        recvRaw(void*, size_t) const override;
        void        sendFlush(void) override;
    };

    class TCPServer : public TCPStream
    {
    public:
        TCPServer() {}

        TCPsocket   accept(void);
        bool        listen(int port);
    };

    class TCPClient : public TCPStream
    {
    public:
        TCPClient() {}
        TCPClient(const std::string & name, int port);

        bool        connect(const std::string & name, int port);
    };

    class TCPClientDebug : public TCPClient
    {
    protected:
        mutable std::ofstream ofs;

    public:
        TCPClientDebug(const std::string &);

        void        sendFlush(void) override;
        void        recvRaw(void*, size_t) const override;
    };

    struct ZlibContext : z_stream
    {
        std::vector<uint8_t> buf;

        ZlibContext();
    
        std::vector<uint8_t> deflateFlush(bool finish = false);
        void inflateFlush(const std::vector<uint8_t> &);
    };

    /// @brief: zlib compress output stream only (VNC version)
    class DeflateStream : public BaseStream
    {
    protected:
        std::unique_ptr<ZlibContext> zlib;

    public:
        DeflateStream(int level = Z_BEST_COMPRESSION);
        ~DeflateStream();

        std::vector<uint8_t> deflateFlush(void) const;
        void        prepareSize(size_t);

        void        sendRaw(const void*, size_t) override;

    private:
        bool        hasInput(void) const override;
        void        recvRaw(void*, size_t) const override;
    };

    /// @brief: zlib compress outut stream only (VNC version)
    class InflateStream : public BaseStream
    {
    protected:
        std::unique_ptr<ZlibContext> zlib;
        mutable std::vector<uint8_t>::iterator it;

    public:
        InflateStream();
        ~InflateStream();

        void        appendData(const std::vector<uint8_t> &);

        bool        hasInput(void) const override;
        void        recvRaw(void*, size_t) const override;

    private:
        void        sendRaw(const void*, size_t) override;
    };
}

#endif
