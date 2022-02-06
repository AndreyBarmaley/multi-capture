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

struct capture_script_t
{
    bool        is_debug;
    std::string exec;
    std::string result;
    Surface     surface;

    capture_script_t() : is_debug(false) {}
    ~capture_script_t()
    {
	clear();
    }

    void clear(void)
    {
        is_debug = false;
        exec.clear();
        result.clear();
        surface.reset();
    }
};

const char* capture_script_get_name(void)
{
    return "capture_script";
}

int capture_script_get_version(void)
{
    return 20220205;
}

void* capture_script_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_script_get_version());

    auto ptr = std::make_unique<capture_script_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    ptr->exec = config.getString("exec");
    ptr->result = config.getString("result");

    DEBUG("params: " << "exec = " << ptr->exec);
    DEBUG("params: " << "result = " << ptr->result);

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
