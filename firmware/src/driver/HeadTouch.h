#ifndef _HEAD_TOUCH_H
#define _HEAD_TOUCH_H

#include <Arduino.h>
#include <M5Unified.h>

// Si12T / TSM12 互換 3ゾーンタッチセンサードライバ
// I2Cアドレス: 0x68  SDA=G12  SCL=G11
//
// ジェスチャー:
//   DoubleTap: 500ms 以内に 2回タッチ→離す
//   Stroke   : 400ms 以上タッチし続ける（撫でる）
class HeadTouch {
public:
    enum class Gesture { None, DoubleTap, Stroke };

    void begin();
    Gesture update();

private:
    uint8_t readStatus();

    uint8_t       _lastStatus  = 0;
    bool          _touching    = false;
    unsigned long _touchStartMs    = 0;
    unsigned long _lastTapMs       = 0;
    unsigned long _lastPollMs      = 0;
    unsigned long _releaseStartMs  = 0;  // 離し始めた時刻（デバウンス用）

    static constexpr uint8_t       I2C_ADDR       = 0x68;
    static constexpr uint8_t       OUTPUT_REG     = 0x10;  // bit0=zone1, bit1=zone2, bit2=zone3
    static constexpr unsigned long DOUBLE_TAP_MS      = 500;
    static constexpr unsigned long STROKE_MIN_MS      = 350;
    static constexpr unsigned long RELEASE_DEBOUNCE_MS = 60;   // ゾーン間の隙間を無視
};

#endif  // _HEAD_TOUCH_H
