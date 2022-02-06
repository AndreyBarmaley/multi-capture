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

#include <string>

#include "libswe.h"
using namespace SWE;

#define VERSION 20220205
enum { ActionNone = 11110, ActionFrameComplete = 11111, ActionPluginReset = 11112,
        ActionUnused11113 = 11113, ActionSignalBack = 11114, ActionStoreComplete = 11115,
        ActionSessionReset = 11116, ActionProgramExit = 11119 };

struct SessionIdName
{
    size_t      id = 0;
    std::string name;
};

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

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define Rmask 0xff000000
#define Gmask 0x00ff0000
#define Bmask 0x0000ff00
#define Amask 0x000000ff
#else
#define Rmask 0x000000ff
#define Gmask 0x0000ff00
#define Bmask 0x00ff0000
#define Amask 0xff000000
#endif

#endif
