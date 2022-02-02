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
#include <mutex>
#include <atomic>
#include <algorithm>

#include "../../settings.h"
#include "DeckLinkAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

struct idname_t
{
    uint32_t id = 0;
    const char* name = nullptr;
};

namespace
{
    std::array<idname_t, 11> _formats = { { 
	{ bmdFormat8BitYUV, "8BitYUV" }, { bmdFormat10BitYUV, "10BitYUV" }, { bmdFormat8BitARGB, "8BitARGB" }, { bmdFormat8BitBGRA, "8BitBGRA" },
	{ bmdFormat10BitRGB, "10BitRGB" }, { bmdFormat12BitRGB, "12BitRGB" }, { bmdFormat12BitRGBLE, "12BitRGBLE" }, { bmdFormat10BitRGBXLE, "10BitRGBXLE" },
	{ bmdFormat10BitRGBX, "10BitRGBX" }, { bmdFormatH265, "H265" }, { bmdFormatDNxHR, "DNxHR" }
    } };

    std::array<idname_t, 6> _connections = { { 
	{ bmdVideoConnectionSDI, "SDI" }, { bmdVideoConnectionHDMI, "HDMI" }, { bmdVideoConnectionOpticalSDI, "OpticalSDI" },
	{ bmdVideoConnectionComponent, "Component" }, { bmdVideoConnectionComposite, "Composite" }, { bmdVideoConnectionSVideo, "SVideo" }
    } };

