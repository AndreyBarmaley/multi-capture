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

#include <iomanip>
#include <sstream>
#include <cstring>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

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

class RGBData
{
public:
    int width;
    int height;
    std::unique_ptr<uint8_t[]> rgb;

    RGBData() : width(0), height(0) {}
};

class IEC61883Base
{
public:
    IEC61883Base() {}
    virtual ~IEC61883Base() {}
    virtual bool startReceive(int channel) = 0;
    virtual const RGBData* getRGBData(void) const = 0;
};


class IEC61883DV : public IEC61883Base, protected RGBData
{
    iec61883_dv_fb_t    ptr;
    std::unique_ptr<dv_decoder_t, decltype(dv_decoder_free)*> decoder;

    uint8_t*            pixels[3];
    int                 pitches[3];

public:
    IEC61883DV(raw1394handle_t handle, iec61883_dv_fb_recv_t func)
        : decoder{ nullptr, dv_decoder_free }, pixels{ nullptr, nullptr, nullptr }, pitches{ 0, 0, 0 }
    {
        ptr = iec61883_dv_fb_init(handle, func, this);
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

    const RGBData* getRGBData(void) const override
    {
        return this;
    }

    dv_decoder_t* getDecoder(void)
    {
        return decoder.get();
    }
    void setRGBData(int w, int h, unsigned char* data)
    {
	int len = w * h * 3;

        if(w != width || h != height)
        {
            width = w;
            height = h;
            rgb.reset(new uint8_t[len]);
        }

	pixels[0] = rgb.get();
    	pitches[0] = width * 3;

    	dv_decode_full_frame(decoder.get(), data, e_dv_color_rgb, pixels, pitches);
    }
};

class IEC61883MPEG2 : public IEC61883Base, protected RGBData
{
    iec61883_mpeg2_t ptr;
    std::unique_ptr<mpeg2dec_t, decltype(mpeg2_close)*> decoder;

public:
    IEC61883MPEG2(raw1394handle_t handle, iec61883_mpeg2_recv_t func)
        : decoder{ nullptr, mpeg2_close }
    {
        ptr = iec61883_mpeg2_recv_init(handle, func, this);
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

    const RGBData* getRGBData(void) const override
    {
        return this;
    }

    mpeg2dec_t* getDecoder(void)
    {
        return decoder.get();
    }

    void setRGBData(int w, int h, uint8_t* data)
    {
	int len = w * h * 3;

        if(w != width || h != height)
        {
            width = w;
            height = h;
            rgb.reset(new uint8_t[len]);
        }

	std::memcpy(rgb.get(), data, len);
    }
};

struct capture_fireware_t
{
    bool 	is_debug;
    Surface 	surface;

    std::unique_ptr<raw1394_handle, decltype(raw1394_destroy_handle)*> raw1394;
    struct pollfd	raw1394_poll;

    std::unique_ptr<IEC61883Base> iec61883_ptr;

    uint64_t		guid;
    int			node;
    int			port;
    int			type;
    int			input;
    int			output;
    int			channel;
    int			bandwidth;
    int			receiving;

    capture_fireware_t() : is_debug(false), 
	raw1394{ nullptr, raw1394_destroy_handle },
	guid(0), node(-1), port(-1), type(0), input(-1), output(-1), channel(-1), bandwidth(0), receiving(0)
    {
        raw1394_poll = {0};
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
	rom1394_directory rom_dir;

	int nb_ports = raw1394_get_port_info(raw1394.get(), pinf, 16);
	if(0 > nb_ports)
	{
    	    ERROR("Failed to get number of IEEE1394 ports.");
    	    return false;
	}

	for(int pi = 0; pi < nb_ports; ++pi)
	{
	    raw1394.reset(raw1394_new_handle_on_port(pi));

    	    if(! raw1394)
	    {
        	ERROR("Failed setting IEEE1394 port: " << pi);
        	return false;
    	    }

    	    for(int ni = 0; ni < raw1394_get_nodecount(raw1394.get()); ++ni)
	    {
        	/* Select device explicitly by GUID */
        	if(guid > 1)
		{
            	    if(guid == rom1394_get_guid(raw1394.get(), ni))
		    {
                	node = ni;
                	port = pi;
			return true;
            	    }
        	}
		else
        	/* Select first AV/C tape recorder player node */
        	if(0 <= rom1394_get_directory(raw1394.get(), ni, & rom_dir))
		{
            	    if((ROM1394_NODE_TYPE_AVC == rom1394_get_node_type(& rom_dir) &&
                        avc1394_check_subunit_type(raw1394.get(), ni, AVC1394_SUBUNIT_TYPE_VCR)) ||
                	rom_dir.unit_spec_id == MOTDCT_SPEC_ID)
		    {
                	node = ni;
                	port = pi;
			uint64_t guid = rom1394_get_guid(raw1394.get(), ni);

			VERBOSE("guid: " << String::hex64(guid));
            	    }

            	    rom1394_free_directory(& rom_dir);
		    if(raw1394) return true;
        	}
    	    }
	}

	return false;
    }

    static int mpeg2_callback(unsigned char* data, int length, unsigned int complete, void* ptr)
    {
	IEC61883MPEG2* mpeg2 = static_cast<IEC61883MPEG2*>(ptr);
        mpeg2dec_t* decoder = mpeg2->getDecoder();

	if(data && 0 < length && decoder)
	{
	    DEBUG("packet length: " << length);

	    const mpeg2_info_t* info = mpeg2_info(decoder);
	    mpeg2_state_t state = mpeg2_parse(decoder);

	    switch(state)
	    {
		case STATE_BUFFER:
		    mpeg2_buffer(decoder, data, data + length);
        	    break;

		case STATE_SEQUENCE:
		{
		    int res = mpeg2_convert(decoder, mpeg2convert_rgb24, nullptr);
		    DEBUG("mpeg2_convert result: " << res);
		}
		break;

		case STATE_SLICE:
		case STATE_END:
		case STATE_INVALID_END:
    		    if(info->display_fbuf)
			mpeg2->setRGBData(info->sequence->width, info->sequence->height, info->display_fbuf->buf[0]);
        	    break;

    		default:
        	    break;
    	    }
	}

	return 0;
    }

