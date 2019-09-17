{
    "name": "restinio",
    "version": "5.3.0",
    "url": "https://github.com/fmtlib/fmt/archive/__VERSION__.tar.gz",
    "deps": [],
    "patches": [],
    "win-patches": [],
    "project-paths": ["msvc/fmt.vcxproj"],
    "with_env" : "false",
    "custom-scripts": {
        "pre-build": [
            "mkdir msvc & cd msvc & cmake .. -G \"Visual Studio 15 2017 Win64\" -DBUILD_SHARED_LIBS=Off -DFMT_USE_USER_DEFINED_LITERALS=0"
        ],
        "build": [],
        "post-build": []
    }
}