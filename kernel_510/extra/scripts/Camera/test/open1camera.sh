#gst-launch-1.0 nvarguscamerasrc sensor-id=$1  ! 'video/x-raw(memory:NVMM), width=1920, height=1080, format=(string)NV12, framerate=(fraction)30/1' ! nv3dsink -e
#gst-launch-1.0 nvarguscamerasrc sensor-id=$1  ! 'video/x-raw(memory:NVMM), width=1920, height=1080, format=(string)NV12, framerate=(fraction)30/1' ! nvoverlaysink -e
#DISPLAY=:0 gst-launch-1.0 nvarguscamerasrc sensor-id=$1 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, format=(string)NV12, framerate=(fraction)30/1' ! nvvidconv flip-method=0 ! 'video/x-raw(memory:NVMM), width=(int)960, height=(int)540, format=(string)NV12, framerate=(fraction)30/1' ! nv3dsink -e
DISPLAY=:0 gst-launch-1.0 nvarguscamerasrc sensor-id=$1 ! 'video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, format=(string)NV12, framerate=(fraction)30/1' ! nvvidconv flip-method=0 ! 'video/x-raw, width=(int)960, height=(int)540, format=(string)I420' ! xvimagesink -e

