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
#include "C/FlyCapture2_C.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_flycap_version = 20220412;

struct capture_flycap_t
{
    int 	debug;
    size_t	cameraIndex;
    fc2Context	context;
    fc2PGRGuid  guid;
    fc2Image	imageRaw;
    fc2Image	imageBGR;

    size_t              framesPerSec;
    std::thread         thread;
    std::atomic<bool>   shutdown;
    Frames              frames;
 
    capture_flycap_t() : debug(0), cameraIndex(0), context(nullptr), framesPerSec(25), shutdown(false)
    {
	fc2CreateImage(& imageRaw);
	fc2CreateImage(& imageBGR);
    }

    ~capture_flycap_t()
    {
	clear();
    }

    void clear(void)
    {
        shutdown = true;
        if(thread.joinable())
            thread.join();

        if(context)
	    fc2StopCapture(context);

	debug = 0;
	cameraIndex = 0;

        framesPerSec = 25;
        shutdown = false;
        frames.clear();

        fc2DestroyImage(& imageRaw);
        fc2DestroyImage(& imageBGR);

	if(context)
	{
    	    fc2Disconnect(context);
	    fc2DestroyContext(context);
	    context = nullptr;
	}
    }

    bool init(void)
    {
        fc2Context ctx;
        fc2Error error = fc2CreateContext(& ctx);

        if(error != FC2_ERROR_OK)
        {
	    ERROR("Error in fc2CreateContext: " << error);
	    return false;
        }

        error = fc2RescanBus(ctx);
        if(error != FC2_ERROR_OK)
	    ERROR("Error in fc2RescanBus: " << error);

        unsigned int numCameras = 0;
        error = fc2GetNumOfCameras(ctx, & numCameras);

        if(error == FC2_ERROR_OK)
        {
    	    if(numCameras <= cameraIndex)
    	    {
                ERROR("No cameras detected, index: " << cameraIndex << ", found: " << numCameras);
    	    }
	    else
	    {
	        fc2PGRGuid guid2;
    	        error = fc2GetCameraFromIndex(ctx, cameraIndex, & guid2);

    	        if(error == FC2_ERROR_OK)
	        {
    		    error = fc2Connect(ctx, & guid2);

    		    if(error == FC2_ERROR_OK)
		    {
		        // 24 bit RGB
		        // fc2SetDefaultOutputFormat(FC2_PIXEL_FORMAT_RGB8);

		        context = ctx;
		        guid = guid2;
		        return true;
		    }
		    else
    		    {
            	        ERROR("Error in fc2Connect: " << error);
    		    }
	        }
	        else
    	        {
            	    ERROR("Error in fc2GetCameraFromIndex: " << error);
                }
	    }
        }
        else
        {
	    ERROR("Error in fc2GetNumOfCameras: " << error);
        }

        fc2DestroyContext(context);
        return false;
    }

    void cameraInfo(void)
    {
        fc2Version version;
        fc2GetLibraryVersion(& version);

        DEBUG("FlyCapture2 library version: " << version.major << "." << version.minor << "." << version.type << "." << version.build);
        DEBUG("Application build date: " << __DATE__ << " " << __TIME__);

        fc2Error error;
        fc2CameraInfo camInfo;
        error = fc2GetCameraInfo(context, & camInfo);

        if(error == FC2_ERROR_OK)
        {
            DEBUG("*** CAMERA INFORMATION ***" << 
                "\nSerial number - " << camInfo.serialNumber <<
                "\nCamera model - " << camInfo.modelName << 
                "\nCamera vendor - " << camInfo.vendorName <<
                "\nSensor - " << camInfo.sensorInfo <<
                "\nResolution - " << camInfo.sensorResolution <<
                "\nFirmware version - " << camInfo.firmwareVersion <<
                "\nFirmware build time - "  << camInfo.firmwareBuildTime << "\n");
        }
    }

