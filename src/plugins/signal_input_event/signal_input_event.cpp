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
    bool        is_thread;
    bool        is_debug;
    std::string signal;
    std::string device;
    int		delay;
    int		type;
    int		code;
    int		value;

    signal_input_event_t() : is_thread(true), is_debug(false), delay(100), type(0), code(0), value(-1) {}
    ~signal_input_event_t()
    {
	clear();
    }

    void clear(void)
    {
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

const char* signal_input_event_get_name(void)
{
    return "signal_test";
}

int signal_input_event_get_version(void)
{
    return 20220205;
}

void* signal_input_event_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_input_event_get_version());

    auto ptr = std::make_unique<signal_input_event_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    ptr->signal = config.getString("signal");
    ptr->device = config.getString("device");
    ptr->delay = config.getInteger("delay", 100);
    ptr->type = config.getInteger("event:type", 0);
    ptr->code = config.getInteger("event:code", 0);
    ptr->value = config.getInteger("event:value", -1);
 
    DEBUG("params: " << "signal = " << ptr->signal);
    DEBUG("params: " << "device = " << ptr->device);
    DEBUG("params: " << "delay = " << ptr->delay);
    DEBUG("params: " << "event:type = " << String::hex(ptr->type, 4));
    DEBUG("params: " << "event:code = " << String::hex(ptr->code, 4));
    DEBUG("params: " << "event:value = " << ptr->value);

    return ptr.release();
}

void signal_input_event_quit(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());

    delete st;
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

            switch(select(fd+1, &set, nullptr, nullptr, &tv))
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
            	        	DisplayScene::pushEvent(nullptr, ActionSignalBack, st);
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

const std::string & signal_input_event_get_signal(void* ptr)
{
    signal_input_event_t* st = static_cast<signal_input_event_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_input_event_get_version());

    return st->signal;
}

#ifdef __cplusplus
}
#endif
