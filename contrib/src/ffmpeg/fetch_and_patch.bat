set BUILD=%SRC%..\build

set FFMPEG_URL=git://github.com/Microsoft/FFmpegInterop.git

mkdir %BUILD%
cd %BUILD%

git clone --recursive %FFMPEG_URL%

cd FFmpegInterop
git apply --reject --whitespace=fix %SRC%\ffmpeg\ffmpeg-uwp.patch

cd ffmpeg
git apply --reject --whitespace=fix %SRC%\ffmpeg\0004-avformat-fix-find_stream_info-not-considering-extradata.patch

cd %SRC%