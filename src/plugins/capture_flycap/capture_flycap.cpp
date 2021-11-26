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

#include "../../settings.h"
#include "C/FlyCapture2_C.h"

#ifdef __cplusplus
extern "C" {
#endif

struct capture_flycap_t
{
    bool 	is_used;
    bool 	is_debug;
    size_t	cam_index;
    fc2Context	context;
    fc2PGRGuid  guid;
    fc2Image	raw_image1;
    fc2Image	raw_image2;
    Surface 	surface;
 
    capture_flycap_t() : is_used(false), is_debug(false), cam_index(0), context(NULL)
    {
	fc2CreateImage(& raw_image1);
	fc2CreateImage(& raw_image2);
    }

    void clear(void)
    {
	is_used = false;
	is_debug = false;
	cam_index = 0;
	surface.reset();

        fc2DestroyImage(& raw_image1);
        fc2DestroyImage(& raw_image2);

	if(context)
	{
    	    fc2Disconnect(context);
	    fc2DestroyContext(context);
	    context = NULL;
	}
    }
};

#ifndef CAPTURE_FLYCAP_SPOOL
#define CAPTURE_FLYCAP_SPOOL 4
#endif

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

capture_flycap_t capture_flycap_vals[CAPTURE_FLYCAP_SPOOL];

void capture_flycap_build_info(void)
{
    fc2Version version;
    fc2GetLibraryVersion(& version);
                
    DEBUG("MultiCapture2 library version: " << version.major << "." << version.minor << "." << version.type << "." << version.build);
    DEBUG("Application build date: " << __DATE__ << " " << __TIME__);
}

void capture_flycap_camera_info(fc2Context context)
{
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

void capture_flycap_set_time_stamping(fc2Context context, bool enableTimeStamp)
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

const char* capture_flycap_get_name(void)
{
    return "capture_flycap";
}

int capture_flycap_get_version(void)
{
    return 20211121;
}

bool capture_flycap_init_cam(capture_flycap_t & st)
{
    fc2Context context;
    fc2Error error = fc2CreateContext(& context);

    if(error != FC2_ERROR_OK)
    {
	ERROR("Error in fc2CreateContext: " << error);
	return false;
    }

    error = fc2RescanBus(context);
    if(error != FC2_ERROR_OK)
	ERROR("Error in fc2RescanBus: " << error);

    unsigned int numCameras = 0;
    error = fc2GetNumOfCameras(context, & numCameras);

    if(error == FC2_ERROR_OK)
    {
    	if(numCameras <= st.cam_index)
    	{
            ERROR("No cameras detected, index: " << st.cam_index << ", found: " << numCameras);
    	}
	else
	{
	    fc2PGRGuid guid;
    	    error = fc2GetCameraFromIndex(context, st.cam_index, & guid);

    	    if(error == FC2_ERROR_OK)
	    {
    		error = fc2Connect(context, & guid);

    		if(error == FC2_ERROR_OK)
		{
		    // 24 bit RGB
		    // fc2SetDefaultOutputFormat(FC2_PIXEL_FORMAT_RGB8);

		    st.context = context;
		    st.guid = guid;
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

void* capture_flycap_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_flycap_get_version());

    int devindex = 0;
    for(; devindex < CAPTURE_FLYCAP_SPOOL; ++devindex)
        if(! capture_flycap_vals[devindex].is_used) break;

    if(CAPTURE_FLYCAP_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << CAPTURE_FLYCAP_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    capture_flycap_t* st = & capture_flycap_vals[devindex];

    st->cam_index = config.getInteger("device", 0);
    st->is_debug = config.getBoolean("debug", false);

    DEBUG("params: " << "device = " << st->cam_index);

    capture_flycap_build_info();

    if(! capture_flycap_init_cam(*st))
	return NULL;

    capture_flycap_camera_info(st->context);
    capture_flycap_set_time_stamping(st->context, true);

    fc2StartCapture(st->context);
    st->is_used = true;

    return st;
}

void capture_flycap_quit(void* ptr)
{
    capture_flycap_t* st = static_cast<capture_flycap_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_flycap_get_version());

    if(st->context)
	fc2StopCapture(st->context);

    st->clear();
}

int capture_flycap_frame_action(void* ptr)
{
    capture_flycap_t* st = static_cast<capture_flycap_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_flycap_get_version());

    fc2Error error = fc2RetrieveBuffer(st->context, & st->raw_image1);
    if(error != FC2_ERROR_OK)
    {
        ERROR("Error in retrieveBuffer: " << error);
	return -1;
    }

    if(st->is_debug)
    {
        DEBUG("image info -  width: " << st->raw_image1.cols << ", height: " << st->raw_image1.rows <<
		    ", stride: " << st->raw_image1.stride << ", format: " << String::hex(st->raw_image1.format) << ", size: " << st->raw_image1.dataSize);
    }

    error = fc2ConvertImageTo(FC2_PIXEL_FORMAT_BGR, & st->raw_image1, & st->raw_image2);
    if(error != FC2_ERROR_OK)
    {
        ERROR("Error in convertImageTo: " << error);
	return -1;
    }

#if SDL_VERSION_ATLEAST(2,0,5)
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(st->raw_image2.pData,
                            st->raw_image2.cols, st->raw_image2.rows,
                            24, st->raw_image2.stride, SDL_PIXELFORMAT_BGR24);
#else
    SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(st->raw_image2.pData,
                            st->raw_image2.cols, st->raw_image2.rows,
                            24, st->raw_image2.stride, Bmask, Gmask, Rmask, Amask);
#endif

    st->surface = Surface::copy(sf);
    return 0;
}

const Surface & capture_flycap_get_surface(void* ptr)
{
    capture_flycap_t* st = static_cast<capture_flycap_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << capture_flycap_get_version());
    return st->surface;
}

#ifdef __cplusplus
}
#endif
