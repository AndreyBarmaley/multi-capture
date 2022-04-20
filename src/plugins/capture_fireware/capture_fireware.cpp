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

#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_fireware_version = 20220415;

#include <sys/poll.h>

#include "libraw1394/raw1394.h"
#include "libavc1394/avc1394.h"
#include "libavc1394/rom1394.h"
#include "libiec61883/iec61883.h"

#include "libdv/dv.h"
#include "libdv/dv_types.h"

#include "mpeg2dec/mpeg2.h"
#include "mpeg2dec/mpeg2convert.h"


#define MOTDCT_SPEC_ID      0x00005068
#define IEC61883_DV         1
#define IEC61883_HDV        2

class IEC61883Base
{
public:
    IEC61883Base() {}
    virtual ~IEC61883Base() {}
    virtual bool startReceive(int channel) = 0;
};

struct capture_fireware_t;

class IEC61883DV : public IEC61883Base
{
    iec61883_dv_fb_t    ptr;
    std::unique_ptr<dv_decoder_t, decltype(dv_decoder_free)*> decoder;

    uint8_t*            pixels[3];
    int                 pitches[3];

public:
    IEC61883DV(raw1394handle_t handle, iec61883_dv_fb_recv_t func, struct capture_fireware_t* st)
        : decoder{ nullptr, dv_decoder_free }, pixels{ nullptr, nullptr, nullptr }, pitches{ 0, 0, 0 }
    {
        ptr = iec61883_dv_fb_init(handle, func, st);
    	decoder.reset(dv_decoder_new(0, 0, 0));

	if(decoder)
            decoder->quality = DV_QUALITY_BEST;
    	else
        {
	    ERROR("DV decoder error");
    	}
    }

    ~IEC61883DV()
    {
        if(ptr)
        {
            iec61883_dv_fb_stop(ptr);
            iec61883_dv_fb_close(ptr);
        }
    }

    bool startReceive(int channel) override
    {
        return 0 == iec61883_dv_fb_start(ptr, channel);
    }

    dv_decoder_t* getDecoder(void)
    {
        return decoder.get();
    }
};

class IEC61883MPEG2 : public IEC61883Base
{
    iec61883_mpeg2_t ptr;
    std::unique_ptr<mpeg2dec_t, decltype(mpeg2_close)*> decoder;

public:
    IEC61883MPEG2(raw1394handle_t handle, iec61883_mpeg2_recv_t func, struct capture_fireware_t* st)
        : decoder{ nullptr, mpeg2_close }
    {
        ptr = iec61883_mpeg2_recv_init(handle, func, st);
	decoder.reset(mpeg2_init());

	if(! decoder)
	    ERROR("mpeg decoder error");
    }

    ~IEC61883MPEG2()
    {
        if(ptr)
        {
	    iec61883_mpeg2_recv_stop(ptr);
    	    iec61883_mpeg2_close(ptr);
        }
    }

    bool startReceive(int channel) override
    {
    	return 0 == iec61883_mpeg2_recv_start(ptr, channel);
    }

    mpeg2dec_t* getDecoder(void)
    {
        return decoder.get();
    }
};

struct capture_fireware_t
{
    int 	debug;

    std::unique_ptr<raw1394_handle, decltype(raw1394_destroy_handle)*> raw1394;
    std::unique_ptr<IEC61883Base> iec61883;

    uint64_t		devGuid;
    int			devNode;
    int			devPort;
    int			devInput;
    int			devOutput;
    int			devChannel;
    int			devBandwidth;

    std::thread         thread;

    size_t              framesPerSec;
    std::atomic<bool>   shutdown;

    size_t              duration;
    std::chrono::time_point<std::chrono::steady_clock> point;
    Frames              frames;

    capture_fireware_t() : debug(0), raw1394{ nullptr, raw1394_destroy_handle },
	devGuid(0), devNode(-1), devPort(-1), devInput(-1), devOutput(-1), devChannel(-1), devBandwidth(0),
        framesPerSec(25), shutdown(false)
    {
        duration = 1000 / framesPerSec;
        point = std::chrono::steady_clock::now();
    }