    static int dv_callback(unsigned char* data, int length, int complete, void* ptr)
    {
	IEC61883DV* dv = static_cast<IEC61883DV*>(ptr);
        dv_decoder_t* decoder = dv->getDecoder();

	if(data && 0 < length && decoder)
	{
    	    int res = dv_parse_header(decoder, data);
    	    dv_parse_packs(decoder, data);

	    DEBUG("packet length: " << length << ", complete: " << complete <<
		    ", frame size: " << decoder->width << "x" << decoder->height << ", dv_parse_header: " << res);

	    if(0 < length)
		dv->setRGBData(decoder->width, decoder->height, data);
	}

	return 0;
    }

    bool init(void)
    {
	if(! device_select())
	{
    	    ERROR("No AV/C devices found.");
	    return false;
	}

	// Provide bus sanity for multiple connections
	iec61883_cmp_normalize_output(raw1394.get(), 0xffc0 | node);

	// Find out if device is DV or HDV
	int response = avc1394_transaction(raw1394.get(), node,
                                       AVC1394_CTYPE_STATUS |
                                       AVC1394_SUBUNIT_TYPE_TAPE_RECORDER |
                                       AVC1394_SUBUNIT_ID_0 |
                                       AVC1394_VCR_COMMAND_OUTPUT_SIGNAL_MODE | 0xFF, 2);

	response = AVC1394_GET_OPERAND0(response);
	type = (response == 0x10 || response == 0x90 || response == 0x1A || response == 0x9A) ?
		IEC61883_HDV : IEC61883_DV;

	// Connect to device, and do initialization
        channel = iec61883_cmp_connect(raw1394.get(), node, & output,
                                       raw1394_get_local_id(raw1394.get()), & input, & bandwidth);
	if(channel < 0)
    	    channel = 63;

	if(type == IEC61883_HDV)
	{
    	    iec61883_ptr.reset(new IEC61883MPEG2(raw1394.get(), mpeg2_callback));
	}
	else
	{
    	    iec61883_ptr.reset(new IEC61883DV(raw1394.get(), dv_callback));
	}

	raw1394_poll.fd = raw1394_get_fd(raw1394.get());
	raw1394_poll.events = POLLIN | POLLERR | POLLHUP | POLLPRI;

	// actually start receiving
        if(! iec61883_ptr->startReceive(channel))
            return false;

    	return true;
    }

    void clear(void)
    {
	if(raw1394)
	{
    	    iec61883_ptr.reset();
	    iec61883_cmp_disconnect(raw1394.get(), node, output,
                            raw1394_get_local_id(raw1394.get()), input, channel, bandwidth);

	    raw1394.reset();
    	    raw1394_poll = {0};
	}

	is_debug = false;
	guid = 0;
	node = -1;
	port = -1;
	type = 0;
	input = -1;
	output = -1;
	channel = -1;
	bandwidth = 0;
	receiving = 0;
    	surface.reset();
    }
};

const char* capture_fireware_get_name(void)
{
    return "capture_fireware";
}

int capture_fireware_get_version(void)
{
    return 20220205;
}


void* capture_fireware_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_fireware_get_version());

    auto ptr = std::make_unique<capture_fireware_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    std::string strguid = config.getString("guid");

    if(strguid.size() && strguid != "auto")
    {
	std::istringstream ss(strguid);
	ss >> std::hex >> ptr->guid;

	if(ss.fail())
	{
            ERROR("invalid dv guid parameter: " << strguid);
	    ptr->guid = 0;
        }
	else
	{
	    DEBUG("params: " << "guid = " << strguid);
	}
    }

    if(! ptr->init())
	return nullptr;

    if(ptr->type == IEC61883_HDV)
    {
	VERBOSE("started mpeg2 mode");
    }
    else
    {
	VERBOSE("started DV mode");
    }

    return ptr.release();
}

void capture_fireware_quit(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());

    delete st;
}

int capture_fireware_frame_action(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());
    int result = 0;

    while((result = poll(& st->raw1394_poll, 1, 200)) < 0)
    {
        if(! (errno == EAGAIN || errno == EINTR))
	{
    	    ERROR("raw1394 poll error occurred");
    	    return -1;
        }
    }

    if(result > 0 && ((st->raw1394_poll.revents & POLLIN) || (st->raw1394_poll.revents & POLLPRI)))
    {
	st->receiving = 1;
        raw1394_loop_iterate(st->raw1394.get());
        
        if(auto info = st->iec61883_ptr->getRGBData())
	{
#if SDL_VERSION_ATLEAST(2,0,5)
    	    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(info->rgb.get(), info->width, info->height,
				24, info->width * 3, SDL_PIXELFORMAT_RGB24);
#else
    	    SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(info->rgb.get(), info->width, info->height,
				24, info->width * 3, Rmask, Gmask, Bmask, Amask);
#endif
            st->surface = Surface::copy(sf);
	}

	return 0;
    }
    else
    if(st->receiving)
    {
        ERROR("no more input data available");
	return -1;
    }

    return -1;
}

const Surface & capture_fireware_get_surface(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());
    return st->surface;
}

#ifdef __cplusplus
}
#endif
