# fly-capture
multi-window system for capturing video from multiple resources

Each video source is described by its own section in the configuration file.  
You can also set a signal plugin and an storage action for each video source.  

# capture plugins
capture_image - read static image from disk  
capture_script - run script, with result static image  
capture_fireware - read fireware stream  
capture_ffmpeg - read ffmpeg stream  

# signal plugins
signal_dbus_signal - get signal from dbus  
signal_input_event - get signal from linux /dev/input  

# storage plugins
storage_file - save screenshot to file  
storage_script - save screenshot with script action  