    ~capture_fireware_t()
    {
	clear();
    }

    bool device_select(void)
    {
	raw1394.reset(raw1394_new_handle());

	if(! raw1394)
	{
    	    ERROR("Failed to open IEEE1394 interface.");
    	    return false;
	}

	struct raw1394_portinfo pinf[16];
	rom1394_directory romDir;

	int nb_ports = raw1394_get_port_info(raw1394.get(), pinf, 16);
	if(0 > nb_ports)
	{
    	    ERROR("Failed to get number of IEEE1394 ports.");
    	    return false;
	}

	for(int portIter = 0; portIter < nb_ports; ++portIter)
	{
	    raw1394.reset(raw1394_new_handle_on_port(portIter));

    	    if(! raw1394)
	    {
        	ERROR("Failed setting IEEE1394 port: " << portIter);
        	return false;
    	    }

    	    for(int nodeIter = 0; nodeIter < raw1394_get_nodecount(raw1394.get()); ++nodeIter)
	    {
        	/* Select device explicitly by GUID */
        	if(devGuid > 1)
		{
            	    if(devGuid != rom1394_get_guid(raw1394.get(), nodeIter))
			continue;

            	    devNode = nodeIter;
            	    devPort = portIter;
		    return true;
        	}
		else
        	/* Select first AV/C tape recorder player node */
        	if(0 <= rom1394_get_directory(raw1394.get(), nodeIter, & romDir))
		{
            	    if((ROM1394_NODE_TYPE_AVC == rom1394_get_node_type(& romDir) &&
                        avc1394_check_subunit_type(raw1394.get(), nodeIter, AVC1394_SUBUNIT_TYPE_VCR)) ||
                	romDir.unit_spec_id == MOTDCT_SPEC_ID)
		    {
                	devNode = nodeIter;
                	devPort = portIter;
			devGuid = rom1394_get_guid(raw1394.get(), nodeIter);

			VERBOSE("selected guid: " << String::hex64(devGuid));
            	    }

            	    rom1394_free_directory(& romDir);
		    if(raw1394) return true;
        	}
    	    }
	}

	return false;
    }

    static int mpeg2_callback(unsigned char* data, int length, unsigned int complete, void* ptr)
    {
        capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
	IEC61883MPEG2* mpeg2 = st ? static_cast<IEC61883MPEG2*>(st->iec61883.get()) : nullptr;
        mpeg2dec_t* decoder = mpeg2 ? mpeg2->getDecoder() : nullptr;

	if(data && 0 < length && decoder)
	{
            auto now = std::chrono::steady_clock::now();

	    const mpeg2_info_t* info = mpeg2_info(decoder);
            if(! info)
            {
                ERROR("mpeg_info failed");
                return -1;
            }

	    mpeg2_state_t state = mpeg2_parse(decoder);
	    bool setrgb = false;

	    switch(state)
	    {
		case STATE_BUFFER:
		{
		    mpeg2_buffer(decoder, data, data + length);
		    if(3 < st->debug) 
			DEBUG("state buffer, length: " << length);
        	}
		break;

		case STATE_SEQUENCE:
		{
		    int res = mpeg2_convert(decoder, mpeg2convert_rgb24, nullptr);
		    if(3 < st->debug) 
			DEBUG("state sequence, length: " << length << ", mpeg2_convert result: " << res);
		}
		break;

		case STATE_SLICE:
		    if(3 < st->debug) 
			DEBUG("state slice, length: " << length);
		    setrgb = true;
        	    break;

		case STATE_END:
		    if(3 < st->debug) 
			DEBUG("state end, length: " << length);
		    setrgb = true;
        	    break;

		case STATE_INVALID_END:
		    if(3 < st->debug) 
			DEBUG("state invalid end, length: " << length);
		    setrgb = true;
        	    break;

    		default:
        	    break;
    	    }

            // skip frame
            if(5 < st->frames.size())
            {
                st->point = now;
                return 0;
            }

            // fastest: skip frame
            if(now - st->point < std::chrono::milliseconds(st->duration))
            {
                st->point = now;
                return 0;
            }

    	    if(setrgb && info->sequence && info->display_fbuf)
            {
                if(4 < st->debug)
                {
                    VERBOSE("image width: " << info->sequence->width << ", height: " << info->sequence->height <<
                        ", data size: " << info->sequence->width * 3 << ", pixelFormat: " << "RGB24");
                }

                // SDL_PIXELFORMAT_RGB24
                uint32_t rmask = 0x00FF0000; uint32_t gmask = 0x0000FF00; uint32_t bmask = 0x000000FF;
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
                std::swap(rmask, bmask);
#endif
 
    	        SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(info->display_fbuf->buf[0], info->sequence->width, info->sequence->height,
				24, info->sequence->width * 3, rmask, gmask, bmask, 0);

                st->frames.push_back(Surface::copy(sf));
                DisplayScene::pushEvent(nullptr, ActionFrameComplete, st);
                st->point = now;
            }
	}

	return 0;
    }

