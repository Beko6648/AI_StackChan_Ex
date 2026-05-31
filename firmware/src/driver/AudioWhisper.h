#ifndef _AUDIOWHISPER_H
#define _AUDIOWHISPER_H

#include "AudioWhisper.h"

class AudioWhisper {
  byte*  record_buffer;
  size_t actualSize = 0;  // VAD で実際に録音したバイト数（ヘッダー含む）
 public:
  AudioWhisper();
  ~AudioWhisper();

  const byte* GetBuffer() const { return record_buffer; }
  size_t GetSize() const;

  void Record();
};

#endif // _AUDIOWHISPER_H
