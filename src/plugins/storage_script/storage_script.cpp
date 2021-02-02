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

struct storage_script_t
{
    bool        is_used;
    bool        is_debug;
    std::string label;
    std::string exec;
    std::string image;
    Surface	surface;

    storage_script_t() : is_used(false), is_debug(false) {}

    void clear(void)
    {
        is_used = false;
        is_debug = false;
        label.clear();
        exec.clear();
        image.clear();
	surface.reset();
    }
};

#ifndef STORAGE_SCRIPT_SPOOL
#define STORAGE_SCRIPT_SPOOL 16
#endif

storage_script_t storage_script_vals[STORAGE_SCRIPT_SPOOL];

const char* storage_script_get_name(void)
{
    return "storage_script";
}

int storage_script_get_version(void)
{
    return 20210130;
}

void* storage_script_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_script_get_version());

    int devindex = 0;
    for(; devindex < STORAGE_SCRIPT_SPOOL; ++devindex)
        if(! storage_script_vals[devindex].is_used) break;

    if(STORAGE_SCRIPT_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << STORAGE_SCRIPT_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    storage_script_t* st = & storage_script_vals[devindex];

    st->is_debug = config.getBoolean("debug", false);
    st->exec = config.getString("exec");
    st->image = config.getString("image");

    DEBUG("params: " << "exec = " << st->exec);
    DEBUG("params: " << "image = " << st->image);

    st->is_used = true;
    return st;
}

void storage_script_quit(void* ptr)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_script_get_version());
    st->clear();
}

int storage_script_store_action(void* ptr)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_script_get_version());

    if(Systems::isFile(st->exec))
    {
	if(st->surface.isValid())
	{
    	    DEBUG("save: " << st->image);
    	    st->surface.save(st->image);

    	    if(! Systems::isFile(st->image))
    	    {
		ERROR("write image error");
		return 1;
    	    }

    	    std::string cmd = st->exec;
    	    cmd.append(" ").append(st->image);

    	    system(cmd.c_str());
	    st->label = String::strftime("%Y/%m/%d %H:%M:%S");

    	    return 0;
	}
	else
	{
    	    ERROR("invalid surface");
	}
    }
    else
    {
        ERROR("exec not found: " << st->exec);
    }


    return -1;
}

int storage_script_set_surface(void* ptr, const Surface & sf)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_script_get_version());

    st->surface = sf;
    return 0;
}

const std::string & storage_script_get_label(void* ptr)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_script_get_version());

    return st->label;
}

const Surface & storage_script_get_surface(void* ptr)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << storage_script_get_version());
    return st->surface;
}

#ifdef __cplusplus
}
#endif