    std::array<idname_t, 110> _modes = { { 
	{ bmdModeNTSC, "NTSC" }, { bmdModeNTSC2398, "NTSC2398" }, { bmdModePAL, "PAL" }, { bmdModeNTSCp, "NTSCp" }, { bmdModePALp, "PALp" },

	{ bmdModeHD1080p2398, "HD1080p2398" }, { bmdModeHD1080p24, "HD1080p24" }, { bmdModeHD1080p25, "HD1080p25" }, { bmdModeHD1080p2997, "HD1080p299" }, { bmdModeHD1080p30, "HD1080p30" },
	{ bmdModeHD1080p4795, "HD1080p4795" }, { bmdModeHD1080p48, "HD1080p48" }, { bmdModeHD1080p50, "HD1080p50" }, { bmdModeHD1080p5994, "HD1080p5994" }, { bmdModeHD1080p6000, "HD1080p6000" },
	{ bmdModeHD1080p9590, "HD1080p9590" }, { bmdModeHD1080p96, "HD1080p96" }, { bmdModeHD1080p100, "HD1080p100" }, { bmdModeHD1080p11988, "HD1080p11988" }, { bmdModeHD1080p120, "HD1080p120" },
	{ bmdModeHD1080i50, "HD1080i50" }, { bmdModeHD1080i5994, "HD1080i5994" }, { bmdModeHD1080i6000, "HD1080i6000" },
	{ bmdModeHD720p50, "HD720p50" }, { bmdModeHD720p5994, "HD720p5994" }, { bmdModeHD720p60, "HD720p60" },

	{ bmdMode2k2398, "2k2398" }, { bmdMode2k24, "2k24" }, { bmdMode2k25, "2k25" }, { bmdMode2kDCI2398, "2kDCI2398" }, { bmdMode2kDCI24, "2kDCI24" }, { bmdMode2kDCI25, "2kDCI25" },
	{ bmdMode2kDCI2997, "2kDCI2997" }, { bmdMode2kDCI30, "2kDCI30" }, { bmdMode2kDCI4795, "2kDCI4795" }, { bmdMode2kDCI48, "2kDCI48" }, { bmdMode2kDCI50, "2kDCI50" }, { bmdMode2kDCI5994, "2kDCI5994" },
	{ bmdMode2kDCI60, "2kDCI60" }, { bmdMode2kDCI9590, "2kDCI9590" }, { bmdMode2kDCI96, "2kDCI96" }, { bmdMode2kDCI100, "2kDCI100" }, { bmdMode2kDCI11988, "2kDCI11988" }, { bmdMode2kDCI120, "2kDCI120" },

	{ bmdMode4K2160p2398, "4K2160p2398" }, { bmdMode4K2160p24, "4K2160p24" }, { bmdMode4K2160p25, "4K2160p25" }, { bmdMode4K2160p2997, "4K2160p2997" }, { bmdMode4K2160p30, "4K2160p30" },
	{ bmdMode4K2160p4795, "4K2160p4795" }, { bmdMode4K2160p48, "4K2160p48" }, { bmdMode4K2160p50, "4K2160p50" }, { bmdMode4K2160p5994, "4K2160p5994" }, { bmdMode4K2160p60, "4K2160p60" },
	{ bmdMode4K2160p9590, "4K2160p9590" }, { bmdMode4K2160p96, "4K2160p96" }, { bmdMode4K2160p100, "4K2160p100" }, { bmdMode4K2160p11988, "4K2160p11988" }, { bmdMode4K2160p120, "4K2160p120" },
	{ bmdMode4kDCI2398, "4kDCI2398" }, { bmdMode4kDCI24, "4kDCI24" }, { bmdMode4kDCI25, "4kDCI25" }, { bmdMode4kDCI2997, "4kDCI2997" }, { bmdMode4kDCI30, "4kDCI30" }, { bmdMode4kDCI4795, "4kDCI4795" },
	{ bmdMode4kDCI48, "4kDCI48" }, { bmdMode4kDCI50, "4kDCI50" }, { bmdMode4kDCI5994, "4kDCI5994" }, { bmdMode4kDCI60, "4kDCI60" }, { bmdMode4kDCI9590, "4kDCI9590" }, { bmdMode4kDCI96, "4kDCI96" },
	{ bmdMode4kDCI100, "4kDCI100" }, { bmdMode4kDCI11988, "4kDCI11988" }, { bmdMode4kDCI120, "4kDCI120" },

	{ bmdMode8K4320p2398, "8K4320p2398" }, { bmdMode8K4320p24, "8K4320p24" }, { bmdMode8K4320p25, "8K4320p25" }, { bmdMode8K4320p2997, "8K4320p2997" }, { bmdMode8K4320p30, "8K4320p30" },
	{ bmdMode8K4320p4795, "8K4320p4795" }, { bmdMode8K4320p48, "8K4320p48" }, { bmdMode8K4320p50, "8K4320p50" }, { bmdMode8K4320p5994, "8K4320p5994" }, { bmdMode8K4320p60, "8K4320p60" },
	{ bmdMode8kDCI2398, "8kDCI2398" }, { bmdMode8kDCI24, "8kDCI24" }, { bmdMode8kDCI25, "8kDCI25" }, { bmdMode8kDCI2997, "8kDCI2997" }, { bmdMode8kDCI30, "8kDCI30" }, { bmdMode8kDCI4795, "8kDCI4795" },
	{ bmdMode8kDCI48, "8kDCI48" }, { bmdMode8kDCI50, "8kDCI50" }, { bmdMode8kDCI5994, "8kDCI5994" }, { bmdMode8kDCI60, "8kDCI60" },

	{ bmdMode640x480p60, "640x480p60" }, { bmdMode800x600p60, "800x600p60" }, { bmdMode1440x900p50, "1440x900p50" }, { bmdMode1440x900p60, "1440x900p60" }, { bmdMode1440x1080p50, "1440x1080p50" },
	{ bmdMode1440x1080p60, "1440x1080p60" }, { bmdMode1600x1200p50, "1600x1200p50" }, { bmdMode1600x1200p60, "1600x1200p60" }, { bmdMode1920x1200p50, "1920x1200p50" }, { bmdMode1920x1200p60, "1920x1200p60" },
	{ bmdMode1920x1440p50, "1920x1440p50" }, { bmdMode1920x1440p60, "1920x1440p60" }, { bmdMode2560x1440p50, "2560x1440p50" }, { bmdMode2560x1440p60, "2560x1440p60" }, { bmdMode2560x1600p50, "2560x1600p50" },
	{ bmdMode2560x1600p60, "2560x1600p60" }
    } };
}

const char* pixelFormat2name(BMDPixelFormat format)
{
    auto it = std::find_if(_formats.begin(), _formats.end(), [=](auto & val){ return val.id == format; });
    return it != _formats.end() ? it->name : "unknown";
}

