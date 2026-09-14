#! /bin/bash 

DISPLAY=:0 
gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video1 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video2 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video4 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video5 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video6 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video8 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video9 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 v4l2src device=/dev/video10 ! 'video/x-raw, width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 -ev &
sleep 0.6
gst-launch-1.0 nvv4l2camerasrc device=/dev/video3 ! 'video/x-raw(memory:NVMM), width=(int)3840, height=(int)2160, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink -ev &
sleep 0.6
gst-launch-1.0 nvv4l2camerasrc device=/dev/video7 ! 'video/x-raw(memory:NVMM), width=(int)3840, height=(int)2160, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink -ev &
sleep 0.6
gst-launch-1.0 nvv4l2camerasrc device=/dev/video11 ! 'video/x-raw(memory:NVMM), width=(int)3840, height=(int)2160, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12' ! nv3dsink -ev &
