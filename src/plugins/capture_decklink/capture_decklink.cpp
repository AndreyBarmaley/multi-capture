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
#include <chrono>
#include <algorithm>

#include "../../settings.h"
#include "DeckLinkAPIDispatch.cpp"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_decklink_version = 20220415;

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

    std::array<idname_t, 4> _duplex = { { 
	{ bmdDuplexFull, "full" }, { bmdDuplexHalf, "half" }, { bmdDuplexSimplex, "simplex" }, { bmdDuplexInactive, "inactive" }
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

const char* duplexMode2name(BMDDuplexMode mode)
{
    auto it = std::find_if(_duplex.begin(), _duplex.end(), [=](auto & val){ return val.id == mode; });
    return it != _duplex.end() ? it->name : "unknown";
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
        if(! ppv)
            return E_INVALIDARG;
 
        // Initialise the return result
        *ppv = nullptr;

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
    std::atomic<ULONG> refCount;

    IDeckLink* deckLink;
    IDeckLinkInput* deckInput;
    IDeckLinkConfiguration* deckConfig;
    IDeckLinkProfileAttributes* deckAttrs;
    IDeckLinkHDMIInputEDID* deckHDMI;
    IDeckLinkStatus* deckStatus;
    IDeckLinkVideoConversion* deckFrameConverter;
    std::unique_ptr<Bgra32VideoFrame> bgra32Frame;

    DeckLinkDevice() : refCount(0), 
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

    		    // Get the supported input connections for the device
    		    int64_t supportedInputConnections = 0;
    		    if(deckAttrs->GetInt(BMDDeckLinkVideoInputConnections, & supportedInputConnections) != S_OK)
    		    {
        		ERROR("Supported input connections failed");
			break;
    		    }

		    auto setConnection = name2videoConnection(connection);
		    if(supportedInputConnections & setConnection)
		    {
			// set connection
			if(S_OK != deckConfig->SetInt(bmdDeckLinkConfigVideoInputConnection, setConnection))
			    ERROR("set connection failed: " << connection);
		    }
		    else
		    {
			ERROR("connection not supported: " << connection << ", all supported: " << String::hex(supportedInputConnections));
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

		    if(S_OK != deckLink->QueryInterface(IID_IDeckLinkStatus, reinterpret_cast<void**>(& deckStatus)))
		    {
			ERROR("DeckLinkStatus: failed");
			break;
		    }

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

    void dumpDisplayModes(void)
    {
	// dump display modes
    	IDeckLinkDisplayModeIterator* displayModeIterator = nullptr;
    	if(deckInput->GetDisplayModeIterator(& displayModeIterator) == S_OK)
	{
	    IDeckLinkDisplayMode* displayMode = nullptr;
    	    while(displayModeIterator->Next(& displayMode) == S_OK)
    	    {
		char* modeName = nullptr;
            	//BMDDisplayMode mode = displayMode->GetDisplayMode();
            	if(displayMode->GetName((const char**) & modeName) == S_OK)
		{
		    VERBOSE("display mode: " << modeName);
		    free(modeName);
		}
            	displayMode->Release();
	    }
	    displayModeIterator->Release();
	}
    }

    bool startCapture(BMDDisplayMode displayMode, bool formatDetection)
    {
	if(S_OK != deckInput->SetCallback(this))
	{
    	    ERROR("Failed set callback");
	    return false;
	}

        BMDVideoInputFlags inputFlags = bmdVideoInputFlagDefault;
        if(formatDetection)
            inputFlags |= bmdVideoInputEnableFormatDetection;

	if(S_OK != deckInput->EnableVideoInput(displayMode, bmdFormat8BitYUV, inputFlags))
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
	deckInput->StopStreams();
	deckInput->SetCallback(nullptr);
	deckInput->DisableVideoInput();
    }

    void deviceInfo(void) const
    {
	char* deckLinkName = nullptr;
	deckLink->GetDisplayName((const char**) & deckLinkName);

        int64_t duplexMode = 0;
        if(S_OK != deckAttrs->GetInt(BMDDeckLinkDuplex, & duplexMode))
	    ERROR("get BMDDeckLinkDuplex failed");

        if(duplexMode == bmdDuplexInactive)
	{
	    ERROR("device inactive");
	}

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
	    "\nDeckLink duplex: " << duplexMode2name(duplexMode) <<
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
        BMDPixelFormat pixelFormat = bmdFormat10BitRGB;
        
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
	if(detectedSignalFlags & bmdDetectedVideoInputYCbCr422)
        {
            if(detectedSignalFlags & bmdDetectedVideoInput8BitDepth)
                pixelFormat = bmdFormat8BitYUV;
            else
	    if(detectedSignalFlags & bmdDetectedVideoInput10BitDepth)
                pixelFormat = bmdFormat10BitYUV;
            else
            {
        	ERROR("Invalid color depth for YUV, input format flags: " << String::hex(detectedSignalFlags));
                return E_INVALIDARG;
	    }
        }
        else
	{
            ERROR("Unexpected detected video input format flags: " << String::hex(detectedSignalFlags));
            return E_FAIL;
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
};

struct capture_decklink_t : DeckLinkDevice
{
    int 	        debug;
    int			noSourceErr;
    size_t              framesPerSec;
    size_t              duration;

    std::chrono::time_point<std::chrono::steady_clock> point;

    Frames             frames;

    capture_decklink_t() : debug(0), noSourceErr(0), framesPerSec(25), duration(0)
    {
        duration = 1000 / framesPerSec;
        point = std::chrono::steady_clock::now();
    }

    ~capture_decklink_t()
    {
	clear();
    }

    void clear(void)
    {
        stopCapture();

	debug = 0;
	noSourceErr = 0;
        framesPerSec = 25;
        duration = 1000 / framesPerSec;
        frames.clear();
    }

    HRESULT VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override
    {
	if(! videoFrame)
            return E_FAIL;

	if(videoFrame->GetFlags() & bmdFrameHasNoInputSource)
	{
	    if(noSourceErr < 7)
	    {
		ERROR("IDeckLinkVideoInputFrame: no input source, flags: " << String::hex(videoFrame->GetFlags()));
	        noSourceErr++;
	    }
            return S_FALSE;
	}

        auto now = std::chrono::steady_clock::now();

        // skip frame
        if(5 < frames.size())
        {
            point = now;
            return S_OK;
        }

        // fastest: skip frame
        if(now - point < std::chrono::milliseconds(duration))
        {
            point = now;
            return S_OK;
        }

        uint8_t* imageData = nullptr;
        auto imageWidth = videoFrame->GetWidth();
        auto imageHeight = videoFrame->GetHeight();
        auto imageRowBytes = videoFrame->GetRowBytes();

        // dump video frame
    	if(4 < debug)
	{
	    VERBOSE("image width: " << imageWidth << ", height: " << imageHeight <<
                    ", data size: " << imageRowBytes * imageHeight <<
		    ", pixelFormat: " << String::hex(videoFrame->GetPixelFormat()) << ", flags: " << String::hex(videoFrame->GetFlags()));
	}

	if(videoFrame->GetPixelFormat() != bmdFormat8BitBGRA)
	{
    	    if(! deckFrameConverter)
	    {
		deckFrameConverter = CreateVideoConversionInstance();
    		if(! deckFrameConverter)
		{
		    ERROR("CreateVideoConversionInstance: failed");
                    DisplayScene::pushEvent(nullptr, ActionCaptureReset, this);
            	    return E_FAIL;
    		}
	    }

	    if(! bgra32Frame || bgra32Frame->GetWidth() != imageWidth || bgra32Frame->GetHeight() != imageHeight)
	    {
		bgra32Frame.reset(new Bgra32VideoFrame(imageWidth, imageHeight, videoFrame->GetFlags()));
	    }

	    if(S_OK != deckFrameConverter->ConvertFrame(videoFrame, bgra32Frame.get()))
		    ERROR("Frame conversion to BGRA was unsuccessful");

	    if(S_OK == bgra32Frame->GetBytes((void**) & imageData))
	    {
                imageWidth = bgra32Frame->GetWidth();
                imageHeight = bgra32Frame->GetHeight();
                imageRowBytes = bgra32Frame->GetRowBytes();
	    }
            else
            {
	        ERROR("IDeckLinkVideoInputFrame::GetBytes failed");
	        imageData = nullptr;
            }
	}
        else
        {
	    if(S_OK != videoFrame->GetBytes(reinterpret_cast<void**>(& imageData)))
	    {
	        ERROR("IDeckLinkVideoInputFrame::GetBytes failed");
	        imageData = nullptr;
	    }
        }

        if(! imageData)
        {
            DisplayScene::pushEvent(nullptr, ActionCaptureReset, this);
            return E_FAIL;
        }

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        // SDL_PIXELFORMAT_ARGB8888
        uint32_t amask = 0xFF000000;
        uint32_t rmask = 0x00FF0000;
        uint32_t gmask = 0x0000FF00;
        uint32_t bmask = 0x000000FF;
#else
        // SDL_PIXELFORMAT_BGRA8888
        uint32_t bmask = 0xFF000000;
        uint32_t gmask = 0x00FF0000;
        uint32_t rmask = 0x0000FF00;
        uint32_t amask = 0x000000FF;
#endif

        SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(imageData, imageWidth, imageHeight,
                            32, imageRowBytes, rmask, gmask, bmask, amask);

        frames.push_back(Surface::copy(sf));
        DisplayScene::pushEvent(nullptr, ActionFrameComplete, this);
        point = now;
	noSourceErr = 0;

	return S_OK;
    }
};

void* capture_decklink_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_decklink_version);

    auto ptr = std::make_unique<capture_decklink_t>();

    int dev_index = config.getInteger("device", 0);
    std::string connector = config.getString("connection", "hdmi");
    auto displayMode = name2displayMode(config.getString("display:mode", "ntsc"));
    auto formatDetection = config.getBoolean("format:detection", true);

    ptr->debug = config.getInteger("debug", 0);
    ptr->framesPerSec = config.getInteger("frames:sec", 25);
    if(0 == ptr->framesPerSec || ptr->framesPerSec > 1000)
    {
        ERROR("frames:sec param incorrect, set default");
        ptr->framesPerSec = 25;
    }

    DEBUG("params: " << "frames:sec = " << ptr->framesPerSec);
    DEBUG("params: " << "device = " << dev_index);
    DEBUG("params: " << "connection = " << connector);
    DEBUG("params: " << "display:mode = " << displayMode2name(displayMode));
    DEBUG("params: " << "format:detection = " << String::Bool(formatDetection));

    if(! ptr->init(dev_index, connector))
	return nullptr;

    ptr->deviceInfo();
    if(ptr->debug) ptr->dumpDisplayModes();

    if(! ptr->startCapture(displayMode, formatDetection))
	return nullptr;

    return ptr.release();
}

void capture_decklink_quit(void* ptr)
{
    capture_decklink_t* st = static_cast<capture_decklink_t*>(ptr);
    if(st->debug) DEBUG("version: " << capture_decklink_version);

    st->clear();

    delete st;
}

bool capture_decklink_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_decklink");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_decklink_version;
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
        capture_decklink_t* st = static_cast<capture_decklink_t*>(ptr);

        if(4 < st->debug)
            DEBUG("version: " << capture_decklink_version << ", type: " << type);    

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

bool capture_decklink_set_value(void* ptr, int type, const void* val)
{
    capture_decklink_t* st = static_cast<capture_decklink_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_decklink_version << ", type: " << type);
    
    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
