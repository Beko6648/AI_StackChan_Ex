#include "HeadTouch.h"

void HeadTouch::begin() {
    Serial.printf("[HeadTouch] init addr=0x%02X\n", I2C_ADDR);
}

uint8_t HeadTouch::readStatus() {
    return M5.In_I2C.readRegister8(I2C_ADDR, OUTPUT_REG, 400000) & 0x07;
}

HeadTouch::Gesture HeadTouch::update() {
    unsigned long now = millis();
    if (now - _lastPollMs < 20) return Gesture::None;
    _lastPollMs = now;

    uint8_t status = readStatus();
    bool touched = (status != 0);

    if (touched) {
        _releaseStartMs = 0;  // 離しタイマーをリセット

        if (!_touching) {
            // タッチ開始
            _touching     = true;
            _touchStartMs = now;
            Serial.printf("[HeadTouch] touch start status=0x%02X\n", status);
        }
        _lastStatus = status;

    } else if (_touching) {
        // 離し始め → デバウンス待ち
        if (_releaseStartMs == 0) {
            _releaseStartMs = now;
        } else if (now - _releaseStartMs >= RELEASE_DEBOUNCE_MS) {
            // 100ms 以上 0x00 が続いた → 確定リリース
            _touching       = false;
            _releaseStartMs = 0;

            unsigned long duration = now - _touchStartMs;

            Serial.printf("[HeadTouch] release duration=%lums\n", duration);

            // 撫で判定: 長押し（境界タップによる複数ゾーン誤検知を避けるため時間のみで判定）
            if (duration >= STROKE_MIN_MS) {
                Serial.println("[HeadTouch] -> Stroke");
                _lastTapMs = 0;
                return Gesture::Stroke;
            }

            // ダブルタップ判定: 同じゾーンを DOUBLE_TAP_MS 以内に 2 回
            // ダブルタップ判定: DOUBLE_TAP_MS 以内に 2 回タップ（ゾーン不問）
            unsigned long interval = (_lastTapMs > 0) ? (now - _lastTapMs) : 9999;
            Serial.printf("[HeadTouch] tap check: interval=%lums\n", interval);
            if (_lastTapMs > 0 && interval <= DOUBLE_TAP_MS) {
                Serial.println("[HeadTouch] -> DoubleTap");
                _lastTapMs = 0;
                return Gesture::DoubleTap;
            }

            _lastTapMs = now;
        }
    }

    return Gesture::None;
}
