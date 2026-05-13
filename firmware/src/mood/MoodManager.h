#ifndef _MOOD_MANAGER_H
#define _MOOD_MANAGER_H

#include <Arduino.h>
#include <Avatar.h>

using namespace m5avatar;

// 気分パラメータを管理し、自発発話タイミングと表情を決定する。
//
// パラメータ:
//   joy      楽しい(+1.0) ⇔ 悲しい(-1.0)
//   trust    信頼(+1.0) ⇔ 怒り(-1.0)
//   sleepiness  眠気 0.0〜1.0（時間経過で増加）
//   wantToTalk  話したい度 0.0〜1.0（時間経過で増加、発話でリセット）
class MoodManager {
public:
    MoodManager();

    // 時間経過による更新。idle() から毎回呼ぶ。
    void update();

    // 会話発生時に呼ぶ（wantToTalk・sleepiness をリセット）
    void onConversation();

    // 自発発話後に呼ぶ（wantToTalk をリセット）
    void onSpontaneousSpeech();

    // wantToTalk が閾値を超えたら true
    bool shouldSpeak() const;

    // 3パラメータのうち最も強いものに対応する表情を返す
    Expression getDominantExpression() const;

    // 自発発話プロンプト用の気分説明文を返す
    String getMoodDescription() const;

    float getJoy()        const { return _joy; }
    float getTrust()      const { return _trust; }
    float getSleepiness() const { return _sleepiness; }
    float getWantToTalk() const { return _wantToTalk; }

    void addJoy(float delta) { _joy = clamp(_joy + delta, -1.0f, 1.0f); }

private:
    float _joy;
    float _trust;
    float _sleepiness;
    float _wantToTalk;

    unsigned long _lastUpdateMs;

    // パラメータ更新の最小インターバル（ms）【テスト用: 10秒】
    static constexpr unsigned long UPDATE_INTERVAL_MS = 10000;

    // wantToTalk が閾値を超えたら自発発話【テスト用: 約2分で0→0.8】
    static constexpr float WANT_TO_TALK_THRESHOLD = 0.8f;
    static constexpr float WANT_TO_TALK_RATE      = 0.8f / 120.0f;

    // 眠気【テスト用: 約3分で0→0.5】
    static constexpr float SLEEPINESS_RATE      = 0.5f / 180.0f;
    static constexpr float SLEEPINESS_THRESHOLD = 0.5f;

    // 感情軸の閾値
    static constexpr float MOOD_THRESHOLD = 0.4f;

    static float clamp(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

#endif  // _MOOD_MANAGER_H
