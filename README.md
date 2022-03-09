# multi-capture
Multi-window system for capturing video from multiple resources

Each video source is described by its own section in the configuration file.  
You can also set a signal plugin and an storage action for each video source.  

# capture plugins
![capture_image](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_image) - read static/dynamic image from disk  
![capture_script](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_script) - run script, with result static image  
![capture_fireware](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_fireware) - read fireware stream  
![capture_ffmpeg](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_ffmpeg) - read ffmpeg stream  
![capture_flycap](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_flycap) - get image from FlyCapture API  
![capture_decklink](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_decklink) - get image from BlackMagic API

# signal plugins
![signal_dbus_event](https://github.com/AndreyBarmaley/multi-capture/wiki/Signal-plugins#signal_dbus_event) - get signal from dbus  
![signal_input_event](https://github.com/AndreyBarmaley/multi-capture/wiki/Signal-plugins#signal_input_event) - get signal from linux /dev/input  

# storage plugins
![storage_file](https://github.com/AndreyBarmaley/multi-capture/wiki/Storage-plugins#storage_file) - save screenshot to file  
![storage_script](https://github.com/AndreyBarmaley/multi-capture/wiki/Storage-plugins#storage_script) - save screenshot with script action  

# example for 9 sources
![screenshot](https://user-images.githubusercontent.com/8620726/150244073-7deda86c-92e9-4b02-9644-215706135363.png)  

config file
```
{
    "display:fullscreen": false,
    "display:geometry": [ 1000, 760 ],
    "font:file":        "terminus.ttf",
    "font:size":        "16",
    "font:blend":       "false",
    "windows":
        [
            {
                "window:skip":  false,
                "label:name":   "endo203",
                "label:color":  "red",
                "position":     [10, 10, 320, 240],
                "plugins":      [ "endo203cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo208",
                "label:color":  "red",
                "position":     [340, 10, 320, 240],
                "plugins":      [ "endo208cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo209",
                "label:color":  "red",
                "position":     [670, 10, 320, 240],
                "plugins":      [ "endo209cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo210",
                "label:color":  "red",
                "position":     [10, 260, 320, 240],
                "plugins":      [ "endo210cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo211",
                "label:color":  "red",
                "position":     [340, 260, 320, 240],
                "plugins":      [ "endo211cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo214",
                "label:color":  "red",
                "position":     [670, 260, 320, 240],
                "plugins":      [ "endo214cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo215",
                "label:color":  "red",
                "position":     [10, 510, 320, 240],
                "plugins":      [ "endo215cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "endo152",
                "label:color":  "red",
                "position":     [340, 510, 320, 240],
                "plugins":      [ "endo152cap", "label2stor" ]
            },
            {
                "window:skip":  false,
                "label:name":   "xray152",
                "label:color":  "red",
                "position":     [670, 510, 320, 240],
                "plugins":      [ "xray152cap", "label2stor" ]
            }
        ],

    "plugins": [
        {
            "name": "endo203cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo203/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo208cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo208/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo209cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo209/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo210cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo210/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo211cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo211/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo214cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo214/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo215cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo215/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "endo152cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://endo152/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "xray152cap",
            "type": "capture_ffmpeg",

            "debug":    false,
            "tick":     50,
            "init:timeout":  5000,
            "device":   "http://xray152/streams/stream0-640x360-600kbps-main.ts",
            "scale":    true
        },
        {
            "name": "label2stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/${label}/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
        }
    ]
}
```
