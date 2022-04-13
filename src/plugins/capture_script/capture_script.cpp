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

#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <memory>
#include <iterator>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_script_version = PLUGIN_API;

struct capture_script_t
{
    int         debug;
    std::string command;

    size_t             framesPerSec;
    std::thread        thread;
    std::atomic<bool>  shutdown;
    std::list<Surface> frames;
    std::array<char, 1024> buffer;

    capture_script_t() : debug(0), framesPerSec(1), shutdown(false)
    {
    }

    ~capture_script_t()
    {
	clear();
    }

    void clear(void)
    {
        shutdown = true;
        if(thread.joinable())
            thread.join();

        debug = 0;
        framesPerSec = 1;

        frames.clear();
        command.clear();
    }

    std::string runCommand(void)
    {
        std::unique_ptr<FILE, decltype(pclose)*> pipe{ popen(command.c_str(), "r"), pclose };

        std::string fileImage;
        std::fill(buffer.begin(), buffer.end(), 0);

        // store command stdout to result fileImage
        while(!std::feof(pipe.get()))
        {
            if(std::fgets(buffer.data(), buffer.size() - 1, pipe.get()))
                fileImage.append(buffer.data());
        }

        if(fileImage.size() && fileImage.back() == '\n')
            fileImage.erase(std::prev(fileImage.end()));

        return fileImage;
    }

    void start(void)
    {
        shutdown = false;
                        
        // frames loop
        thread = std::thread([st = this]
        {
            Surface frame;
            size_t duration = 1000/st->framesPerSec;
            auto point = std::chrono::steady_clock::now();

            while(! st->shutdown)
            {
                if(5 < st->frames.size())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
                    point = std::chrono::steady_clock::now();
                    continue;
                }

                auto now = std::chrono::steady_clock::now();
                auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - point);

                // limit fps
                if(timeMS < std::chrono::milliseconds(duration))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                else
                if(timeMS > std::chrono::milliseconds(duration))
                {
                    if(st->debug)
                        ERROR("incorrect fps:sec, calculate: " << 1000 / timeMS.count() << "fps");
                    duration = timeMS.count();
                }   

                std::string fileImage = st->runCommand();
                if(3 < st->debug)
                {
                    DEBUG("command result image: " << fileImage);
                }

                if(Systems::isFile(fileImage))
                {
                    Surface frame(fileImage);

                    if(! frame.isValid())
                    {
                        ERROR("unknown image format, file: " << fileImage);
                        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
                        point = std::chrono::steady_clock::now();
                        continue;
                    }

                    st->frames.push_back(frame);
                    DisplayScene::pushEvent(nullptr, ActionFrameComplete, st);
                    point = now;
                }
                else
                if(3 < st->debug)
                {
	            ERROR("file not found: " << fileImage);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
};

void* capture_script_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_script_version);

    auto ptr = std::make_unique<capture_script_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->command = config.getString("exec");
    ptr->framesPerSec = config.getInteger("frames:sec", 1);
    if(0 == ptr->framesPerSec || ptr->framesPerSec > 1000)
    {
        ERROR("frames:sec param incorrect, set default");
        ptr->framesPerSec = 1;
    }

    if(! Systems::isFile(ptr->command))
    {
        ERROR("not command present: " << ptr->command);
        ptr->clear();
        return nullptr;
    }

    DEBUG("params: " << "frames:sec = " << ptr->framesPerSec);
    DEBUG("params: " << "exec = " << ptr->command);

    ptr->start();

    return ptr.release();
}

void capture_script_quit(void* ptr)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(st->debug) DEBUG("version: " << capture_script_version);

    delete st;
}

bool capture_script_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_script");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_script_version;
                return true;
            }
            break;

        case PluginValue::PluginType:
            if(auto res = static_cast<int*>(val))
            {
                *res = PluginType::Capture;
                return true;
            }
            break;

        default:
            break;
    }

    if(ptr)
    {
        capture_script_t* st = static_cast<capture_script_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << capture_script_version << ", type: " << type);
    
        switch(type)
        {
            case PluginValue::CaptureSurface:
                if(auto res = static_cast<Surface*>(val))
                {
                    if(! st->frames.empty())
                    {
                        res->setSurface(st->frames.front());
                        st->frames.pop_front();
                        return true;
                    }
                    return false;
                }
                break;

            default: break;
        }
    }

    return false;
}

bool capture_script_set_value(void* ptr, int type, const void* val)
{
    capture_script_t* st = static_cast<capture_script_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_script_version << ", type: " << type);

    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
