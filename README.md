# multi-capture
Multi-window system for capturing video from multiple resources

Each video source is described by its own section in the configuration file.  
You can also set a signal plugin and an storage action for each video source.  

# capture plugins
![capture_image](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_image) - read static image from disk  
![capture_script](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_script) - run script, with result static image  
![capture_fireware](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_fireware) - read fireware stream  
![capture_ffmpeg](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_ffmpeg) - read ffmpeg stream  
![capture_flycap](https://github.com/AndreyBarmaley/multi-capture/wiki/Capture-plugins#capture_flycap) - get image from FlyCapture API

# signal plugins
![signal_dbus_event](https://github.com/AndreyBarmaley/multi-capture/wiki/Signal-plugins#signal_dbus_event) - get signal from dbus  
![signal_input_event](https://github.com/AndreyBarmaley/multi-capture/wiki/Signal-plugins#signal_input_event) - get signal from linux /dev/input  

# storage plugins
![storage_file](https://github.com/AndreyBarmaley/multi-capture/wiki/Storage-plugins#storage_file) - save screenshot to file  
![storage_script](https://github.com/AndreyBarmaley/multi-capture/wiki/Storage-plugins#storage_script) - save screenshot with script action  

# example
![screenshot](https://user-images.githubusercontent.com/8620726/149245731-87088444-fe9f-4b65-8409-1d1a9e957501.png)  
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
                "capture":      "endo203cap",
                "storage":      "endo203stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo208",
                "label:color":  "red",
                "position":     [340, 10, 320, 240],
                "capture":      "endo208cap",
                "storage":      "endo208stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo209",
                "label:color":  "red",
                "position":     [670, 10, 320, 240],
                "capture":      "endo209cap",
                "storage":      "endo209stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo210",
                "label:color":  "red",
                "position":     [10, 260, 320, 240],
                "capture":      "endo210cap",
                "storage":      "endo210stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo211",
                "label:color":  "red",
                "position":     [340, 260, 320, 240],
                "capture":      "endo211cap",
                "storage":      "endo211stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo214",
                "label:color":  "red",
                "position":     [670, 260, 320, 240],
                "capture":      "endo214cap",
                "storage":      "endo214stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo215",
                "label:color":  "red",
                "position":     [10, 510, 320, 240],
                "capture":      "endo215cap",
                "storage":      "endo215stor"
            },
            {
                "window:skip":  false,
                "label:name":   "endo152",
                "label:color":  "red",
                "position":     [340, 510, 320, 240],
                "capture":      "endo152cap",
                "storage":      "endo152stor"
            },
            {
                "window:skip":  false,
                "label:name":   "xray152",
                "label:color":  "red",
                "position":     [670, 510, 320, 240],
                "capture":      "xray152cap",
                "storage":      "xray152stor"
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
            "name": "endo203stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo203/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo208stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo208/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo209stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo209/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo210stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo210/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo211stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo211/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo214stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo214/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo215stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo215/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "endo152stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/endo152/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
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
            "name": "xray152stor",
            "type": "storage_file",

            "debug":  false,
            "format:description": "strftime",
            "format": "/var/tmp/xray152/%Y%m%d_%H%M%S.png",
            "signals":  [ "mouse:click" ]
        }
    ]
}
```
