#include "HeadMotionController.h"
#include "Robot.h"

extern Robot* robot;
extern bool servo_manual;

HeadMotionController::HeadMotionController()
    : _current(nullptr)
{
}

HeadMotionController::~HeadMotionController() {
    if (_current) {
        _current->onExit();
        delete _current;
    }
}

void HeadMotionController::setMotion(HeadMotion* motion) {
    // 現在の動作を終了
    if (_current) {
        _current->onExit();
        delete _current;
        _current = nullptr;
    }

    // 新しい動作を開始
    _current = motion;
    if (_current) {
        servo_manual = true;   // サーボタスクの干渉を防ぐ
        _current->onEnter();
    } else {
        servo_manual = false;  // 動作なしのときはサーボタスクに戻す
    }
}

void HeadMotionController::stop() {
    setMotion(nullptr);  // servo_manual = false はここで設定される
}

void HeadMotionController::update() {
    if (_current) {
        _current->update();
    }
}
