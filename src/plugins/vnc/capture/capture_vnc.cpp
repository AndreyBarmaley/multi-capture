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
#include "capture_vnc_connector.h"

#ifdef __cplusplus
extern "C" {
#endif

struct capture_vnc_t
{
    int 	is_debug;
    int         port;
    std::string host;
    std::string password;
    Surface 	surface;

    std::thread thread;
    std::mutex change;
    const SWE::JsonObject* config;
    std::unique_ptr<RFB::ClientConnector> vnc;

    capture_vnc_t() : is_debug(0), port(5900)
    {
    }

    ~capture_vnc_t()
    {
	clear();
    }

    void clear(void)
    {
        if(vnc)
        {
            vnc->shutdown();
            if(thread.joinable()) thread.join();
        }

	is_debug = 0;
        port = 5900;
        host.clear();
        password.clear();
        surface.reset();
    }

    static void start_thread(capture_vnc_t* st)
    {
        try
        {
            st->vnc->messages();
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

    bool init(void)
    {
        bool handshake = false;
        vnc.reset(new RFB::ClientConnector(config));

        try
        {
            handshake = vnc->communication(host, port, password);
        }
        catch(const std::exception & err)
        {
            ERROR("exception: " << err.what());
        }
        catch(...)
        {
            ERROR("exception: " << "unknown");
        }

        if(handshake)
            thread = std::thread([this]{ start_thread(this); });

        return handshake;
    }
};

const char* capture_vnc_get_name(void)
{
    return "capture_vnc";
}

int capture_vnc_get_version(void)
{
    return 20220313;
}

void* capture_vnc_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_vnc_get_version());

    auto ptr = std::make_unique<capture_vnc_t>();

    ptr->is_debug = config.getInteger("debug", 0);
    ptr->port = config.getInteger("port", 5900);
    ptr->host = config.getString("host");
    ptr->password = config.getString("password");
    ptr->config = & config;

    if(ptr->is_debug)
    {
        DEBUG("params: " << "host = " << ptr->host);
        DEBUG("params: " << "port = " << ptr->port);
    }

    if(! ptr->init())
    {
        ptr->clear();
        return nullptr;
    }

    return ptr.release();
}

void capture_vnc_quit(void* ptr)
{
    capture_vnc_t* st = static_cast<capture_vnc_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_vnc_get_version());

    delete st;
}

int capture_vnc_frame_action(void* ptr)
{
    capture_vnc_t* st = static_cast<capture_vnc_t*>(ptr);
    if(5 < st->is_debug) DEBUG("version: " << capture_vnc_get_version());

    if(st->vnc)
    {
        const std::lock_guard<std::mutex> lock(st->change);
        st->vnc->syncFrameBuffer(st->surface);
        return 0;
    }

    return -1;
}

const Surface & capture_vnc_get_surface(void* ptr)
{
    capture_vnc_t* st = static_cast<capture_vnc_t*>(ptr);

    if(5 < st->is_debug) DEBUG("version: " << capture_vnc_get_version());

    const std::lock_guard<std::mutex> lock(st->change);
    return st->surface;
}

#ifdef __cplusplus
}
#endif
