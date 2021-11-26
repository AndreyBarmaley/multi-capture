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

struct signal_test_thread_t
{
    bool        is_debug;
    bool        is_thread;
    int         delay;
    std::string signal;

    signal_test_thread_t() : is_debug(false), is_thread(true), delay(1000) {}

    void clear(void)
    {
        is_debug = false;
        is_thread = true;
        delay = 1000;
        signal.clear();
    }
};

signal_test_thread_t signal_test_thread_vals;

const char* signal_test_thread_get_name(void)
{
    return "signal_test";
}

int signal_test_thread_get_version(void)
{
    return 20211121;
}

void* signal_test_thread_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_test_thread_get_version());

    signal_test_thread_t* st = & signal_test_thread_vals;

    st->is_debug = config.getBoolean("debug", false);
    st->delay = config.getInteger("delay", 100);
    st->signal = config.getString("signal");
 
    DEBUG("params: " << "signal = " << st->signal);
    DEBUG("params: " << "delay = " << st->delay);

    return st;
}

void signal_test_thread_quit(void* ptr)
{
    signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_test_thread_get_version());
    st->clear();
}

void signal_test_thread_stop_thread(void* ptr)
{
    signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_test_thread_get_version());

    if(st->is_thread)
        st->is_thread = false;
}

int signal_test_thread_action(void* ptr)
{
    signal_test_thread_t* st = static_cast<signal_test_thread_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_test_thread_get_version());

    // thread loop
    while(st->is_thread)
    {
	VERBOSE("FIXME thread action");

        DisplayScene::pushEvent(NULL, ActionBackSignal, & st->signal);
	Tools::delay(st->delay);
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
