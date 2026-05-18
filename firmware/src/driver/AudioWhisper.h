#ifndef _AUDIOWHISPER_H
#define _AUDIOWHISPER_H

#include "AudioWhisper.h"

class AudioWhisper {
  byte*    record_buffer;
  int16_t* chunk_buf;     // VAD用ダブルバッファ（DMA RAM・永続確保）
  size_t   _actualSize = 0;
 public:
  AudioWhisper();
  ~AudioWhisper();

  const byte* GetBuffer() const { return record_buffer; }
  size_t GetSize() const { return _actualSize; }

  void Record();
};

#endif // _AUDIOWHISPER_H
