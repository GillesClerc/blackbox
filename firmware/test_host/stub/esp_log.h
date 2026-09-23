// Stubs minimaux pour compiler les modules purs du firmware sur host.
#pragma once
#include <stdio.h>
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "    [E %s] " fmt "\n", tag, ##__VA_ARGS__)
