#ifndef _HEAD_MOTION_CONTROLLER_H
#define _HEAD_MOTION_CONTROLLER_H

#include "HeadMotion.h"

// 頭の動作を管理するコントローラ。
// setMotion() で動作を切り替え、update() を毎ループ呼び出す。
//
// 使い方:
//   headCtrl.setMotion(new IdleLookAround());  // アイドル時
//   headCtrl.stop();                           // 会話中など停止
//   headCtrl.update();                         // idle() から毎回呼ぶ
class HeadMotionController {
public:
    HeadMotionController();
    ~HeadMotionController();

    // 動作を切り替える。現在の動作の onExit() を呼んでから新しい動作の onEnter() を呼ぶ。
    // nullptr を渡すと動作なし（stop() と同じ）。
    // 所有権はこのクラスが持つ（呼び出し元で delete しない）。
    void setMotion(HeadMotion* motion);

    // 現在の動作を停止して正面に戻す
    void stop();

    // 毎ループ呼ぶ。現在の動作の update() を呼び出す
    void update();

    bool isRunning() const { return _current != nullptr; }

private:
    HeadMotion* _current;
};

#endif  //_HEAD_MOTION_CONTROLLER_H
