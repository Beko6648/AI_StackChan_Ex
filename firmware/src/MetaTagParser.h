#pragma once
#include <Arduino.h>

// テキスト内の [META]{...}[/META] タグを解析して感情・気分を適用し、タグを除去する。
// text はインプレースで書き換えられる（タグ除去後のテキストが残る）。
// ChatGPT モード・Claude Code モード共通で使用する。
void applyMetaTag(String& text);
