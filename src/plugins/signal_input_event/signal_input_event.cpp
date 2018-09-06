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

#include "engine.h"
#include "../../videowindow.h"

#ifdef __cplusplus
extern "C" {
#endif

struct signal_input_event_t
{
    bool        is_used;
    bool        is_debug;
    std::string signal;
    std::string device;
    int		delay;
    int		type;
    int		code;
    int		value;

    signal_input_event_t() : is_used(false), is_debug(false), delay(100), type(0), code(0), value(-1) {}

    void clear(void)
    {
	is_used = false;
        is_debug = false;
        signal.clear();
	device.clear();
	delay = 100;
	type = 0;
	code = 0;
	value = -1;
    }
};

#ifndef INPUT_EVENT_SPOOL
#define INPUT_EVENT_SPOOL 4
#endif

signal_input_event_t signal_input_event_vals[INPUT_EVENT_SPOOL];

const char* signal_input_event_get_name(void)
{
    return "signal_test";
}

int signal_input_event_get_version(void)
{
    return 20180817;
}

void* signal_input_event_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_input_event_get_version());

    int devindex = 0;
    for(; devindex < INPUT_EVENT_SPOOL; ++devindex)
        if(! signal_input_event_vals[devindex].is_used) break;

    if(INPUT_EVENT_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << INPUT_EVENT_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    signal_input_event_t* st = & signal_input_event_vals[devindex];

    st->is_debug = config.getBoolean("debug", "false");
    st->signal = config.getString("signal");
    st->device = config.getString("device");
    st->delay = config.getInteger("delay", 100);
    st->type = config.getInteger("event:type", 0);
    st->code = config.getInteger("event:code", 0);
    st->value = config.getInteger("event:value", -1);
 
    DEBUG("params: " << "signal = " << st->signal);

    st->is_used = true;
    return st;
}

void signal_input_event_quit(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());
    st->clear();
}

int signal_input_event_push_event(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());

    // push event broadcast
    SDL_Event event;
    std::memset(&event, 0, sizeof(event));

    event.type = SDL_USEREVENT;
    event.user.code = ActionBackSignal;
    event.user.data1 = NULL;
    event.user.data2 = & st->signal;

    if(0 > SDL_PushEvent(&event))
    {
        ERROR(SDL_GetError());
        return -1;
    }

    return 0;
}

int signal_input_event_action(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());

    int loop = 1;

    // thread loop
    while(loop)
    {
	StreamFile sf(st->device, "rb");

	if(sf.isValid())
	{
    	    // skip tv_sec
	    sf.skip(4);

    	    // skip tv_usec
	    sf.skip(4);

    	    int type = sf.getLE16();
    	    int code = sf.getLE16();
    	    int value = sf.getLE32();

	    if(type == st->type &&
		code == st->code &&
		(0 > st->value || st->value == value))
	    {
		if(0 > signal_input_event_push_event(ptr))
		    return 0;
	    }

	    sf.close();
	    Tools::delay(st->delay);
	}
	else
	{
	    ERROR("invalid device: " << st->device);
	    // 15 sec reread device
	    Tools::delay(15000);
	}
    }

    // reinitialized plugin disabled
    return 0;
}

#ifdef __cplusplus
}
#endif
