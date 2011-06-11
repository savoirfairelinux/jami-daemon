# disables audio
ffmpeg -f video4linux2 -i /dev/video0 -an -r 30 -vcodec libvpx -f rtp rtp://127.0.0.1:5000/ -vb 10000
