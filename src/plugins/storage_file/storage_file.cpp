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

#include <sys/stat.h>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

struct storage_file_t
{
    bool        is_used;
    bool        is_debug;
    bool        is_overwrite;
    std::string label;
    std::string format;
    Surface	surface;

    storage_file_t() : is_used(false), is_debug(false), is_overwrite(false) {}

    void clear(void)
    {
        is_used = false;
        is_debug = false;
        is_overwrite = false;
        label.clear();
        format.clear();
	surface.reset();
    }
};

#ifndef STORAGE_FILE_SPOOL
#define STORAGE_FILE_SPOOL 16
#endif

storage_file_t storage_file_vals[STORAGE_FILE_SPOOL];

const char* storage_file_get_name(void)
{
    return "storage_file";
}

int storage_file_get_version(void)
{
    return 20211121;
}

void* storage_file_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_file_get_version());

    int devindex = 0;
    for(; devindex < STORAGE_FILE_SPOOL; ++devindex)
        if(! storage_file_vals[devindex].is_used) break;

    if(STORAGE_FILE_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << STORAGE_FILE_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    storage_file_t* st = & storage_file_vals[devindex];

    st->is_used = true;
    st->is_debug = config.getBoolean("debug", false);
    st->is_overwrite = config.getBoolean("overwrite", false);
    st->format = config.getString("format");

    DEBUG("params: " << "format = " << st->format);

    return st;
}

void storage_file_quit(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());
    st->clear();
}

int storage_file_store_action(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());

    if(st->surface.isValid())
    {
	std::string filename = String::strftime(st->format);
	std::string dir = Systems::dirname(filename);

	if(! Systems::isDirectory(dir))
	{
	    if(! Systems::makeDirectory(dir, 0755))
	    {
		ERROR("error mkdir: " << dir);
		return -1;
	    }
	}

	if(! Systems::isFile(filename) || st->is_overwrite)
	{
	    DEBUG("save: " << filename);
	    st->surface.save(filename);
	    st->label = filename;
	}
	else
	{
	    ERROR("file present: " << filename << ", overwrite skipping...");
	}

	return 0;
    }

    return -1;
}

int storage_file_set_surface(void* ptr, const Surface & sf)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());

    st->surface = sf;
    return 0;
}

const Surface & storage_file_get_surface(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << storage_file_get_version());
    return st->surface;
}

const std::string & storage_file_get_label(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());

    return st->label;
}

#ifdef __cplusplus
}
#endif
