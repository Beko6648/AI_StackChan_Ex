#ifndef __ACK_SOUND_H__
#define __ACK_SOUND_H__

#include "StackchanExConfig.h"

// STT完了後に呼び出す。stackChanMind の現在の感情に応じた相槌を再生する
void playAckSound(const ex_config_s& cfg);

#endif
