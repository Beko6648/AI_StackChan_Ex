#ifndef _IDLE_LOOK_AROUND_H
#define _IDLE_LOOK_AROUND_H

#include <Arduino.h>
#include "HeadMotion.h"

// アイドル時にランダムに頭を動かす動作。
// 目の動き（Avatar のサッカード）とは独立してサーボを直接制御する。
//
// 動作:
//   ランダムな絶対方向に一気に振り向く → 1〜10秒キープ → また別の方向へ振り向く → 繰り返し
//   正面への「戻り」動作はなく、方向から方向へ直接移動し続ける。
class IdleLookAround : public HeadMotion {
public:
    IdleLookAround();

    void onEnter() override;
    void update() override;
    void onExit() override;

private:
    unsigned long _lookStartTime;   // 現在の方向を向き始めた時刻 (ms)
    unsigned long _lookDuration;    // この方向を向いていた継続時間 (ms)

    // 継続時間の範囲
    static const unsigned long LOOK_MIN_MS = 1000;
    static const unsigned long LOOK_MAX_MS = 7000;

    // サーボ移動角度の絶対値最大（中心からのオフセット）
    static const int GAZE_X_MAX = 70;  // 水平 ±70度
    static const int GAZE_Y_MAX = 30;  // 垂直 ±30度

    void moveToNextRandom();
};

#endif  //_IDLE_LOOK_AROUND_H