    void setTimeStamping(bool enableTimeStamp)
    {
        fc2Error error;
        fc2EmbeddedImageInfo embeddedInfo;

        error = fc2GetEmbeddedImageInfo(context, & embeddedInfo);
        if(error == FC2_ERROR_OK)
        {
	    if(embeddedInfo.timestamp.available != 0)
	    {
                embeddedInfo.timestamp.onOff = enableTimeStamp;
	    }

            error = fc2SetEmbeddedImageInfo(context, & embeddedInfo);
            if( error != FC2_ERROR_OK )
            {
                ERROR( "Error in fc2SetEmbeddedImageInfo: " << error);
            }
        }
        else
        {
            ERROR("Error in fc2GetEmbeddedImageInfo: " << error);
        }
    }

    bool frameToSurface(Surface & result)
    {
        fc2Error error = fc2RetrieveBuffer(context, & imageRaw);
        if(error != FC2_ERROR_OK)
        {
            ERROR("Error in retrieveBuffer: " << error);
	    return false;
        }

        if(4 < debug)
        {
            VERBOSE("image info -  width: " << imageRaw.cols << ", height: " << imageRaw.rows <<
		    ", stride: " << imageRaw.stride << ", format: " << String::hex(imageRaw.format) << ", size: " << imageRaw.dataSize);
        }

        error = fc2ConvertImageTo(FC2_PIXEL_FORMAT_BGRU, & imageRaw, & imageBGR);
        if(error != FC2_ERROR_OK)
        {
            ERROR("Error in convertImageTo: " << error);
	    return false;
        }

//#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
//#endif

        // SDL_PIXELFORMAT_BGRX8888
        int bpp = 32; uint32_t bmask = 0xFF000000; uint32_t gmask = 0x00FF0000; uint32_t rmask = 0x0000FF00; uint32_t amask = 0;

        SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(imageBGR.pData,
                            imageBGR.cols, imageBGR.rows, bpp, imageBGR.stride, rmask, gmask, bmask, amask);

        result = Surface::copy(sf);
        return true;
    }

    void start(void)
    {
        setTimeStamping(true);
        fc2StartCapture(context);

        shutdown = false;
        // frames loop
        thread = std::thread([st = this]
        {
            Surface frame;
            size_t delay = 10;
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

                if(st->frameToSurface(frame))
                {
                    auto now = std::chrono::steady_clock::now();
                    auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - point);

                    if(4 < st->debug)
                    {
                        DEBUG("real frame time: " << timeMS.count() << "ms");
                    }

                    if(timeMS > std::chrono::milliseconds(duration))
                    {
                        delay--;
                    }
                    else
                    if(timeMS < std::chrono::milliseconds(duration))
                    {
                        delay++;
                    }
                    
                    st->frames.push_back(frame);
                    DisplayScene::pushEvent(nullptr, ActionFrameComplete, st);
                    point = now;
                }
                else
                {
                    DisplayScene::pushEvent(nullptr, ActionCaptureReset, st);
                    st->shutdown = true;
                }

                if(delay)
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        });
    }
};


void* capture_flycap_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_flycap_version);

    auto ptr = std::make_unique<capture_flycap_t>();

    ptr->cameraIndex = config.getInteger("device", 0);
    ptr->debug = config.getInteger("debug", 0);
    ptr->framesPerSec = config.getInteger("frames:sec", 25);
    if(0 == ptr->framesPerSec || ptr->framesPerSec > 1000)
    {
        ERROR("frames:sec param incorrect, set default");
        ptr->framesPerSec = 25;
    }

    DEBUG("params: " << "device = " << ptr->cameraIndex);
    DEBUG("params: " << "frames:sec = " << ptr->framesPerSec);

    if(! ptr->init())
	return nullptr;

    ptr->cameraInfo();
    ptr->start();

    return ptr.release();
}

void capture_flycap_quit(void* ptr)
{
    capture_flycap_t* st = static_cast<capture_flycap_t*>(ptr);
    if(st->debug) DEBUG("version: " << capture_flycap_version);

    delete st;
}

bool capture_flycap_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_flycap");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_flycap_version;
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
        capture_flycap_t* st = static_cast<capture_flycap_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << capture_flycap_version << ", type: " << type);
    
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

bool capture_flycap_set_value(void* ptr, int type, const void* val)
{
    capture_flycap_t* st = static_cast<capture_flycap_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_flycap_version << ", type: " << type);

    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
