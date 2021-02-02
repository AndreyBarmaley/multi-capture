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

#ifndef _CNA_PLUGINS_
#define _CNA_PLUGINS_

#if defined(__WIN32__)
#include <windows.h>
#undef ERROR
#undef DELETE
#undef MessageBox
#define ERROR(x)   { PRETTY("[ERROR]", x); }
#else
#include <dlfcn.h>
#endif

#include <atomic>
#include "settings.h"

enum { PluginReturnTrue = 0, PluginReturnFalse = -1, PluginReturnClose = -2 };

struct SurfaceLabel : std::pair<Surface, std::string>
{
    SurfaceLabel(const Surface sf, const std::string & lb) : std::pair<Surface, std::string>(sf, lb) {}
    SurfaceLabel() {}

    const Surface &     surface(void) const { return first; }
    const std::string & label(void) const { return second; }
};

struct PluginParams
{
    std::string         name;
    std::string         file;
    JsonObject          config;

    PluginParams() {}
    PluginParams(const JsonObject &);
};

class BasePlugin : protected PluginParams
{
protected:
    void*		lib;
    void*	        data;
    TickTrigger		tt;

    const char*		(*fun_get_name) (void);
    int			(*fun_get_version) (void);
    void*		(*fun_init) (const JsonObject &);
    void		(*fun_quit) (void*);

    std::atomic<bool>	threadInitialize;
    std::atomic<bool>	threadAction;
    std::atomic<bool>	threadExit;

    Window*             parent;
    SDL_Thread*	        thread;

    bool                loadFunctions(void);

public:
    BasePlugin(const PluginParams &, Window &);
    virtual ~BasePlugin();

    const char*         pluginName(void) const;
    int                 pluginVersion(void) const;

    bool                isValid(void) const { return lib; }
    bool		isInitComplete(void) const { return threadInitialize; }
};

class CapturePlugin : public BasePlugin
{
    Surface		blue;

protected:
    int			(*fun_frame_action) (void*);
    const Surface &	(*fun_get_surface) (void*);

    bool                loadFunctions(void);
    Surface             generateBlueScreen(const std::string &) const;

    static int          runThreadInitialize(void*);
    static int          runThreadAction(void*);

public:
    CapturePlugin(const PluginParams &, Window &);

    int			frameAction(void);
    const Surface &	getSurface(void);
};

class StoragePlugin : public BasePlugin
{
    StringList		signals;

protected:
    int			(*fun_store_action) (void*);
    int			(*fun_set_surface) (void*, const Surface &);
    const Surface &	(*fun_get_surface) (void*);
    const std::string &	(*fun_get_label) (void*);

    bool loadFunctions(void);

    static int          runThreadInitialize(void*);
    static int          runThreadAction(void*);

public:
    StoragePlugin(const PluginParams &, Window &);

    int			storeAction(void);
    int			setSurface(const Surface &);

    std::string		findSignal(const std::string &) const;
    SurfaceLabel	getSurfaceLabel(void);
};

class SignalPlugin : public BasePlugin
{
    int			tickval;
    bool                isthread;

protected:
    int			(*fun_action) (void*);

    bool loadFunctions(void);

public:
    SignalPlugin(const PluginParams &, Window &);

    bool		isTick(u32 ms) const;
    int			signalAction(void);
};

#endif