BMDPixelFormat name2pixelFormat(const std::string & name)
{
    auto it = std::find_if(_formats.begin(), _formats.end(), [lower = String::toLower(name)](auto & val){ return lower == String::toLower(val.name); });
    return it != _formats.end() ? it->id : bmdFormatUnspecified;
}

const char* displayMode2name(BMDDisplayMode mode)
{
    auto it = std::find_if(_modes.begin(), _modes.end(), [=](auto & val){ return val.id == mode; });
    return it != _modes.end() ? it->name : "unknown";
}

BMDDisplayMode name2displayMode(const std::string & name)
{
    auto it = std::find_if(_modes.begin(), _modes.end(), [lower = String::toLower(name)](auto & val){ return lower == String::toLower(val.name); });
    return it != _modes.end() ? it->id : bmdModeUnknown;
}

const char* videoConnection2name(BMDVideoConnection conn)
{
    auto it = std::find_if(_connections.begin(), _connections.end(), [=](auto & val){ return val.id == conn; });
    return it != _connections.end() ? it->name : "unknown";
}

BMDVideoConnection name2videoConnection(const std::string & name)
{
    auto it = std::find_if(_connections.begin(), _connections.end(), [lower = String::toLower(name)](auto & val){ return lower == String::toLower(val.name); });
    return it != _connections.end() ? it->id : bmdVideoConnectionUnspecified;
}

class Bgra32VideoFrame : public IDeckLinkVideoFrame
{
private:
    long        		width;
    long			height;
    BMDFrameFlags		flags;
    std::vector<uint8_t>	pixelBuffer;
    std::atomic<ULONG>		refCount;

public:
    Bgra32VideoFrame(long width, long height, BMDFrameFlags flags) :
        width(width), height(height), flags(flags), refCount(1)
    {
        // Allocate pixel buffer
        pixelBuffer.resize(width*height*4);
    }

    virtual ~Bgra32VideoFrame() {}

    void SwapBuffer(std::vector<uint8_t> & buf) { pixelBuffer.swap(buf); }

    // IDeckLinkVideoFrame interface
    long GetWidth(void) override { return width; };
    long GetHeight(void) override { return height; } ;
    long GetRowBytes(void) override { return width * 4; };

    HRESULT GetBytes(void **buffer) override
    {
    	*buffer = pixelBuffer.data();
    	return S_OK;
    }

    BMDFrameFlags GetFlags(void) override { return flags; };
    BMDPixelFormat GetPixelFormat(void) override { return bmdFormat8BitBGRA; };

    // Dummy implementations of remaining methods in IDeckLinkVideoFrame
    HRESULT GetAncillaryData(IDeckLinkVideoFrameAncillary** ancillary) override { return E_NOTIMPL; };
    HRESULT GetTimecode(BMDTimecodeFormat format, IDeckLinkTimecode** timecode) override { return E_NOTIMPL; };

