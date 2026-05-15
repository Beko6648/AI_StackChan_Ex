#include "HeadTouch.h"

void HeadTouch::begin() {
    Serial.printf("[HeadTouch] init addr=0x%02X\n", I2C_ADDR);

    // si12t_enable_channel: REF_RST / CH_HOLD / CAL_HOLD を 0x00 で有効化
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x0A, 0x00, I2C_FREQ);  // REF_RST1
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x0B, 0x00, I2C_FREQ);  // REF_RST2
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x0C, 0x00, I2C_FREQ);  // CH_HOLD1
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x0D, 0x00, I2C_FREQ);  // CH_HOLD2
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x0E, 0x00, I2C_FREQ);  // CAL_HOLD1
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x0F, 0x00, I2C_FREQ);  // CAL_HOLD2

    // si12t_set_ctrl2: ソフトリセット後に動作モード設定
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x09, 0x0F, I2C_FREQ);  // CTRL2: reset
    delay(10);
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x09, 0x07, I2C_FREQ);  // CTRL2: 動作設定

    // si12t_set_ctrl1: オートモード設定
    M5.In_I2C.writeRegister8(I2C_ADDR, 0x08, 0x22, I2C_FREQ);  // CTRL1

    // si12t_set_sensitivity: TYPE_HIGH / LEVEL_7 → 0xFF（最大感度）
    for (uint8_t reg = 0x02; reg <= 0x07; reg++) {
        M5.In_I2C.writeRegister8(I2C_ADDR, reg, 0xFF, I2C_FREQ);
    }

    Serial.println("[HeadTouch] init done");
}

void HeadTouch::readTouchData() {
    // 0x10の1バイトに2bit×3ゾーンが詰まっている（公式準拠）
    uint8_t raw = M5.In_I2C.readRegister8(I2C_ADDR, 0x10, I2C_FREQ);
    for (int i = 0; i < 3; i++) {
        _intensity[i] = (raw >> (i * 2)) & 0x03;
    }

    // タッチ時に全レジスタをスキャンして変化を確認
}

bool HeadTouch::isTouched() const {
    uint8_t maxVal = _intensity[0];
    if (_intensity[1] > maxVal) maxVal = _intensity[1];
    if (_intensity[2] > maxVal) maxVal = _intensity[2];
    return maxVal >= 1;
}

int16_t HeadTouch::getPosition() const {
    uint16_t total = _intensity[0] + _intensity[1] + _intensity[2];
    if (total == 0) return 0;
    // zone0=-100, zone1=0, zone2=+100
    int32_t weighted = (int32_t)_intensity[0] * (-100) + (int32_t)_intensity[2] * 100;
    return (int16_t)(weighted / total);
}

HeadTouch::Gesture HeadTouch::update() {
    unsigned long now = millis();
    if (now - _lastPollMs < POLL_INTERVAL_MS) return Gesture::None;
    _lastPollMs = now;

    readTouchData();
    bool touched = isTouched();

    switch (_state) {
        case TouchState::IDLE:
            if (touched) {
                _state            = TouchState::TOUCHED;
                _initialPos       = getPosition();
                _touchStartMs     = now;
                _longPressEmitted = false;
                Serial.printf("[HeadTouch] Press pos=%d\n", _initialPos);
                return Gesture::Press;
            }
            break;

        case TouchState::TOUCHED:
            if (!touched) {
                _state = TouchState::IDLE;
                Serial.println("[HeadTouch] Release");
                return Gesture::Release;
            } else {
                int16_t delta = getPosition() - _initialPos;
                if (delta > SWIPE_THRESHOLD) {
                    _state = TouchState::SWIPING;
                    Serial.printf("[HeadTouch] SwipeForward delta=%d\n", delta);
                    return Gesture::SwipeForward;
                } else if (delta < -SWIPE_THRESHOLD) {
                    _state = TouchState::SWIPING;
                    Serial.printf("[HeadTouch] SwipeBackward delta=%d\n", delta);
                    return Gesture::SwipeBackward;
                } else if (!_longPressEmitted && (now - _touchStartMs) >= LONG_PRESS_MS) {
                    _longPressEmitted = true;
                    Serial.println("[HeadTouch] LongPress");
                    return Gesture::LongPress;
                }
            }
            break;

        case TouchState::SWIPING:
            if (!touched) {
                _state = TouchState::IDLE;
                Serial.println("[HeadTouch] Release (after swipe)");
                return Gesture::Release;
            }
            break;
    }

    return Gesture::None;
}
