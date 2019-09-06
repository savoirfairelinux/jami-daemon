set BUILD=%SRC%..\build

set HTTP_PARSER_VERSION=2.9.3
set HTTP_PARSER_URL=https://github.com/binarytrails/http_parser/archive/v%HTTP_PARSER_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%HTTP_PARSER_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %HTTP_PARSER_URL%
)

7z -y x v%HTTP_PARSER_VERSION%.tar.gz && 7z -y x v%HTTP_PARSER_VERSION%.tar -o%BUILD%
del v%HTTP_PARSER_VERSION%.tar && del v%HTTP_PARSER_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\http_parser-%HTTP_PARSER_VERSION% http_parser

cd %BUILD%\http_parser

cd %SRC%
