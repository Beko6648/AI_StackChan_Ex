#include "MoodManager.h"

MoodManager* g_moodManager = nullptr;

MoodManager::MoodManager()
    : _joy(0.0f)
    , _trust(0.0f)
    , _sleepiness(0.0f)
    , _wantToTalk(0.0f)
    , _lastUpdateMs(0)
{}

void MoodManager::update() {
    unsigned long now = millis();
    if (_lastUpdateMs == 0) {
        _lastUpdateMs = now;
        return;
    }

    // 更新インターバル未満なら何もしない
    if (now - _lastUpdateMs < UPDATE_INTERVAL_MS) return;

    float deltaSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    _wantToTalk = clamp(_wantToTalk + WANT_TO_TALK_RATE * deltaSeconds, 0.0f, 1.0f);
    _sleepiness = clamp(_sleepiness + SLEEPINESS_RATE * deltaSeconds, 0.0f, 1.0f);

    // joy/trust を中立方向へ減衰
    float joyDecay   = JOY_DECAY_RATE   * deltaSeconds;
    float trustDecay = TRUST_DECAY_RATE * deltaSeconds;
    if      (_joy >  joyDecay)   _joy -= joyDecay;
    else if (_joy < -joyDecay)   _joy += joyDecay;
    else                         _joy  = 0.0f;
    if      (_trust >  trustDecay) _trust -= trustDecay;
    else if (_trust < -trustDecay) _trust += trustDecay;
    else                           _trust  = 0.0f;

    Serial.printf("[MoodManager] joy=%.2f trust=%.2f sleepiness=%.2f wantToTalk=%.2f\n",
                  _joy, _trust, _sleepiness, _wantToTalk);
}

void MoodManager::onConversation() {
    _wantToTalk = 0.0f;
    _sleepiness = clamp(_sleepiness - 0.3f, 0.0f, 1.0f);
}

void MoodManager::onSelfTalk() {
    _wantToTalk = 0.0f;
}

bool MoodManager::shouldSpeak() const {
    return _wantToTalk >= WANT_TO_TALK_THRESHOLD;
}

Expression MoodManager::getDominantExpression() const {
    float joyStrength   = fabsf(_joy);
    float trustStrength = fabsf(_trust);
    float sleepStrength = _sleepiness;

    // 最も強いパラメータを特定して表情を決定
    if (sleepStrength >= joyStrength && sleepStrength >= trustStrength) {
        if (_sleepiness >= SLEEPINESS_THRESHOLD) return Expression::Sleepy;
    } else if (joyStrength >= trustStrength) {
        if (_joy >=  MOOD_THRESHOLD) return Expression::Happy;
        if (_joy <= -MOOD_THRESHOLD) return Expression::Sad;
    } else {
        if (_trust >=  MOOD_THRESHOLD) return Expression::Happy;
        if (_trust <= -MOOD_THRESHOLD) return Expression::Angry;
    }
    return Expression::Neutral;
}

String MoodManager::getMoodDescription() const {
    String desc;
    if      (_joy >  0.3f) desc += "楽しい気分";
    else if (_joy < -0.3f) desc += "少し落ち込み気味";
    else                   desc += "普通の気分";

    if      (_trust >  0.3f) desc += "・穏やか";
    else if (_trust < -0.3f) desc += "・少しイライラ";

    if (_sleepiness > 0.5f) desc += "・眠い";

    return desc;
}