    static int dv_callback(unsigned char* data, int length, int complete, void* ptr)
    {
        capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
	IEC61883DV* dv = st ? static_cast<IEC61883DV*>(st->iec61883.get()) : nullptr;
        dv_decoder_t* decoder = dv ? dv->getDecoder() : nullptr;

	if(data && 0 < length && decoder)
	{
            auto now = std::chrono::steady_clock::now();

    	    int res = dv_parse_header(decoder, data);
            if(res < 0)
            {
                ERROR("dv_parse_header failed, error: " << res);
                return -1;
            }

    	    dv_parse_packs(decoder, data);

            // skip frame
            if(5 < st->frames.size())
            {
                st->point = now;
                return 0;
            }

            // fastest: skip frame
            if(now - st->point < std::chrono::milliseconds(st->duration))
            {
                st->point = now;
                return 0;
            }

	    if(complete)
            {

                uint8_t* pixels[3] = {nullptr};
                int pitches[3] = {0};
                std::vector<uint8_t> buf(decoder->width * decoder->height * 3);

                pixels[0] = buf.data();
    	        pitches[0] = decoder->width * 3;

    	        dv_decode_full_frame(decoder, data, e_dv_color_rgb, pixels, pitches);

                // dump video frame
                if(4 < st->debug)
                {
                    VERBOSE("image width: " << decoder->width << ", height: " << decoder->height <<
                        ", data size: " << decoder->width * 3 << ", pixelFormat: " << "RGB24");
                }

                // SDL_PIXELFORMAT_RGB24
                uint32_t rmask = 0x00FF0000; uint32_t gmask = 0x0000FF00; uint32_t bmask = 0x000000FF;
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
                std::swap(rmask, bmask);
#endif
    	        SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(pixels[0], decoder->width, decoder->height,
				24, decoder->width * 3, rmask, gmask, bmask, 0);

                st->frames.push_back(Surface::copy(sf));
                DisplayScene::pushEvent(nullptr, ActionFrameComplete, st);
                st->point = now;
            }
	}

	return 0;
    }

    bool init(void)
    {
	if(! device_select())
	{
    	    ERROR("No AV/C devices found");
	    return false;
	}

	// Provide bus sanity for multiple connections
	iec61883_cmp_normalize_output(raw1394.get(), 0xffc0 | devNode);

	// Find out if device is DV or HDV
	int response = avc1394_transaction(raw1394.get(), devNode,
                                       AVC1394_CTYPE_STATUS |
                                       AVC1394_SUBUNIT_TYPE_TAPE_RECORDER |
                                       AVC1394_SUBUNIT_ID_0 |
                                       AVC1394_VCR_COMMAND_OUTPUT_SIGNAL_MODE | 0xFF, 2);

	response = AVC1394_GET_OPERAND0(response);
	int type = (response == 0x10 || response == 0x90 || response == 0x1A || response == 0x9A) ?
		IEC61883_HDV : IEC61883_DV;

	// Connect to device, and do initialization
        devChannel = iec61883_cmp_connect(raw1394.get(), devNode, & devOutput,
                                       raw1394_get_local_id(raw1394.get()), & devInput, & devBandwidth);
	if(devChannel < 0)
    	    devChannel = 63;

	if(type == IEC61883_HDV)
	{
    	    iec61883.reset(new IEC61883MPEG2(raw1394.get(), mpeg2_callback, this));
	    if(debug)
                DEBUG("started mpeg2 mode");
	}
	else
	{
    	    iec61883.reset(new IEC61883DV(raw1394.get(), dv_callback, this));
	    if(debug)
                DEBUG("started DV mode");
	}

	// actually start receiving
        if(! iec61883->startReceive(devChannel))
            return false;

    	return true;
    }

