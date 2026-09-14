###v4l2src plugin
#gst-launch-1.0 v4l2src device=/dev/video$1 ! 'video/x-raw, width=(int)3840, height=(int)2160, format=(string)YUY2, framerate=(fraction)30/1' ! xvimagesink -ev

###nvv4l2camerasrc plugin
DISPLAY=:0 gst-launch-1.0 \
nvv4l2camerasrc device=/dev/video$1 ! 'video/x-raw(memory:NVMM), width=(int)3840, height=(int)2160, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink window-x=0 window-y=0 \
nvv4l2camerasrc device=/dev/video$2 ! 'video/x-raw(memory:NVMM), width=(int)3840, height=(int)2160, framerate=(fraction)30/1' ! nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink window-x=960 window-y=0 -ev