    // IUnknown interface
    HRESULT QueryInterface(REFIID iid, LPVOID *ppv) override
    {
        if (ppv == NULL)
                return E_INVALIDARG;
 
        // Initialise the return result
        *ppv = NULL;

        // Obtain the IUnknown interface and compare it the provided REFIID
        CFUUIDBytes iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
        if(memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
        {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        else
	if(memcmp(&iid, &IID_IDeckLinkVideoFrame, sizeof(REFIID)) == 0)
        {
            *ppv = (IDeckLinkVideoFrame*)this;
            AddRef();
            return S_OK;
        }
                
        return E_NOINTERFACE;
    }

    ULONG AddRef(void) override
    {
	refCount++;
	return refCount;
    }

    ULONG Release(void) override
    {
        refCount--;
        return refCount;
    }
};

struct DeckLinkDevice : public IDeckLinkInputCallback
{
    std::mutex lockData;
    std::atomic<ULONG> refCount;

    long imageWidth;
    long imageHeight;
    long imageRowBytes;
    BMDPixelFormat imagePixelFormat;
    BMDFrameFlags imageFlags;
    uint8_t* imageData;

    IDeckLink*  deckLink;
    IDeckLinkInput* deckInput;
    IDeckLinkConfiguration* deckConfig;
    IDeckLinkProfileAttributes* deckAttrs;
    IDeckLinkHDMIInputEDID* deckHDMI;
    IDeckLinkStatus* deckStatus;
    IDeckLinkVideoConversion* deckFrameConverter;
    std::unique_ptr<Bgra32VideoFrame> bgra32Frame;

    DeckLinkDevice() : refCount(0), imageWidth(0), imageHeight(0), imageRowBytes(0),
	imagePixelFormat(bmdFormatUnspecified), imageFlags(bmdFrameFlagDefault), imageData(nullptr),
	deckLink(nullptr), deckInput(nullptr), deckConfig(nullptr), deckAttrs(nullptr), deckHDMI(nullptr),
	deckStatus(nullptr), deckFrameConverter(nullptr)
    {
    }

    ~DeckLinkDevice()
    {
	reset();
    }

    void reset(void)
    {
	bgra32Frame.reset();

	if(deckFrameConverter)
	{
	    deckFrameConverter->Release();
	    deckFrameConverter = nullptr;
	}

	if(deckStatus)
	{
	    deckStatus->Release();
	    deckStatus = nullptr;
	}

	if(deckAttrs)
	{
	    deckAttrs->Release();
	    deckAttrs = nullptr;
	}

	if(deckConfig)
	{
	    deckConfig->Release();
	    deckConfig = nullptr;
	}

	if(deckHDMI)
	{
	    deckHDMI->Release();
	    deckHDMI = nullptr;
	}

	if(deckInput)
	{
	    deckInput->Release();
	    deckInput = nullptr;
	}

	if(deckLink)
	{
	    deckLink->Release();
	    deckLink = nullptr;
	}
    }

    bool init(int devIndex, const std::string & connection)
    {
	auto deckLinkIterator = CreateDeckLinkIteratorInstance();
	if(! deckLinkIterator)
	{
    	    ERROR("This application requires the DeckLink drivers installed");
    	    return false;
	}

	while(S_OK == deckLinkIterator->Next(&deckLink))
	{
    	    if(S_OK != deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, reinterpret_cast<void**>(& deckAttrs)))
    	    {
        	deckLink->Release();
		deckLink = nullptr;
        	return false;
    	    }

    	    int64_t intAttribute = 0;
    	    // Skip over devices that don't support capture
    	    if(S_OK == deckAttrs->GetInt(BMDDeckLinkVideoIOSupport, &intAttribute) &&
                (intAttribute & bmdDeviceSupportsCapture))
    	    {
        	if(devIndex <= 0)
		{
		    if(S_OK != deckLink->QueryInterface(IID_IDeckLinkInput, reinterpret_cast<void**>(& deckInput)))
		    {
			ERROR("DeckLinkInput: failed");
			break;
		    }

		    if(S_OK != deckLink->QueryInterface(IID_IDeckLinkConfiguration, reinterpret_cast<void**>(& deckConfig)))
		    {
			ERROR("DeckLinkConfiguration: failed");
			break;
		    }

		    if(S_OK != deckLink->QueryInterface(IID_IDeckLinkStatus, reinterpret_cast<void**>(& deckStatus)))
		    {
			ERROR("DeckLinkStatus: failed");
			break;
		    }

		    // Check whether device supports capture
		    int64_t ioSupportAttribute = 0;
            	    auto ret = deckAttrs->GetInt(BMDDeckLinkVideoIOSupport, & ioSupportAttribute);

            	    if((ret != S_OK) || ((ioSupportAttribute & bmdDeviceSupportsCapture) == 0))
            	    {
                	ERROR("Selected device does not support capture");
			break;
            	    }

/*
		    if(S_OK == deckLink->QueryInterface(IID_IDeckLinkHDMIInputEDID, reinterpret_cast<void**>(& deckHDMI)))
		    {
    			// Enable all EDID functionality if possible
    			int64_t allKnownRanges = bmdDynamicRangeSDR | bmdDynamicRangeHDRStaticPQ | bmdDynamicRangeHDRStaticHLG;
    			deckHDMI->SetInt(bmdDeckLinkHDMIInputEDIDDynamicRange, allKnownRanges);
    			deckHDMI->WriteToEDID();
		    }
		    else
		    {
			ERROR("DeckLinkHDMI: failed");
		    }

*/
		    // set connection
		    if(S_OK != deckConfig->SetInt(bmdDeckLinkConfigVideoInputConnection, name2videoConnection(connection)))
			ERROR("set connection failed");

            	    return true;
		}

        	devIndex--;
    	    }

    	    deckAttrs->Release();
	    deckAttrs = nullptr;

    	    deckLink->Release();
    	    deckLink = nullptr;
	}

	deckLinkIterator->Release();

	ERROR("Unable to get DeckLink device: " << devIndex);
	return false;
    }

    bool startCapture(void)
    {
	if(S_OK != deckInput->SetCallback(this))
	{
    	    ERROR("Failed set callback");
	    return false;
	}

	if(S_OK != deckInput->EnableVideoInput(bmdModeNTSC, bmdFormat8BitBGRA, bmdVideoInputFlagDefault | bmdVideoInputEnableFormatDetection))
	{
    	    ERROR("Failed to switch video mode");
	    return false;
	}

	if(S_OK != deckInput->StartStreams())
	{
    	    ERROR("Failed start streams");
	    return false;
	}

	return true;
    }

    void stopCapture(void)
    {
	const std::lock_guard<std::mutex> lock(lockData);

	deckInput->StopStreams();
	deckInput->SetCallback(nullptr);
	deckInput->DisableVideoInput();
	imageData = nullptr;
    }

    void debugInfo(void) const
    {
	char* deckLinkName = nullptr;
	deckLink->GetDisplayName((const char**) & deckLinkName);

	int64_t currentInputConnection = 0;
	if(S_OK != deckConfig->GetInt(bmdDeckLinkConfigVideoInputConnection, & currentInputConnection))
	    ERROR("get bmdDeckLinkConfigVideoInputConnection failed");

	int64_t currentVideoInputMode = 0;
	if(S_OK != deckStatus->GetInt(bmdDeckLinkStatusCurrentVideoInputMode, & currentVideoInputMode))
	    ERROR("get bmdDeckLinkStatusCurrentVideoInputMode failed");

	int64_t currentVideoInputPixelFormat = 0;
	if(S_OK != deckStatus->GetInt(bmdDeckLinkStatusCurrentVideoInputPixelFormat, & currentVideoInputPixelFormat))
	    ERROR("get bmdDeckLinkStatusCurrentVideoInputPixelFormat failed");

	//int64_t serialNumber = 0;
	//if(deckConfig->GetInt(bmdDeckLinkConfigDeviceInformationSerialNumber, &serialNumber))
	//   ERROR("get bmdDeckLinkConfigDeviceInformationSerialNumber failed");

	// Check if input mode detection is supported.
	bool formatDetectionSupportAttribute = false;
	auto ret = deckAttrs->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &formatDetectionSupportAttribute);
	bool supportsFormatDetection = (ret == S_OK) && formatDetectionSupportAttribute;

	DEBUG("*** DeckLink INFORMATION ***" << 
	    "\nLibrary version: " << BMDDeckLinkAPIVersion <<
	    "\nCapture device: " << deckLinkName <<
	    "\nCurrent connector: " << videoConnection2name(currentInputConnection) <<
	    "\nCurrent input video mode: " << displayMode2name(currentVideoInputMode) <<
	    "\nCurrent input pixel format: " << pixelFormat2name(currentVideoInputPixelFormat) <<
	    "\nFormat detection: " << String::Bool(supportsFormatDetection));

	std::free(deckLinkName);
    }

