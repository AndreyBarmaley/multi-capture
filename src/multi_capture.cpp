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

#include <clocale>

#include "settings.h"
#include "mainscreen.h"

bool translationInit(void)
{
    Translation::setStripContext('|');
    const std::string trans = Settings::dataLang(Systems::messageLocale(1).append(".mo"));

    if(Translation::bindDomain(Settings::programDomain(), trans))
    {
        VERBOSE("loaded from: " << trans);
        //Translation::setDomain(Settings::domain());
        return true;
    }

    return false;
}

int main(int argc, char **argv)
{
    LogWrapper::init("multi_capture", argv[0]);

#ifndef ANDROID
    try
#endif
    {
        Systems::setLocale(LC_ALL, "");
	Systems::setLocale(LC_NUMERIC, "C"); // fix xml parsing decimal point

	translationInit();

	if(! Engine::init())
    	    return EXIT_FAILURE;

	std::string config = "windows.json";
	int opt;

	while((opt = Systems::GetCommandOptions(argc, argv, "c:")) != -1)
	switch(opt)
	{
    	    case 'c':
		if(Systems::GetOptionsArgument())
            	    config = Systems::GetOptionsArgument();
        	break;

    	    default: break;
	}

	JsonContentFile	json(config);

	if(json.isValid() && json.isObject())
	{
	    JsonObject jo = json.toObject();
	    const std::string title = Settings::programName().append(", version: ").append(Settings::programVersion());

	    Size geometry = JsonUnpack::size(jo, "display:geometry", Size(1280, 800));
	    bool fullscreen = jo.getBoolean("display:fullscreen", false);

	    if(! Display::init(title, geometry, fullscreen))
    		return EXIT_FAILURE;

	    MainScreen(jo).exec();
	}
	else
	{
	    ERROR("config not found: " << config);
	}
    }
#ifndef ANDROID
    catch(Engine::exception &)
    {
    }
#endif
    return EXIT_SUCCESS;
}
