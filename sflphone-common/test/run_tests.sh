#!/bin/bash
CODECS_PATH="../src/audio/codecs" FAKE_PLUGIN_DIR="../src/plug-in/test/" FAKE_PLUGIN_NAME="../src/plug-in/test/libplugintest.so" ./test --xml || exit 1
