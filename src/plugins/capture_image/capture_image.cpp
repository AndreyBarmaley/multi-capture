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

#include <atomic>
#include <thread>
#include <chrono>
#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_image_version = 20220412;

struct capture_image_t
{
    int 	debug;
    bool 	staticImage;
    std::string fileImage;
    std::string fileLock;

    size_t             framesPerSec;
    std::thread        thread;
    std::atomic<bool>  shutdown;
    Frames             frames;

    capture_image_t() : debug(0), staticImage(true), framesPerSec(1), shutdown(false)
    {
    }

    ~capture_image_t()
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

	staticImage = true;
	fileImage.clear();
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
        
                if(! st->fileLock.empty() && Systems::isFile(st->fileLock))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

                if(3 < st->debug)
                {
                    DEBUG("calculate fps: " << 1000 / timeMS.count() << "fps");
                }

                if(Systems::isFile(st->fileImage))
                {
	            Surface frame(st->fileImage);

	            if(! frame.isValid())
                    {
	                ERROR("unknown image format, file: " << st->fileImage);
                        std::this_thread::sleep_for(std::chrono::milliseconds(300));
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
	            ERROR("file not found: " << st->fileImage);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
};

void* capture_image_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_image_version);

    auto ptr = std::make_unique<capture_image_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->framesPerSec = config.getInteger("frames:sec", 1);
    if(0 == ptr->framesPerSec || ptr->framesPerSec > 1000)
    {
        ERROR("frames:sec param incorrect, set default");
        ptr->framesPerSec = 1;
    }

    ptr->staticImage = config.getBoolean("static", true);
    ptr->fileImage = config.getString("image");
    ptr->fileLock = config.getString("lock");

    DEBUG("params: " << "frames:sec = " << ptr->framesPerSec);
    DEBUG("params: " << "static = " << (ptr->staticImage ? "true" : "false"));
    DEBUG("params: " << "image = " << ptr->fileImage);
    DEBUG("params: " << "lock = " << ptr->fileLock);

    if(ptr->staticImage)
    {
        Surface frame(ptr->fileImage);
        if(! frame.isValid())
        {
	    ERROR("unknown image format, file: " << ptr->fileImage);
            ptr->clear();
            return nullptr;
        }

        ptr->frames.push_back(frame);
        DisplayScene::pushEvent(nullptr, ActionFrameComplete, ptr.get());
    }
    else
    {
        ptr->start();
    }

    return ptr.release();
}

void capture_image_quit(void* ptr)
{
    capture_image_t* st = static_cast<capture_image_t*>(ptr);
    if(st->debug) DEBUG("version: " << capture_image_version);

    delete st;
}

bool capture_image_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_image");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_image_version;
                return true;
            }
            break;

        case PluginValue::PluginAPI:
            if(auto res = static_cast<int*>(val))
            {
                *res = PLUGIN_API;
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
        capture_image_t* st = static_cast<capture_image_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << capture_image_version << ", type: " << type);
    
        switch(type)
        {
            case PluginValue::CaptureSurface:
                if(auto res = static_cast<Surface*>(val))
                {
                    if(! st->frames.empty())
                    {
                        res->setSurface(st->frames.front());
                        if(! st->staticImage)
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

bool capture_image_set_value(void* ptr, int type, const void* val)
{
    capture_image_t* st = static_cast<capture_image_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_image_version << ", type: " << type);

    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
