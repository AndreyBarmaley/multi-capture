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

#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/select.h>

#include <cstdio>
#include <cstdlib>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

struct signal_input_event_t
{
    bool        is_used;
    bool        is_thread;
    bool        is_debug;
    std::string signal;
    std::string device;
    int		delay;
    int		type;
    int		code;
    int		value;

    signal_input_event_t() : is_used(false), is_thread(true), is_debug(false), delay(100), type(0), code(0), value(-1) {}

    void clear(void)
    {
	is_used = false;
	is_thread = true;
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
#define INPUT_EVENT_SPOOL 16
#endif

signal_input_event_t signal_input_event_vals[INPUT_EVENT_SPOOL];

const char* signal_input_event_get_name(void)
{
    return "signal_test";
}

int signal_input_event_get_version(void)
{
    return 20211121;
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

    st->is_used = true;
    st->is_debug = config.getBoolean("debug", false);
    st->signal = config.getString("signal");
    st->device = config.getString("device");
    st->delay = config.getInteger("delay", 100);
    st->type = config.getInteger("event:type", 0);
    st->code = config.getInteger("event:code", 0);
    st->value = config.getInteger("event:value", -1);
 
    DEBUG("params: " << "signal = " << st->signal);
    DEBUG("params: " << "device = " << st->device);
    DEBUG("params: " << "delay = " << st->delay);
    DEBUG("params: " << "event:type = " << String::hex(st->type, 4));
    DEBUG("params: " << "event:code = " << String::hex(st->code, 4));
    DEBUG("params: " << "event:value = " << st->value);

    return st;
}

void signal_input_event_quit(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());
    st->clear();
}

void signal_input_event_stop_thread(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());

    if(st->is_thread)
	st->is_thread = false;
}

int signal_input_event_action(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());

    int fd = 0;
    int res = 0;
    struct input_event ev;
    fd_set set;
    struct timeval tv;

    tv.tv_sec  = 0;
    tv.tv_usec = st->delay ? st->delay : 1;

    // thread loop
    while(st->is_thread)
    {
        if(! fd)
            fd = open(st->device.c_str(), O_RDONLY);

        if(fd)
        {
            FD_ZERO(&set);
            FD_SET(fd, &set);

            switch(select(fd+1, &set, NULL, NULL, &tv))
            {
                case -1: ERROR("select failed"); close(fd); fd = 0; break;
                case 0: break;

                default:
                    if(FD_ISSET(fd, &set))
                    {
                        if(0 < read(fd, & ev, sizeof(ev)))
                        {
        		    if(st->is_debug)
                        	VERBOSE("dump input - " << "type: " << String::hex(ev.type, 4) <<
                            	    ", code: " << String::hex(ev.code, 4) << ", value: " << ev.value);

			    if(ev.type == st->type && ev.code == st->code &&
		        	(0 > st->value || st->value == ev.value))
            	        	DisplayScene::pushEvent(NULL, ActionBackSignal, & st->signal);
                        }
                        else
                        {
                            ERROR("read failed");
			    close(fd); fd = 0;
                        }
                    }
                    break;
            }
        }
        else
        {
            ERROR("file open: " << st->device);
            // 5 sec reread device
	    if(st->is_thread) Tools::delay(5000);
        }
    }

    if(fd)
        close(fd);

    return res;
}

#ifdef __cplusplus
}
#endif
