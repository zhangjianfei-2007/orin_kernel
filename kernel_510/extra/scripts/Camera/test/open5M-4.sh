###v4l2src plugin
#gst-launch-1.0 v4l2src device=/dev/video$1 ! 'video/x-raw, width=(int)1920, height=(int)1080, format=(string)YUY2, framerate=(fraction)30/1' ! xvimagesink -ev

###nvv4l2camerasrc plugin
## 1 port
#gst-launch-1.0 nvv4l2camerasrc device=/dev/video$1 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, format=(string)YUY2, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), format=(string)NV12' ! nv3dsink -ev
## 2 port
#gst-launch-1.0 \
#nvv4l2camerasrc device=/dev/video$1 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink  \
#nvv4l2camerasrc device=/dev/video$2 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink -ev

#gst-launch-1.0 \
#nvv4l2camerasrc device=/dev/video$1 ! 'video/x-raw(memory:NVMM), width=(int)3840, height=(int)2160, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink  \
#nvv4l2camerasrc device=/dev/video$2 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink -ev

DISPLAY=:0 gst-launch-1.0 \
v4l2src device=/dev/video$1 ! 'video/x-raw, width=(int)2880, height=(int)1860, format=(string)YUY2, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)1440, height=(int)930, format=(string)NV12' ! nv3dsink \
v4l2src device=/dev/video$2 ! 'video/x-raw, width=(int)2880, height=(int)1860, format=(string)YUY2, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)1440, height=(int)930, format=(string)NV12' ! nv3dsink \
v4l2src device=/dev/video$3 ! 'video/x-raw, width=(int)2880, height=(int)1860, format=(string)YUY2, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)1440, height=(int)930, format=(string)NV12' ! nv3dsink \
v4l2src device=/dev/video$4 ! 'video/x-raw, width=(int)2880, height=(int)1860, format=(string)YUY2, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)1440, height=(int)930, format=(string)NV12' ! nv3dsink -ev
