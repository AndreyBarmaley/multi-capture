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

#ifndef _CNA_SETTINGS_
#define _CNA_SETTINGS_

#include <mutex>
#include <string>

#include "libswe.h"
using namespace SWE;

#define VERSION 20230227
#define PLUGIN_API 20220406

enum { ActionNone = 11110, ActionFrameComplete = 11111, ActionCaptureReset = 11112,
        ActionStorageBack = 11113, ActionSignalName = 11114, ActionPushGallery = 11115,
        ActionSessionReset = 11116, ActionStorageReset = 11117, ActionProgramExit = 11119 };

#ifdef SWE_SDL12
#include "SDL_rotozoom.h"
#else
#include "SDL2_rotozoom.h"
#endif

struct SessionIdName
{
    size_t id = 0;
    std::string name;
};

namespace PluginType
{
    enum { Unknown = 0, Capture = 1, Signal = 2, Storage = 3 };
}

namespace PluginResult
{
    enum { Reset = -2, Failed = -1, NoAction = 0, DefaultOk = 1 };
}

namespace PluginValue
{
    enum { Unknown = 0, PluginName = 1, PluginVersion = 2, PluginType = 3, PluginAPI = 4,
            CaptureSurface = 11,
            SignalStopThread = 22,
            StorageLocation = 31, StorageSurface = 32,
            SessionId = 41, SessionName = 42, InitGui = 43 };

    const char* getName(int);
}

namespace Settings
{
    std::string		programName(void);
    std::string		programDomain(void);
    std::string		programVersion(void);

    std::string		dataLang(const std::string &);
    std::string 	dataFonts(const std::string &);
    std::string		dataJson(const std::string &);
    std::string		dataPath(void);
}

struct Frames : protected std::list<Surface>
{
    mutable std::mutex mt;

    Frames()
    {
    }

    bool empty(void) const
    {
        return std::list<Surface>::empty();
    }
    
    size_t size(void) const
    {
        const std::lock_guard<std::mutex> lock(mt);
        return std::list<Surface>::size();
    }
    
    void clear(void)
    {
        const std::lock_guard<std::mutex> lock(mt);
        std::list<Surface>::clear();
    }
    
    void push_back(const Surface & sf)
    {
        const std::lock_guard<std::mutex> lock(mt);
        std::list<Surface>::push_back(sf);
    }

    void emplace_back(Surface && sf)
    {
        const std::lock_guard<std::mutex> lock(mt);
        std::list<Surface>::emplace_back(std::move(sf));
    }

    void pop_front(void)
    {
        const std::lock_guard<std::mutex> lock(mt);
        std::list<Surface>::pop_front();
    }

    Surface front(void) const
    {
        return std::list<Surface>::front();
    }

    Surface back(void) const
    {
        return std::list<Surface>::back();
    }
};

#endif
