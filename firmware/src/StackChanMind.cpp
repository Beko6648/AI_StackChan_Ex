#include "StackChanMind.h"
#include "Avatar.h"

using namespace m5avatar;

extern Avatar avatar;

// グローバルインスタンス
StackChanMind stackChanMind;

const StackChanMind::EmotionEntry StackChanMind::_emotionTable[] = {
    { "happy",   Expression::Happy   },
    { "neutral", Expression::Neutral },
    { "sad",     Expression::Sad     },
    { "angry",   Expression::Angry   },
    { "doubt",   Expression::Doubt   },
    { "sleepy",  Expression::Sleepy  },
};
const int StackChanMind::_emotionTableSize =
    sizeof(StackChanMind::_emotionTable) / sizeof(StackChanMind::_emotionTable[0]);

StackChanMind::StackChanMind() : _emotion(Expression::Neutral) {}

Expression StackChanMind::getEmotion() const {
    return _emotion;
}

String StackChanMind::emotionToString() const {
    for (int i = 0; i < _emotionTableSize; i++) {
        if (_emotionTable[i].expression == _emotion) {
            return String(_emotionTable[i].name);
        }
    }
    return "neutral";
}

void StackChanMind::setEmotion(const String& emotionName) {
    for (int i = 0; i < _emotionTableSize; i++) {
        if (emotionName.equalsIgnoreCase(_emotionTable[i].name)) {
            _emotion = _emotionTable[i].expression;
            avatar.setExpression(_emotion);  // 感情変化と同時に表情も即反映
            return;
        }
    }
    // 未知の感情名は neutral にフォールバック
    _emotion = Expression::Neutral;
    avatar.setExpression(_emotion);
}

void StackChanMind::applyExpression() {
    avatar.setExpression(_emotion);
}
