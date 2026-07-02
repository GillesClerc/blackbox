# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Version semver rapportée au cloud (GET /api/box/sync?firmware_version=…),
# comparée côté serveur à firmware_releases pour proposer une OTA.
set(PROJECT_VER "0.1.0")
project(blackbox)