    // IUnknown interface
    HRESULT QueryInterface(REFIID iid, LPVOID *ppv) override
    {
        CFUUIDBytes iunknown;
        HRESULT result = E_NOINTERFACE;
        
        if(ppv == nullptr)
                return E_INVALIDARG;

        // Initialise the return result
        *ppv = nullptr;

        // Obtain the IUnknown interface and compare it the provided REFIID
        iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
        if(memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
        {
            *ppv = this;
            AddRef();
            result = S_OK;
        }
        else
	if(memcmp(&iid, &IID_IDeckLinkInputCallback, sizeof(REFIID)) == 0)
        {
            *ppv = (IDeckLinkInputCallback*)this;
            AddRef();
            result = S_OK;
        }

        return result;
    }

    ULONG AddRef(void) override
    {
	refCount++;
	return refCount;
    }

    ULONG Release(void) override
    {
	refCount--;
	return refCount;
    }

    // IDeckLinkInputCallback interface
    HRESULT VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents,
				IDeckLinkDisplayMode* newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override
    {
        BMDPixelFormat  pixelFormat;
        
        if(detectedSignalFlags & bmdDetectedVideoInputRGB444)
        {
            if(detectedSignalFlags & bmdDetectedVideoInput8BitDepth)
                pixelFormat = bmdFormat8BitARGB;
            else
	    if(detectedSignalFlags & bmdDetectedVideoInput10BitDepth)
                pixelFormat = bmdFormat10BitRGB;
            else
	    if(detectedSignalFlags & bmdDetectedVideoInput12BitDepth)
                pixelFormat = bmdFormat12BitRGB;
            else
            {
        	ERROR("Invalid color depth for RGB, input format flags: " << String::hex(detectedSignalFlags));
                return E_INVALIDARG;
	    }
        }
        else
	{
            ERROR("Unexpected detected video input format flags: " << String::hex(detectedSignalFlags));
            return E_INVALIDARG;
	}

	bgra32Frame.reset();

	// Restart streams if either display mode or colorspace has changed
        if(notificationEvents & (bmdVideoInputDisplayModeChanged | bmdVideoInputColorspaceChanged))
        {
    	    HRESULT result = S_OK;
            // Stop the capture
            deckInput->StopStreams();

            // Set the detected video input mode
            if(S_OK != (result = deckInput->EnableVideoInput(newDisplayMode->GetDisplayMode(), pixelFormat, bmdVideoInputEnableFormatDetection)))
            {
                ERROR("Unable to re-enable video input on auto-format detection");
                return E_FAIL;
            }

            // Restart the capture
            if(S_OK != (result = deckInput->StartStreams()))
            {
                ERROR("Unable to restart streams on auto-format detection");
                return E_FAIL;
            }

	    DEBUG("Video format changed to mode: " << displayMode2name(newDisplayMode->GetDisplayMode()) << ", format: " << pixelFormat2name(pixelFormat));
	    return S_OK;
        }

	return E_FAIL;
    }

    HRESULT VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override
    {
	if(videoFrame)
	{
	    if(videoFrame->GetFlags() & bmdFrameHasNoInputSource)
	    {
		ERROR("IDeckLinkVideoInputFrame: invalid frame");
                return E_FAIL;
	    }

	    const std::lock_guard<std::mutex> lock(lockData);

	    if(S_OK != videoFrame->GetBytes(reinterpret_cast<void**>(& imageData)))
	    {
		ERROR("IDeckLinkVideoInputFrame::GetBytes failed");
		imageData = nullptr;
                return E_FAIL;
	    }

	    imageWidth = videoFrame->GetWidth();
	    imageHeight = videoFrame->GetHeight();
	    imageRowBytes = videoFrame->GetRowBytes();
	    imagePixelFormat = videoFrame->GetPixelFormat();
	    imageFlags = videoFrame->GetFlags();

    	    if(0)
	    {
		DEBUG("image width: " << imageWidth << ", height: " << imageHeight << ", size: " << imageRowBytes * imageHeight <<
		    ", pixelFormat: " << String::hex(imagePixelFormat) << ", flags: " << String::hex(imageFlags));
	    }

	    if(imagePixelFormat != bmdFormat8BitBGRA)
	    {
    		if(! deckFrameConverter)
		{
		    deckFrameConverter = CreateVideoConversionInstance();
    		    if(! deckFrameConverter)
		    {
			ERROR("CreateVideoConversionInstance: failed");
            		return E_FAIL;
    		    }
		}

		if(! bgra32Frame || bgra32Frame->GetWidth() != videoFrame->GetWidth() || bgra32Frame->GetHeight() != videoFrame->GetHeight())
		{
		    bgra32Frame.reset(new Bgra32VideoFrame(videoFrame->GetWidth(), videoFrame->GetHeight(), videoFrame->GetFlags()));
		}

		if(S_OK != deckFrameConverter->ConvertFrame(videoFrame, bgra32Frame.get()))
		    ERROR("Frame conversion to BGRA was unsuccessful");

		imageWidth = bgra32Frame->GetWidth();
		imageHeight = bgra32Frame->GetHeight();
    		imageRowBytes = bgra32Frame->GetRowBytes();
		imagePixelFormat = bmdFormat8BitBGRA;
		bgra32Frame->GetBytes((void**) & imageData);
	    }
	}

	return S_OK;
    }
};

struct capture_decklink_t
{
    bool 	is_used;
    bool 	is_debug;

