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

#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <memory>

using namespace std::chrono_literals;

#include "../../../settings.h"
#include "storage_vnc_connector.h"

#ifdef __cplusplus
extern "C" {
#endif

struct storage_vnc_t
{
    int        debug;
    std::string label;
    Network::TCPServer sn;
    Surface	surface;
    int         port;
    std::thread	thread;
    std::mutex change;
    std::atomic<bool> shutdown;
    const SWE::JsonObject* config;
    std::unique_ptr<RFB::ServerConnector> vnc;

    storage_vnc_t() : debug(0), label("vnc"), port(0), shutdown(false), config(nullptr)
    {
    }

    ~storage_vnc_t()
    {
        clear();
    }

    static void start_thread(storage_vnc_t* st)
    {
        // wait accept
        TCPsocket sock = nullptr;
        DEBUG("start thread");

        while(! st->shutdown)
        {
            // wait accept
            while(! st->shutdown)
            {
                sock = st->sn.accept();
                if(sock) break;
                std::this_thread::sleep_for(50ms);
            }

            if(! sock)
            {
                ERROR("socket is null");
                std::this_thread::sleep_for(50ms);
                continue;
            }

            VERBOSE("client: connected");
            st->vnc.reset(new RFB::ServerConnector(sock, st->config));

            // wait surface
            while(! st->shutdown)
            {
                std::this_thread::sleep_for(150ms);
                const std::lock_guard<std::mutex> lock(st->change);
                if(st->surface.isValid()) break;
            }

            if(! st->shutdown)
            {
                IPaddress* ipa = sock ? SDLNet_TCP_GetPeerAddress(sock) : nullptr;
                std::string peer = StringFormat("%4.%3.%2.%1").
                               arg(ipa ? 0xFF & (ipa->host >> 24) : 0).arg(ipa ? 0xFF & (ipa->host >> 16) : 0).
                               arg(ipa ? 0xFF & (ipa->host >> 8) : 0).arg(ipa ? 0xFF & ipa->host : 0);

                st->vnc->setFrameBuffer(st->surface);

                try
                {
                    st->vnc->communication(peer);
                }
                catch(const std::exception & err)
                {
                    ERROR("exception: " << err.what());
                }
                catch(...)
                {
                    ERROR("exception: " << "unknown");
                }

                st->vnc.reset();
            }

            VERBOSE("client: disconnected");
        }
    }

    bool init(void)
    {
        if(! sn.listen(port))
        {
            ERROR("listen port: " << port);
            return false;
        }

        thread = std::thread([this]()
        {
            start_thread(this);
        });
        return true;
    }

    void clear(void)
    {
        shutdown = true;
        if(thread.joinable()) thread.join();

        debug = 0;
        surface.reset();
        sn.close();
        config = nullptr;
        shutdown = false;
    }
};

const char* storage_vnc_get_name(void)
{
    return "storage_vnc";
}

int storage_vnc_get_version(void)
{
    return 20220310;
}

void* storage_vnc_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_vnc_get_version());
    auto ptr = std::make_unique<storage_vnc_t>();
    ptr->debug = config.getInteger("debug", 0);
    ptr->port = config.getInteger("port", 5900);
    ptr->config = & config;
    DEBUG("params: " << "port = " << ptr->port);

    if(! ptr->init())
    {
        ptr->clear();
        return nullptr;
    }

    return ptr.release();
}

void storage_vnc_quit(void* ptr)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(st->debug) DEBUG("version: " << storage_vnc_get_version());

    st->shutdown = true;
    if(st->vnc) st->vnc->shutdown();

    delete st;
}

int storage_vnc_store_action(void* ptr)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(3 < st->debug) DEBUG("version: " << storage_vnc_get_version());

    // always store
    return 1;
}

int storage_vnc_set_surface(void* ptr, const Surface & sf)
{
    auto st = static_cast<storage_vnc_t*>(ptr);

    const std::lock_guard<std::mutex> lock(st->change);
    if(3 < st->debug) DEBUG("version: " << storage_vnc_get_version());
    st->surface.setSurface(sf);
    if(st->vnc)
        st->vnc->setFrameBuffer(st->surface);

    return 0;
}

int storage_vnc_session_reset(void* ptr, const SessionIdName & ss)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(st->debug) DEBUG("version: " << storage_vnc_get_version());

    return 0;
}

const std::string & storage_vnc_get_label(void* ptr)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(st->debug) DEBUG("version: " << storage_vnc_get_version());

    return st->label;
}

const Surface & storage_vnc_get_surface(void* ptr)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(st->debug) DEBUG("version: " << storage_vnc_get_version());

    return st->surface;
}

#ifdef __cplusplus
}
#endif
