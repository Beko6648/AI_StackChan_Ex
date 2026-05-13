#ifndef _AI_STACKCHAN_MOD_H
#define _AI_STACKCHAN_MOD_H

#include <Arduino.h>
#include "mod/ModBase.h"
#include "head/HeadMotionController.h"
#include "mood/MoodManager.h"
#include "driver/HeadTouch.h"

class AiStackChanMod: public ModBase{
private:
    box_t box_servo;
    box_t box_stt;
    box_t box_BtnA;
    box_t box_BtnC;
    #if defined(ENABLE_CAMERA)
    box_t box_subWindow;
    #endif
    String avatarText;
    bool isOffline;
    HeadMotionController _headCtrl;   // 頭の動き制御
    MoodManager          _moodManager; // 気分パラメータ管理
    HeadTouch            _headTouch;   // 頭部タッチセンサー
public:
    AiStackChanMod(bool _isOffline);

    void init(void);
    void pause(void);
    void update(int page_no);
    void btnA_pressed(void);
    void btnB_longPressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void doubleTapped(float ax, float ay, float az);   // 加速度センサによるダブルタップ検出のコールバック。platformio.iniで-DENABLE_TAP_DETECTを有効にしてください
    void idle(void);
};


#endif  //_AI_STACKCHAN_MOD_H