    DeckLinkDevice device;
    Surface surface;


    capture_decklink_t() : is_used(false), is_debug(false)
    {
    }

    void clear(void)
    {
	is_used = false;
	is_debug = false;


	surface.reset();
	device.reset();
    }
};

#ifndef CAPTURE_DECKLINK_SPOOL
#define CAPTURE_DECKLINK_SPOOL 4
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

capture_decklink_t capture_decklink_vals[CAPTURE_DECKLINK_SPOOL];

const char* capture_decklink_get_name(void)
{
    return "capture_decklink";
}

int capture_decklink_get_version(void)
{
    return 20211121;
}


bool capture_decklink_set_mode(capture_decklink_t* st, size_t devIndex)
{
    return false;
}


void* capture_decklink_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_decklink_get_version());

    int devindex = 0;
    for(; devindex < CAPTURE_DECKLINK_SPOOL; ++devindex)
        if(! capture_decklink_vals[devindex].is_used) break;

    if(CAPTURE_DECKLINK_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << CAPTURE_DECKLINK_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    capture_decklink_t* st = & capture_decklink_vals[devindex];

    int dev_index = config.getInteger("device", 0);
    st->is_debug = config.getBoolean("debug", false);

    DEBUG("params: " << "device = " << dev_index);

    if(! st->device.init(dev_index, config.getString("connection", "hdmi")))
	return nullptr;

    if(! st->device.startCapture())
    {
	st->clear();
	return nullptr;
    }

    st->device.debugInfo();
    st->is_used = true;

    return st;
}

