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

#include <cstdio>
#include <memory>
#include <iterator>
#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

struct capture_script_t
{
    bool        is_debug;
    std::string command;
    Surface     surface;

    capture_script_t() : is_debug(false) {}
    ~capture_script_t()
    {
	clear();
    }

    void clear(void)
    {
        is_debug = false;
        command.clear();
        surface.reset();
    }
};

const char* capture_script_get_name(void)
{
    return "capture_script";
}

int capture_script_get_version(void)
{
    return 20220214;
}

void* capture_script_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_script_get_version());

    auto ptr = std::make_unique<capture_script_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    ptr->command = config.getString("exec");

    DEBUG("params: " << "exec = " << ptr->command);

    return ptr.release();
}

void capture_script_quit(void* ptr)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_script_get_version());
    delete st;
}

int capture_script_frame_action(void* ptr)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_script_get_version());

    if(! Systems::isFile(st->command))
    {
        ERROR("not command present: " << st->command);
        return -1;
    }

    std::unique_ptr<FILE, decltype(pclose)*> pipe{ popen(st->command.c_str(), "r"), pclose };

    if(!pipe)
    {
        ERROR("popen failed: " << st->command);
        return -1;
    }

    char buffer[128];
    std::string fileImage;
                
    while(!std::feof(pipe.get()))
    {
        if(std::fgets(buffer, sizeof(buffer), pipe.get()))
            fileImage.append(buffer);
    }
        
    if(fileImage.size() && fileImage.back() == '\n')
        fileImage.erase(std::prev(fileImage.end()));

    if(! Systems::isFile(fileImage))
    {
	ERROR("not file: " << fileImage);
	// reinitialized plugin disabled
	return 0;
    }

    st->surface = Surface(fileImage);
    if(! st->surface.isValid())
    {
	ERROR("not image file: " << fileImage);
	// reinitialized plugin disabled
	return 0;
    }

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
