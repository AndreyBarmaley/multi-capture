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

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

struct capture_image_t
{
    bool 	is_used;
    bool 	is_debug;
    bool 	is_static;
    std::string file;
    std::string lock;
    Surface 	surface;

    capture_image_t() : is_used(false), is_debug(false), is_static(true) {}
    
    void clear(void)
    {
	is_used = false;
	is_debug = false;
	is_static = true;
	file.clear();
	surface.reset();
    }
};

#ifndef CAPTURE_IMAGE_SPOOL
#define CAPTURE_IMAGE_SPOOL 16
#endif

capture_image_t capture_image_vals[CAPTURE_IMAGE_SPOOL];

const char* capture_image_get_name(void)
{
    return "capture_image";
}

int capture_image_get_version(void)
{
    return 20211121;
}

void* capture_image_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_image_get_version());

    int devindex = 0;
    for(; devindex < CAPTURE_IMAGE_SPOOL; ++devindex)
        if(! capture_image_vals[devindex].is_used) break;

    if(CAPTURE_IMAGE_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << CAPTURE_IMAGE_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    capture_image_t* st = & capture_image_vals[devindex];

    st->is_used = true;
    st->is_debug = config.getBoolean("debug", false);
    st->is_static = config.getBoolean("static", true);
    st->file = config.getString("file");
    st->lock = config.getString("lock");

    DEBUG("params: " << "static = " << (st->is_static ? "true" : "false"));
    DEBUG("params: " << "file = " << st->file);
    DEBUG("params: " << "lock = " << st->lock);

    return st;
}

void capture_image_quit(void* ptr)
{
    capture_image_t* st = static_cast<capture_image_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_image_get_version());
    st->clear();
}

int capture_image_frame_action(void* ptr)
{
    capture_image_t* st = static_cast<capture_image_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_image_get_version());

    if(! st->lock.empty() && Systems::isFile(st->lock))
    {
	ERROR("skip read, lock present: " << st->lock);
	return 0;
    }

    if(Systems::isFile(st->file))
    {
	if(st->is_static)
	{
	    if(! st->surface.isValid())
		st->surface = Surface(st->file);
	}
	else
	{
	    st->surface = Surface(st->file);
	}

	if(st->surface.isValid())
	    return 0;

	ERROR("unknown image format, file: " << st->file);
    }
    else
    {
	ERROR("file not found: " << st->file);
    }

    // reinitialized plugin disabled
    return 0;
}

const Surface & capture_image_get_surface(void* ptr)
{
    capture_image_t* st = static_cast<capture_image_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << capture_image_get_version());
    return st->surface;
}

#ifdef __cplusplus
}
#endif
