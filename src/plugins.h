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
#include <thread>

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
    std::string         type;
    std::string         name;
    std::string         file;
    JsonObject          config;

    PluginParams() {}
    PluginParams(const JsonObject &);

    bool                isCapture(void) const;
    bool                isSignal(void) const;
    bool                isStorage(void) const;
};

class BasePlugin : protected PluginParams
{
protected:
    void*		lib;
    void*	        data;

    bool		(*fun_set_value) (void* ptr, int type, const void* val);
    bool		(*fun_get_value) (void* ptr, int type, void* val);
    void*		(*fun_init) (const JsonObject &);
    void		(*fun_quit) (void*);

    std::atomic<bool>	threadInitialize;
    std::atomic<bool>	threadAction;
    std::atomic<bool>	threadExit;
    std::atomic<int>    threadResult;

    Window*             parent;
    std::thread	        thread;

    bool                loadFunctions(void);
    void                joinThread(void);

public:
    BasePlugin(const PluginParams &, Window &);
    virtual ~BasePlugin();

    std::string         pluginName(void) const;
    int                 pluginVersion(void) const;
    int                 pluginType(void) const;

    bool                isValid(void) const { return lib; }
    bool		isInitComplete(void) const { return threadInitialize; }
    bool		isData(void* ptr) const { return ptr && data == ptr; }
    const PluginParams &pluginParams(void) const;

    void                stopThread(void);
};

class CapturePlugin : public BasePlugin
{
    Surface		surf;

    bool                scaleImage;
    bool                blueFormat;

protected:
    bool                loadFunctions(void);
    Surface             generateBlueScreen(const std::string &) const;

public:
    CapturePlugin(const PluginParams &, Window &);
    ~CapturePlugin();

    const Surface &     getSurface(void);
    bool                isBlue(const Surface &) const;
    bool		isScaleImage(void) const;
};

class StoragePlugin : public BasePlugin
{
    StringList		signals;

    TickTrigger		ttStorage;
    int                 tickPeriod;
    std::string         threadSignal;

protected:
    int			(*fun_store_action) (void*, const std::string &);

    bool                loadFunctions(void);

public:
    StoragePlugin(const PluginParams &, Window &);
    ~StoragePlugin();

    int			storeAction(const std::string &);
    void		setSurface(const Surface &);
    void		sessionReset(const SessionIdName &);

    std::string		findSignal(const std::string &, bool strong) const;
    SurfaceLabel	getSurfaceLabel(void);
    bool		isTickEvent(u32 ms) const;
};

class SignalPlugin : public BasePlugin
{
protected:
    int			(*fun_action) (void*);

    bool                loadFunctions(void);

public:
    SignalPlugin(const PluginParams &, Window &);
    ~SignalPlugin();
};

#endif
