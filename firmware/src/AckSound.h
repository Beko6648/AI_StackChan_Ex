#ifndef __ACK_SOUND_H__
#define __ACK_SOUND_H__

#include "StackchanExConfig.h"

// STT完了後に呼び出す。stackChanMind の現在の感情に応じた相槌を再生する
void playAckSound(const ex_config_s& cfg);

// sleepiness が 0.8 に達したときに一度だけ再生するあくび音声
void playYawnSound(const ex_config_s& cfg);

// 就寝時に再生する音声
void playSleepSound(const ex_config_s& cfg);

// 起床時に再生する音声
void playWakeupSound(const ex_config_s& cfg);

#endif
