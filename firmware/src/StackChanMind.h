#ifndef __STACKCHAN_MIND_H__
#define __STACKCHAN_MIND_H__

#include <Arduino.h>
#include "Avatar.h"

// スタックちゃんの内面・感情状態を管理するクラス
// 将来的に自律思考機能（自発発話・気分の時間変化）もここに追加する
class StackChanMind {
public:
    StackChanMind();

    // 現在の感情を返す
    m5avatar::Expression getEmotion() const;

    // 感情を文字列で返す（LLM プロンプト埋め込み用）
    String emotionToString() const;

    // 感情名（"happy" 等）を受け取り内部状態を更新し、即座に表情にも反映する
    // LLM 返答・時間経過による感情変化など、感情が変わるすべての箇所から呼び出す
    void setEmotion(const String& emotionName);

    // 内部状態を変えずに現在の感情表情を再適用する
    // 発話中に強制変更した表情を発話終了後に元に戻す用途で使用する
    void applyExpression();

private:
    m5avatar::Expression _emotion;

    // 感情名と Expression の対応テーブル
    struct EmotionEntry {
        const char*          name;
        m5avatar::Expression expression;
    };
    static const EmotionEntry _emotionTable[];
    static const int          _emotionTableSize;
};

extern StackChanMind stackChanMind;

#endif
