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

#ifndef _CNA_SETTINGS_
#define _CNA_SETTINGS_

#include <string>

#include "libswe.h"
using namespace SWE;

#define VERSION 20210201
enum { ActionNone = 11110, ActionFrameComplete = 11111, ActionFrameError = 11112, ActionBackStore = 11113, ActionBackSignal = 11114, ActionStoreComplete = 11115 };
 
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

#endif
