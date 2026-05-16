#include "AckSound.h"
#include "StackChanMind.h"
#include "driver/PlayMP3.h"

using namespace m5avatar;

// 各表情グループのファイル名（拡張子なし）と対応する Expression
struct ExprGroup {
    const char**  files;
    int           count;
    Expression    expression;
};

static const char* happy_files[]   = { "happy_hai"                   };
static const char* neutral_files[] = { "neutral_hai"                 };
static const char* sad_files[]     = { "sad_uun"                     };
static const char* angry_files[]   = { "angry_eee"                   };
static const char* doubt_files[]   = { "doubt_ettoo", "doubt_uun"    };
static const char* sleepy_files[]  = { "sleepy_uun"                  };

static const ExprGroup groups[] = {
    { happy_files,   1, Expression::Happy   },
    { neutral_files, 1, Expression::Neutral },
    { sad_files,     1, Expression::Sad     },
    { angry_files,   1, Expression::Angry   },
    { doubt_files,   2, Expression::Doubt   },
    { sleepy_files,  1, Expression::Sleepy  },
};
static const int N_GROUPS = sizeof(groups) / sizeof(groups[0]);

static const ExprGroup* findGroup(Expression expr) {
    for (int i = 0; i < N_GROUPS; i++) {
        if (groups[i].expression == expr) return &groups[i];
    }
    return &groups[1];  // フォールバック: neutral
}

// speakerId を設定から解決する。-1 の場合は WebVoiceVox の voice を使用する
static int resolveSpeakerId(const ex_config_s& cfg) {
    if (cfg.ack.speakerId >= 0) return cfg.ack.speakerId;
    if (cfg.tts.type == TTS_TYPE_WEB_VOICEVOX) return cfg.tts.voice.toInt();
    return 3;  // フォールバック
}

static void playEventSound(const ex_config_s& cfg, const char** files, int count, Expression expr) {
    int speakerId    = resolveSpeakerId(cfg);
    const char* file = files[random(count)];

    char path[64];
    snprintf(path, sizeof(path), "/ack/%d/%s.mp3", speakerId, file);

    Serial.printf("AckSound: %s\n", path);
    playMP3SDWithExpression(path, expr);
}

void playAckSound(const ex_config_s& cfg) {
    int speakerId        = resolveSpeakerId(cfg);
    const ExprGroup* grp = findGroup(stackChanMind.getEmotion());
    const char* file     = grp->files[random(grp->count)];

    char path[64];
    snprintf(path, sizeof(path), "/ack/%d/%s.mp3", speakerId, file);

    Serial.printf("AckSound: %s\n", path);
    playMP3SDWithExpression(path, grp->expression);
}

static const char* yawn_files[]   = { "yawn_fuwa"                              };
static const char* sleep_files[]  = { "sleep_oyasumi", "sleep_oyasuminasai"    };
static const char* wakeup_files[] = { "wakeup_ohayou", "wakeup_ohayougozaimasu" };

void playYawnSound(const ex_config_s& cfg) {
    playEventSound(cfg, yawn_files, 1, Expression::Sleepy);
}

void playSleepSound(const ex_config_s& cfg) {
    playEventSound(cfg, sleep_files, 2, Expression::Sleeping);
}

void playWakeupSound(const ex_config_s& cfg) {
    playEventSound(cfg, wakeup_files, 2, Expression::Neutral);
}
