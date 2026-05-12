#include "IdleLookAround.h"
#include "Robot.h"

extern Robot* robot;

IdleLookAround::IdleLookAround()
    : _lookStartTime(0)
    , _lookDuration(0)
{
}

void IdleLookAround::onEnter() {
    // 開始直後に最初のランダム方向へ振り向く
    moveToNextRandom();
}

void IdleLookAround::update() {
    if (millis() - _lookStartTime >= _lookDuration) {
        // 時間が来たら次のランダム方向へ振り向く
        moveToNextRandom();
    }
}

void IdleLookAround::onExit() {
    // 正面に戻す
#ifdef USE_SERVO
    robot->servo->moveToGaze(0, 0);
#endif
}

void IdleLookAround::moveToNextRandom() {
    // 中心を起点とした絶対角度でランダムな方向に一気に振り向く
    int gazeX = random(-GAZE_X_MAX, GAZE_X_MAX + 1);
    int gazeY = random(-GAZE_Y_MAX, GAZE_Y_MAX + 1);
#ifdef USE_SERVO
    robot->servo->moveToGaze(gazeX, gazeY);
#endif
    Serial.printf("IdleLookAround: -> (%d, %d)\n", gazeX, gazeY);

    _lookStartTime = millis();
    _lookDuration  = random(LOOK_MIN_MS, LOOK_MAX_MS + 1);
}
