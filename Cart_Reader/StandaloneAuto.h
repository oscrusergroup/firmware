// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
void standaloneBootCapture();
void standaloneBootReady();
void standaloneError();
void standaloneVerified();
void standaloneFileClosed(uint32_t size);
bool standaloneWait();
bool standaloneCandidate(uint8_t target);
void standaloneCandidateAllowed(bool allowed);
int8_t standaloneMenu(const __FlashStringHelper* title, char answers[7][20], uint8_t count);
uint8_t standaloneTarget(const __FlashStringHelper* title, const char* answer, bool resolving);
bool standaloneHold(uint8_t target, bool& presetChanged, bool allowAuto);
bool standaloneInputBlocked();
void standaloneSuppressNextBoot();
bool standaloneRightDown();
void standaloneTitle(const __FlashStringHelper* title);
bool standaloneFileTracking();
bool standaloneHeaderSelectable(const __FlashStringHelper* title);
void standaloneDisablePreset();
