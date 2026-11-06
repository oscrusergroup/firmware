// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone automation uses existing read-only menu entries and verification.
// No heap allocation, recursive menu traversal or extra display buffers.
static uint8_t standalonePreset = 0;
static uint8_t standaloneRun = 0;
static uint8_t standaloneStage = 0; // 0 idle, 1 entry, 2 ROM, 3 save, 4 save menu
static uint8_t standaloneFlags = 0;
static bool standaloneSkip = false;
static bool standaloneStartupHeld = false;
static uint32_t standaloneResetMarker __attribute__((section(".noinit")));
static const uint32_t SA_RESET_MARKER = 0xB005D105UL;
static bool standaloneNesCandidate = false;
static const uint8_t SA_FILE = 1, SA_VERIFIED = 2;
static const int SA_EEPROM = E2END - 3;

bool standaloneRightDown() {
#if defined(ENABLE_BUTTON2) || defined(ENABLE_ROTARY)
  return !(PING & _BV(2));
#else
  return false;
#endif
}
static bool standaloneIs(const __FlashStringHelper* value, const __FlashStringHelper* expected) {
  const char* a = reinterpret_cast<const char*>(value);
  const char* b = reinterpret_cast<const char*>(expected);
  while (true) { char x=pgm_read_byte(a++), y=pgm_read_byte(b++); if(x!=y) return false; if(!x) return true; }
}
static const __FlashStringHelper* standaloneName(uint8_t target) {
  switch(target) {
    case 1: return F("Game Boy (Color)");
    case 2: return F("GB Advance (3V)");
    case 3: return F("NES/Famicom");
    case 4: return F("Super Nintendo/SFC");
    case 5: return F("Nintendo 64 (3V)");
    case 6: return F("Mega Drive/Genesis");
    case 7: return F("PCE swapped");
    case 8: return F("PCE not swapped");
    default: return F("OFF");
  }
}
static void standaloneRelease() {
  while(standaloneRightDown()) { delay(5); }
  delay(25);
#ifdef ENABLE_BUTTON2
  buttonVal2 = buttonLast2 = HIGH;
  DCwaiting2=DConUp2=false; ignoreUp2=true;
#endif
#ifdef ENABLE_ROTARY
  buttonState=lastButtonState=HIGH;
#endif
}
void standaloneBootCapture() {
  standaloneStartupHeld=standaloneRightDown();
  standaloneSkip=standaloneStartupHeld || standaloneResetMarker==SA_RESET_MARKER;
  standaloneResetMarker=0;
  uint8_t target=EEPROM.read(SA_EEPROM+1);
  if(EEPROM.read(SA_EEPROM)==0xA7 && EEPROM.read(SA_EEPROM+3)==1
      && target<=8 && EEPROM.read(SA_EEPROM+2)==uint8_t(target^0x5A)) standalonePreset=target;
}
void standaloneBootReady() {
#if (defined(ENABLE_LCD) || defined(ENABLE_OLED)) && (defined(ENABLE_BUTTON2) || defined(ENABLE_ROTARY))
  if(!standaloneSkip && standalonePreset) {
    standaloneRun=standalonePreset; standaloneStage=1; standaloneFlags=0;
  }
#endif
}
static void standaloneSetPreset(uint8_t target) {
  if(target==standalonePreset) target=0;
  // Invalidate before writing, commit the schema marker last. Interrupted
  // writes disable the setting rather than selecting an unintended console.
  EEPROM.update(SA_EEPROM,0);
  EEPROM.update(SA_EEPROM+1,target);
  EEPROM.update(SA_EEPROM+2,uint8_t(target^0x5A));
  EEPROM.update(SA_EEPROM+3,1);
  EEPROM.update(SA_EEPROM,0xA7);
  standalonePreset=target;
}
void standaloneError() {
  if(standaloneStage) {
    standaloneStage=0; standaloneRun=0; standaloneFlags=0;
    println_Msg(F("Auto stopped"));
  }
}
void standaloneVerified() { if(standaloneStage==2) standaloneFlags|=SA_VERIFIED; }
void standaloneFileClosed(uint32_t size) {
  if(standaloneStage==2 || standaloneStage==3) {
    if(size) standaloneFlags|=SA_FILE; else standaloneError();
  }
}
void standaloneCandidateAllowed(bool allowed) { standaloneNesCandidate=allowed; }
bool standaloneCandidate(uint8_t target) {
  return standaloneStage==1 && standaloneRun==target
      && (target!=3 || standaloneNesCandidate);
}
static int8_t standaloneHasSave() {
  switch(standaloneRun) {
#ifdef ENABLE_GBX
    case 1: return lastByte>0;
    case 2: return saveType>=1 && saveType<=6 ? 1 : (saveType==0 ? 0 : -1);
#endif
#ifdef ENABLE_NES
    case 3: return ram>0;
#endif
#ifdef ENABLE_SNES
    case 4: return sramSize>0;
#endif
#ifdef ENABLE_N64
    case 5: return saveType==0 ? 0 : (saveType==1 || saveType==2 || saveType==4 || saveType==5 || saveType==6 ? 1 : -1);
#endif
#ifdef ENABLE_MD
    case 6: return saveType==0 ? 0 : (saveType<=4 ? 1 : -1);
#endif
    case 7: case 8: return 0;
  }
  return -1;
}
static void standaloneComplete() {
  standaloneStage=0; standaloneRun=0; standaloneFlags=0;
  println_Msg(F("Auto complete")); rgbLed(green_color); display_Update();
}
bool standaloneWait() {
  if(!standaloneStage) return false;
  if(errorLvl) { standaloneError(); return false; }
  if(standaloneStage==1) return true;
  if(standaloneStage==2) {
    if((standaloneFlags&(SA_FILE|SA_VERIFIED))!=(SA_FILE|SA_VERIFIED)) {
      standaloneError(); return false;
    }
    int8_t save=standaloneHasSave();
    if(save==1) { standaloneStage=4; return true; }
    if(save==0) standaloneComplete(); else standaloneError();
  } else if(standaloneStage==3) {
    if(standaloneFlags&SA_FILE) standaloneComplete(); else standaloneError();
  }
  return false; // Keep the final result until the user acknowledges it.
}
uint8_t standaloneTarget(const __FlashStringHelper* title, const char* answer, bool resolving) {
  if(standaloneIs(title,F("OPEN SOURCE CART READER"))) {
    if(!strcmp_P(answer,PSTR("Game Boy"))) return resolving && standalonePreset<=2 ? standalonePreset : 0;
    if(!strcmp_P(answer,PSTR("NES/Famicom"))) return 3;
    if(!strcmp_P(answer,PSTR("Super Nintendo/SFC"))) return 4;
    if(!strcmp_P(answer,PSTR("Nintendo 64 (3V)"))) return 5;
    if(!strcmp_P(answer,PSTR("Mega Drive/Genesis"))) return 6;
    if(!strcmp_P(answer,PSTR("PC Engine/TG16"))) return resolving && standalonePreset>=7 ? standalonePreset : 0;
  }
  if(standaloneIs(title,F("Select Game Boy"))) {
    if(!strcmp_P(answer,PSTR("Game Boy (Color)"))) return 1;
    if(!strcmp_P(answer,PSTR("GB Advance (3V)"))) return 2;
  }
  if(standaloneIs(title,F("Select device"))) {
    if(!strcmp_P(answer,PSTR("HuCARD(swapped)"))) return 7;
    if(!strcmp_P(answer,PSTR("HuCARD(not swapped)"))) return 8;
  }
  return 0;
}
static bool standaloneReadMenu(const __FlashStringHelper* title) {
  switch(standaloneRun) {
    case 1:return standaloneIs(title,F("GB Cart Reader"));
    case 2:return standaloneIs(title,F("GBA Cart Reader"));
    case 3:return standaloneIs(title,F("NES CART READER"));
    case 4:return standaloneIs(title,F("SNES Cart Reader"));
    case 5:return standaloneIs(title,F("N64 Cart Reader"));
    case 6:return standaloneIs(title,F("MEGA DRIVE Reader"));
    case 7:case 8:return standaloneIs(title,F("PCE HuCARD menu"));
  }
  return false;
}
int8_t standaloneMenu(const __FlashStringHelper* title, char answers[7][20], uint8_t count) {
  if(!standaloneStage) return -1;
  if(standaloneReadMenu(title)) {
    const char* wanted=standaloneStage==4 ? PSTR("Read Save") : (standaloneRun==3 ? PSTR("Read iNES Rom") : PSTR("Read ROM"));
    if(standaloneStage!=1 && standaloneStage!=4) { standaloneError(); return -1; }
    for(uint8_t i=0;i<count;i++) if(!strcmp_P(answers[i],wanted)) {
      standaloneStage=standaloneStage==4 ? 3 : 2; standaloneFlags=0; return i;
    }
    standaloneError();return -1;
  }
  if(standaloneStage!=1) { standaloneError();return -1; }
  for(uint8_t i=0;i<count;i++) {
    uint8_t target=standaloneTarget(title,answers[i],false);
    if(standaloneIs(title,F("OPEN SOURCE CART READER"))) {
      if((standaloneRun<=2 && !strcmp_P(answers[i],PSTR("Game Boy")))
         || (standaloneRun>=7 && !strcmp_P(answers[i],PSTR("PC Engine/TG16")))
         || target==standaloneRun) return i;
    } else if(target==standaloneRun) return i;
    if((standaloneRun==6 && standaloneIs(title,F("Select MD device")))
        || (standaloneRun==5 && standaloneIs(title,F("Select N64 device")))) {
      if(!strcmp_P(answers[i],PSTR("Game Cartridge"))) return i;
    }
    if(standaloneRun==4 && standaloneIs(title,FS(FSTRING_SELECT_CART_TYPE))
        && !strcmp_P(answers[i],PSTR("SNES/SFC cartridge"))) return i;
  }
  if(standaloneIs(title,F("OPEN SOURCE CART READER")) && currPage<numPages) {
    lastPage=currPage;currPage++;return -2;
  }
  standaloneError();return -1;
}
// A previous startup hold must never turn into an ordinary click or Auto.
// This is polled after drawing the menu, rather than blocking setup().
bool standaloneInputBlocked() {
  if(!standaloneStartupHeld) return false;
  if(standaloneRightDown()) return true;
  standaloneStartupHeld=false;
  standaloneRelease();
  return true; // Discard the release edge as well.
}
void standaloneSuppressNextBoot() { standaloneResetMarker=SA_RESET_MARKER; }
static const __FlashStringHelper* standaloneDisplayName(uint8_t target) {
  switch(target) {
    case 4: return F("SNES/SFC");
    case 5: return F("N64 (3V)");
    case 6: return F("MD/Genesis");
    default: return standaloneName(target);
  }
}
bool standaloneHold(uint8_t target, bool& presetChanged, bool allowAuto) {
  presetChanged=false;
  if(!target || !standaloneRightDown()) return false;
  unsigned long pressed=millis(); uint8_t shown=255;
  while(standaloneRightDown()) {
    uint32_t elapsed=millis()-pressed;
    uint8_t state=elapsed<1000 ? 0 : (elapsed<3000 ? 1 : (2+((elapsed/500)&1)));
#if defined(ENABLE_LCD) || defined(ENABLE_OLED)
    if(state!=shown && elapsed>=1000) {
      display.setDrawColor(0);display.drawBox(0,0,128,9);display.setDrawColor(1);
      display.setCursor(0,8);
      if(state==1 && allowAuto) {
        display.print(F("Auto: ")); display.print(standaloneDisplayName(target));
      } else if(state==1 || state==2) {
        display.print(F("BootDump: "));
        display.print(target==standalonePreset ? F("OFF") : standaloneDisplayName(target));
      }
      display.updateDisplay();
    }
#endif
    shown=state;delay(5);
  }
  uint32_t elapsed=millis()-pressed;standaloneRelease();
  if(elapsed>=3000) { standaloneSetPreset(target);presetChanged=true;return false; }
  if(elapsed>=1000 && !allowAuto) return false;
  if(elapsed>=1000) {
    standaloneRun=target;standaloneStage=1;standaloneFlags=0;standaloneNesCandidate=false;
    errorLvl=0;return true;
  }
  return true; // Ordinary right-click: caller selects without enabling Auto.
}

void standaloneTitle(const __FlashStringHelper* title) {
#if defined(ENABLE_LCD) || defined(ENABLE_OLED)
  if(standalonePreset && (standaloneIs(title,F("OPEN SOURCE CART READER"))
      || standaloneIs(title,F("Select Game Boy")) || standaloneIs(title,F("Select device")))) {
    display.setCursor(6,8);
    display.print(F("BootDump: ")); display.println(standaloneDisplayName(standalonePreset));
  } else display.println(title);
#endif
}


bool standaloneFileTracking() { return standaloneStage==2 || standaloneStage==3; }

bool standaloneHeaderSelectable(const __FlashStringHelper* title) {
  return standalonePreset && (
      standaloneIs(title,F("OPEN SOURCE CART READER"))
      || standaloneIs(title,F("Select Game Boy")) || standaloneIs(title,F("Select device")));
}
void standaloneDisablePreset() {
  if(standalonePreset) standaloneSetPreset(standalonePreset);
}
