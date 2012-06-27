# disables audio
ffmpeg -f video4linux2 -i /dev/video0 -an -r 30 -vb 10000 -vcodec mpeg4 -f rtp rtp://192.168.40.52:5000
