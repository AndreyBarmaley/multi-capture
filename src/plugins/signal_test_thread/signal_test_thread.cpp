/***************************************************************************
 *   Copyright (C) 2018 by MultiCapture team <public.irkutsk@gmail.com>    *
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

#include <cstring>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

const int signal_test_thread_version = PLUGIN_API;

struct signal_test_thread_t
{
    int         debug;
    bool        is_thread;
    int         delay;
    std::string signal;

    signal_test_thread_t() : debug(0), is_thread(true), delay(1000) {}
    ~signal_test_thread_t()
    {
	clear();
    }

    void clear(void)
    {
        debug = 0;
        is_thread = true;
        delay = 1000;
        signal.clear();
    }
};

void* signal_test_thread_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_test_thread_version);

    auto ptr = std::make_unique<signal_test_thread_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->delay = config.getInteger("delay", 100);
    ptr->signal = config.getString("signal");
 
    DEBUG("params: " << "signal = " << ptr->signal);
    DEBUG("params: " << "delay = " << ptr->delay);

    return ptr.release();
}

void signal_test_thread_quit(void* ptr)
{
    signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
    if(st->debug) DEBUG("version: " << signal_test_thread_version);

    delete st;
}

int signal_test_thread_action(void* ptr)
{
    signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
    if(3 < st->debug) DEBUG("version: " << signal_test_thread_version);

    // thread loop
    while(st->is_thread)
    {
	VERBOSE("FIXME thread action");

        DisplayScene::pushEvent(nullptr, ActionSignalName, (void*) & st->signal);
	Tools::delay(st->delay);
    }

    return PluginResult::DefaultOk;
}

bool signal_test_thread_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("signal_test_thread");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = signal_test_thread_version;
                return true;
            }
            break;

        case PluginValue::PluginType:
            if(auto res = static_cast<int*>(val))
            {
                *res = PluginType::Signal;
                return true;
            }
            break;

        default: break;
    }       

    if(ptr)
    {
        signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << signal_test_thread_version << ", type: " << type);
            
        switch(type)
        {
            default: break;
        }
    }

    return false;
}

bool signal_test_thread_set_value(void* ptr, int type, const void* val)
{
    signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << signal_test_thread_version << ", type: " << type);

    switch(type)
    {
        case PluginValue::SignalStopThread:
            if(st->is_thread)
                st->is_thread = false;
            break;

        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
