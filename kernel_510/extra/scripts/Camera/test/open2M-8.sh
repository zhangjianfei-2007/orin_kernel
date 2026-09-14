#! /bin/bash

export DISPLAY=:0

###nvv4l2camerasrc plugin

gst-launch-1.0 nvv4l2camerasrc device=/dev/video0 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=0 \
nvv4l2camerasrc device=/dev/video1 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=480 window-y=0 \
nvv4l2camerasrc device=/dev/video2 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=960 window-y=0 \
nvv4l2camerasrc device=/dev/video3 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=1440 window-y=0 \
nvv4l2camerasrc device=/dev/video4 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=0 window-y=270 \
nvv4l2camerasrc device=/dev/video5 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=480 window-y=270 \
nvv4l2camerasrc device=/dev/video6 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=960 window-y=270 \
nvv4l2camerasrc device=/dev/video7 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1' ! \
nvvidconv ! 'video/x-raw(memory:NVMM), width=(int)480, height=(int)270, format=(string)NV12, framerate=(fraction)30/1' ! \
nv3dsink window-x=1440 window-y=270 -ev
