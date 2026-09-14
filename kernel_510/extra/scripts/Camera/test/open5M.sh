###v4l2src plugin
DISPLAY=:0 gst-launch-1.0 v4l2src device=/dev/video$1 ! 'video/x-raw, width=(int)2880, height=(int)1860, format=(string)YUY2, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)1440, height=(int)930, format=(string)NV12' ! nv3dsink -ev

###nvv4l2camerasrc plugin
#gst-launch-1.0 nvv4l2camerasrc device=/dev/video$1 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink -ev