void capture_decklink_quit(void* ptr)
{
    capture_decklink_t* st = static_cast<capture_decklink_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_decklink_get_version());

    st->device.stopCapture();
    st->clear();
}

int capture_decklink_frame_action(void* ptr)
{
    capture_decklink_t* st = static_cast<capture_decklink_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_decklink_get_version());

    const std::lock_guard<std::mutex> lock(st->device.lockData);

    if(! st->device.imageData)
	return 0;

    if(st->device.imagePixelFormat != bmdFormat8BitBGRA)
    {
	ERROR("incorrect format");
	return 0;
    }

    if(st->is_debug)
    {
        DEBUG("image info -  width: " << st->device.imageWidth << ", height: " << st->device.imageHeight <<
		    ", stride: " << st->device.imageRowBytes << ", size: " << st->device.imageRowBytes * st->device.imageHeight);
    }

#if SDL_VERSION_ATLEAST(2,0,5)
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(st->device.imageData,
                            st->device.imageWidth, st->device.imageHeight,
                            32, st->device.imageRowBytes, SDL_PIXELFORMAT_BGRA32);
#else
    SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(st->device.imageData,
                            st->device.imageWidth, st->device.imageHeight,
                            32, st->device.imageRowBytes, Bmask, Gmask, Rmask, Amask);
#endif

    st->surface = Surface::copy(sf);
    return 0;
}

const Surface & capture_decklink_get_surface(void* ptr)
{
    capture_decklink_t* st = static_cast<capture_decklink_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << capture_decklink_get_version());
    return st->surface;
}

#ifdef __cplusplus
}
#endif
