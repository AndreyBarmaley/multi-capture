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

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

struct capture_image_t
{
    bool 	is_debug;
    bool 	is_static;
    std::string file;
    std::string lock;
    Surface 	surface;

    capture_image_t() : is_debug(false), is_static(true) {}
    ~capture_image_t()
    {
	clear();
    }

    void clear(void)
    {
	is_debug = false;
	is_static = true;
	file.clear();
	surface.reset();
    }
};

const char* capture_image_get_name(void)
{
    return "capture_image";
}

int capture_image_get_version(void)
{
    return 20220205;
}

void* capture_image_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_image_get_version());

    auto ptr = std::make_unique<capture_image_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    ptr->is_static = config.getBoolean("static", true);
    ptr->file = config.getString("file");
    ptr->lock = config.getString("lock");

    DEBUG("params: " << "static = " << (ptr->is_static ? "true" : "false"));
    DEBUG("params: " << "file = " << ptr->file);
    DEBUG("params: " << "lock = " << ptr->lock);

    return ptr.release();
}

void capture_image_quit(void* ptr)
{
    capture_image_t* st = static_cast<capture_image_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_image_get_version());

    delete st;
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
