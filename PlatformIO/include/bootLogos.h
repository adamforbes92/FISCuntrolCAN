#pragma once

#include <Arduino.h>

static const size_t bootLogoWidth      = 64;
static const size_t bootLogoHeight     = 88;
static const size_t bootLogoBufferSize = (bootLogoWidth * bootLogoHeight) / 8;

extern const unsigned char MK4Golf[] PROGMEM;
extern const unsigned char logo[] PROGMEM;
extern const unsigned char finger[] PROGMEM;
extern const unsigned char danger[] PROGMEM;

const uint8_t* getBootLogoBitmap(uint8_t bootScreenId);
bool loadCustomBootLogo();
bool hasCustomBootLogo();
bool saveCustomBootLogoFromBmp(const char* bmpPath);

const uint8_t* getRpmLogoBitmap(uint8_t rpmLogoId);
bool loadCustomRpmLogo();
bool hasCustomRpmLogo();
bool saveCustomRpmLogoFromBmp(const char* bmpPath);
