/***************************************************************************
 *   Copyright (C) 2018 by FlyCapture team <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   Part of the FlyCapture engine:                                        *
 *   https://github.com/AndreyBarmaley/fly-capture                         *
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

struct signal_test_tick_t
{
    bool        is_debug;
    std::string signal;

    signal_test_tick_t() : is_debug(false) {}

    void clear(void)
    {
        is_debug = false;
        signal.clear();
    }
};

signal_test_tick_t signal_test_tick_vals;

const char* signal_test_tick_get_name(void)
{
    return "signal_test";
}

int signal_test_tick_get_version(void)
{
    return 20210130;
}

void* signal_test_tick_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_test_tick_get_version());

    signal_test_tick_t* st = & signal_test_tick_vals;

    st->is_debug = config.getBoolean("debug", false);
    st->signal = config.getString("signal");
 
    DEBUG("params: " << "signal = " << st->signal);

    return st;
}

void signal_test_tick_quit(void* ptr)
{
    signal_test_tick_t* st = static_cast<signal_test_tick_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_test_tick_get_version());
    st->clear();
}

int signal_test_tick_action(void* ptr)
{
    signal_test_tick_t* st = static_cast<signal_test_tick_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_test_tick_get_version());

    DisplayScene::pushEvent(NULL, ActionBackSignal, & st->signal);
    return 0;
}

#ifdef __cplusplus
}
#endif
