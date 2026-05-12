#ifndef _HEAD_MOTION_H
#define _HEAD_MOTION_H

// 頭の動きを表す抽象基底クラス。
// 新しい動作を追加する場合はこのクラスを継承して update() を実装する。
class HeadMotion {
public:
    virtual ~HeadMotion() = default;

    // 動作開始時に1回呼ばれる
    virtual void onEnter() {}

    // HeadMotionController::update() から毎ループ呼ばれる
    virtual void update() = 0;

    // 動作終了時に1回呼ばれる（別の動作に切り替える直前など）
    virtual void onExit() {}
};

#endif  //_HEAD_MOTION_H
