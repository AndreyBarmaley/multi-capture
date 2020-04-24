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

#ifndef WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

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
    mutable void*	data;

    const char*		(*fun_get_name) (void);
    int			(*fun_get_version) (void);
    void*		(*fun_init) (const JsonObject &);
    void		(*fun_quit) (void*);

    bool loadFunctions(void);
    void quit(void) const;

public:
    BasePlugin(const PluginParams &);
    virtual ~BasePlugin();

    const char* pluginName(void) const;
    int pluginVersion(void) const;

    bool isValid(void) const { return lib; }

    bool isInit(void) const { return data; }
    void reInit(void);
};

class CapturePlugin : public BasePlugin
{
    bool		isthread;
    mutable SDL_Thread*	thread;
    Surface		blue;
    bool		initComplete;

protected:
    int			(*fun_frame_action) (void*);
    const Surface &	(*fun_get_surface) (void*);

    bool loadFunctions(void);
    void generateBlueScreen(const std::string &);
    static int initializeBackground(void*);

public:
    CapturePlugin(const PluginParams &);
    ~CapturePlugin();

    int			frameAction(void);
    const Surface &	getSurface(void) const;
    bool		isInitComplete(void) const { return initComplete; }
};

class StoragePlugin : public BasePlugin
{
    bool		isthread;
    mutable SDL_Thread*	thread;
    StringList		signals;

protected:
    int			(*fun_store_action) (void*);
    int			(*fun_set_surface) (void*, const Surface &);
    const Surface &	(*fun_get_surface) (void*);
    const std::string &	(*fun_get_label) (void*);

    bool loadFunctions(void);

public:
    StoragePlugin(const PluginParams &);
    ~StoragePlugin();

    int			storeAction(void);
    int			setSurface(const Surface &);
    std::string		findSignal(const std::string &) const;
    SurfaceLabel	getSurfaceLabel(void) const;
};

class SignalPlugin : public BasePlugin
{
    bool		isthread;
    mutable SDL_Thread*	thread;
    int			tickval;
    mutable int		ticktmp;

protected:
    int			(*fun_action) (void*);

    bool loadFunctions(void);

public:
    SignalPlugin(const PluginParams &);
    ~SignalPlugin();

    bool		isThread(void) const { return  isthread; }
    bool		isTick(u32 ms) const;

    int			signalAction(void);
};

#endif
