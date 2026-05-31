#include <M5Unified.h>
#include "AudioWhisper.h"

//constexpr size_t record_number = 300/2;
constexpr size_t record_number = 400;
//constexpr size_t record_number = 200;
constexpr size_t record_length = 150;
constexpr size_t record_size = record_number * record_length;
constexpr size_t record_samplerate = 16000;
constexpr int headerSize = 44;

AudioWhisper::AudioWhisper() {
  const auto size = record_size * sizeof(int16_t) + headerSize;
  record_buffer = static_cast<byte*>(::heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  ::memset(record_buffer, 0, size);
}

AudioWhisper::~AudioWhisper() {
  ::heap_caps_free(record_buffer);  // heap_caps_malloc には heap_caps_free を使う
}

size_t AudioWhisper::GetSize() const {
  return actualSize > 0 ? actualSize : record_size * sizeof(int16_t) + headerSize;
}

static float calcRMS(const int16_t* data, int length);

int16_t* MakeHeader(byte* header) {
  const auto wavDataSize = record_number * record_length * 2;
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSizeMinus8 = wavDataSize + headerSize - 8;
  header[4] = (byte)(fileSizeMinus8 & 0xFF);
  header[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  header[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  header[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;  // linear PCM
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;  // linear PCM
  header[21] = 0x00;
  header[22] = 0x01;  // monoral
  header[23] = 0x00;
  header[24] = 0x80;  // sampling rate 16000
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;  // Byte/sec = 16000x2x1 = 32000
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;  // 16bit monoral
  header[33] = 0x00;
  header[34] = 0x10;  // 16bit
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavDataSize & 0xFF);
  header[41] = (byte)((wavDataSize >> 8) & 0xFF);
  header[42] = (byte)((wavDataSize >> 16) & 0xFF);
  header[43] = (byte)((wavDataSize >> 24) & 0xFF);
  return (int16_t*)&header[headerSize];
}

// 録音後にバッファ全体を解析して末尾の無音チャンクを除いた実際のチャンク数を返す。
// M5.Mic.record() は PSRAM への DMA 書き込みにキャッシュ問題があるため、
// M5.Mic.end() 完了後（DMA 確定後）に読むことで正しい値が得られる。
static int trimSilenceFromBuffer(const int16_t* wavData) {
  // 先頭5チャンクのノイズフロアを計測
  float noiseFloor = 0;
  for (int i = 0; i < 5; i++) {
    noiseFloor = max(noiseFloor, calcRMS(&wavData[i * record_length], record_length));
  }
  noiseFloor = max(noiseFloor, 100.0f);
  noiseFloor = min(noiseFloor, 500.0f);  // 声を拾っても跳ね上がらないよう上限を設定
  float threshold = noiseFloor * 2.5f;

  // 末尾から走査して音声がある最後のチャンクを探す
  int lastSpeechChunk = 5;
  for (int i = 5; i < (int)record_number; i++) {
    if (calcRMS(&wavData[i * record_length], record_length) > threshold) {
      lastSpeechChunk = i;
    }
  }

  // 末尾に余裕（約200ms = 10チャンク）を持たせる
  int trimmed = min(lastSpeechChunk + 10, (int)record_number);
  Serial.printf("[Audio] VAD trim: noiseFloor=%.1f threshold=%.1f lastSpeech=%d trimmed=%d (%.2fs)\n",
                noiseFloor, threshold, lastSpeechChunk, trimmed,
                (float)trimmed * record_length / record_samplerate);
  return trimmed;
}

void AudioWhisper::Record() {
  M5.Mic.begin();
  auto *wavData = MakeHeader(record_buffer);

  // 全チャンク録音（PSRAM への DMA 書き込みはキャッシュ問題があるため
  // リアルタイム VAD は使わず、end() 後にポスト処理で末尾無音を除去する）
  for (int i = 0; i < (int)record_number; ++i) {
    M5.Mic.record(&wavData[i * record_length], record_length, record_samplerate);
  }
  M5.Mic.end();

  // end() 完了後（DMA 確定後）にバッファを解析して末尾の無音を除去
  int actual_chunks = trimSilenceFromBuffer(wavData);

  // 実際の録音サイズで WAV ヘッダーを書き直す
  const int actualWavDataSize = actual_chunks * (int)record_length * 2;
  MakeHeader(record_buffer);  // まず全体サイズで作成
  // ヘッダのデータサイズフィールドだけ上書き
  record_buffer[40] = (byte)(actualWavDataSize & 0xFF);
  record_buffer[41] = (byte)((actualWavDataSize >> 8) & 0xFF);
  record_buffer[42] = (byte)((actualWavDataSize >> 16) & 0xFF);
  record_buffer[43] = (byte)((actualWavDataSize >> 24) & 0xFF);
  unsigned int fileSizeMinus8 = actualWavDataSize + headerSize - 8;
  record_buffer[4] = (byte)(fileSizeMinus8 & 0xFF);
  record_buffer[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  record_buffer[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  record_buffer[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);

  actualSize = actualWavDataSize + headerSize;

  Serial.printf("[Audio] recorded %d/%d chunks (%.2fs)\n",
                actual_chunks, (int)record_number,
                (float)actual_chunks * record_length / record_samplerate);
}

static float calcRMS(const int16_t* data, int length) {
  float sum = 0;
  for (int i = 0; i < length; i++) {
    float s = data[i];
    sum += s * s;
  }
  return sqrtf(sum / length);
}