    void clear(void)
    {
        shutdown = true;
        if(thread.joinable())
            thread.join();

        framesPerSec = 25;

	if(raw1394)
	{
    	    iec61883.reset();
	    iec61883_cmp_disconnect(raw1394.get(), devNode, devOutput,
                            raw1394_get_local_id(raw1394.get()), devInput, devChannel, devBandwidth);

	    raw1394.reset();
	}

	debug = 0;
	devGuid = 0;
	devNode = -1;
	devPort = -1;
	devInput = -1;
	devOutput = -1;
	devChannel = -1;
	devBandwidth = 0;
    }

    void start(void)
    {
        shutdown = false;

	//raw1394_busreset_notify(st->raw1394.get(), 1);

        // frames loop
        thread = std::thread([st = this]
        {

            while(! st->shutdown)
            {
                int ret = 0;
		struct pollfd rawPoll{0};
		rawPoll.fd = raw1394_get_fd(st->raw1394.get());
		rawPoll.events = POLLIN | POLLERR | POLLNVAL | POLLHUP | POLLPRI;

                ret = poll(& rawPoll, 1, 10);

                if(0 > ret)
	        {
		    if(errno == EAGAIN || errno == EINTR)
			continue;

    	            ERROR("raw1394 poll error occurred");
                    DisplayScene::pushEvent(nullptr, ActionCaptureReset, st);
                    st->shutdown = true;
                    break;
                }
                else
		if(0 == ret)
		{
                    // if no dv_callback more 3 sec
                    auto now = std::chrono::steady_clock::now();
		    auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - st->point);

                    if(timeMS.count() > 3000)
		    {
			ERROR("raw1394 timeout exception");
            	        DisplayScene::pushEvent(nullptr, ActionCaptureReset, st);
            		st->point = now;
                	st->shutdown = true;
                	break;
		    }
		}

                if(0 < ret && ((rawPoll.revents & POLLIN) || (rawPoll.revents & POLLPRI)))
                {
                    ret = raw1394_loop_iterate(st->raw1394.get());
                    if(ret != 0)
                    {
                        ERROR("raw1394_loop_iterate failed, error: " << ret);
                        DisplayScene::pushEvent(nullptr, ActionCaptureReset, st);
                        st->shutdown = true;
                        break;
                    }
                }
            }
        });
    }
};

void* capture_fireware_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_fireware_version);

    auto ptr = std::make_unique<capture_fireware_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->framesPerSec = config.getInteger("frames:sec", 25);
    if(0 == ptr->framesPerSec || ptr->framesPerSec > 1000)
    {
        ERROR("frames:sec param incorrect, set default");
        ptr->framesPerSec = 25;
    }

    std::string strGuid = config.getString("guid");

    DEBUG("params: " << "frames:sec = " << ptr->framesPerSec);

    if(strGuid.size() && strGuid != "auto")
    {
	std::istringstream ss(strGuid);
	ss >> std::hex >> ptr->devGuid;

	if(ss.fail())
	{
            ERROR("invalid dv guid parameter: " << strGuid);
	    ptr->devGuid = 0;
        }
	else
	{
	    DEBUG("params: " << "guid = " << strGuid);
	}
    }

    if(! ptr->init())
	return nullptr;


    ptr->start();

    return ptr.release();
}

void capture_fireware_quit(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(st->debug) DEBUG("version: " << capture_fireware_version);

    delete st;
}

bool capture_fireware_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_fireware");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_fireware_version;
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
        capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << capture_fireware_version << ", type: " << type);
    
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

bool capture_fireware_set_value(void* ptr, int type, const void* val)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_fireware_version << ", type: " << type);

    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
