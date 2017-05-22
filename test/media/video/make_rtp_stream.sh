# disables audio
ffmpeg -f video4linux2 -i /dev/video0 -srcw 320 -srch 240 -an -r 30 -vprofile baseline -level 13 -vb 400000 -vcodec libx264 -payload_type 109 -preset veryfast -tune zerolatency -f rtp rtp://192.168.50.116:2228
