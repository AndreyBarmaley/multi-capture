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

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

struct capture_script_t
{
    bool        is_used;
    bool        is_debug;
    std::string exec;
    std::string result;
    Surface     surface;

    capture_script_t() : is_used(false), is_debug(false) {}

    void clear(void)
    {
        is_used = false;
        is_debug = false;
        exec.clear();
        result.clear();
        surface.reset();
    }
};

#ifndef CAPTURE_SCRIPT_SPOOL
#define CAPTURE_SCRIPT_SPOOL 4
#endif

capture_script_t capture_script_vals[CAPTURE_SCRIPT_SPOOL];

const char* capture_script_get_name(void)
{
    return "capture_script";
}

int capture_script_get_version(void)
{
    return 20180817;
}

void* capture_script_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_script_get_version());

    int devindex = 0;
    for(; devindex < CAPTURE_SCRIPT_SPOOL; ++devindex)
        if(! capture_script_vals[devindex].is_used) break;

    if(CAPTURE_SCRIPT_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << CAPTURE_SCRIPT_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    capture_script_t* st = & capture_script_vals[devindex];

    st->is_debug = config.getBoolean("debug", false);
    st->exec = config.getString("exec");
    st->result = config.getString("result");

    DEBUG("params: " << "exec = " << st->exec);
    DEBUG("params: " << "result = " << st->result);

    st->is_used = true;
    return st;
}

void capture_script_quit(void* ptr)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_script_get_version());
    st->clear();
}

int capture_script_frame_action(void* ptr)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_script_get_version());

    if(Systems::isFile(st->exec))
    {
	std::string cmd = st->exec;
	cmd.append(" ").append(st->result);

	system(cmd.c_str());
	st->surface = Surface(st->result);

	return 0;
    }
    else
    {
	ERROR("exec not found: " << st->exec);
    }

    // reinitialized plugin disabled
    return 0;
}

const Surface & capture_script_get_surface(void* ptr)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_script_get_version());

    return st->surface;
}

#ifdef __cplusplus
}
#endif
