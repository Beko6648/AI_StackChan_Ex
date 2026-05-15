#ifndef _HEAD_TOUCH_H
#define _HEAD_TOUCH_H

#include <Arduino.h>
#include <M5Unified.h>

// Si12T 3ゾーンタッチセンサードライバ
// I2Cアドレス: 0x68  SDA=G12  SCL=G11
//
// ジェスチャー（公式実装準拠）:
//   Press        : タッチ開始
//   Release      : タッチ終了
//   SwipeForward : 左→右スワイプ（位置変化 > 閾値）
//   SwipeBackward: 右→左スワイプ（位置変化 < -閾値）
class HeadTouch {
public:
    enum class Gesture { None, Press, Release, SwipeForward, SwipeBackward };

    void begin();
    Gesture update();

private:
    enum class TouchState { IDLE, TOUCHED, SWIPING };

    // OUTPUT1レジスタ1バイトを2bit×3ゾーンにパース
    void readTouchData();

    // 強度の加重平均で位置を算出（-100〜100）
    int16_t getPosition() const;

    bool isTouched() const;

    uint8_t       _intensity[3]   = {0, 0, 0};  // zone0/zone1/zone2（前後方向。実機で要確認）
    TouchState    _state          = TouchState::IDLE;
    int16_t       _initialPos     = 0;
    unsigned long _lastPollMs     = 0;

    static constexpr uint8_t  I2C_ADDR        = 0x68;
    static constexpr uint32_t I2C_FREQ        = 400000;
    static constexpr uint32_t POLL_INTERVAL_MS = 50;   // 公式版準拠
    static constexpr int16_t  SWIPE_THRESHOLD = 40;    // 公式版準拠（-100〜100中の40%）
};

#endif  // _HEAD_TOUCH_H
