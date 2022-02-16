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
    bool        is_debug;
    bool        is_overwrite;
    std::string label;
    std::string format;
    Surface	surface;
    SessionIdName session;

    storage_file_t() : is_debug(false), is_overwrite(false) {}
    ~storage_file_t()
    {
	clear();
    }

    void clear(void)
    {
        is_debug = false;
        is_overwrite = false;
        label.clear();
        format.clear();
	surface.reset();
    }
};

const char* storage_file_get_name(void)
{
    return "storage_file";
}

int storage_file_get_version(void)
{
    return 20220214;
}

void* storage_file_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_file_get_version());

    auto ptr = std::make_unique<storage_file_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    ptr->is_overwrite = config.getBoolean("overwrite", false);
    ptr->format = config.getString("format");

    DEBUG("params: " << "format = " << ptr->format);

    return ptr.release();
}

void storage_file_quit(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());

    delete st;
}

int storage_file_store_action(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());

    if(st->surface.isValid())
    {
	std::string filename = String::strftime(st->format);

	if(0 < st->session.id)
	{
    	    filename = String::replace(filename, "${sid}", st->session.id);
    	    filename = String::replace(filename, "${session}", st->session.name);
	}

	std::string dir = Systems::dirname(filename);

	if(! Systems::isDirectory(dir))
	{
	    if(! Systems::makeDirectory(dir, 0775))
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

int storage_file_session_reset(void* ptr, const SessionIdName & ss)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << storage_file_get_version());

    st->session = ss;

    return 0;
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
