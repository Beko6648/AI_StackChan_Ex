#!/usr/bin/env python3
"""
hermes_webhook_server.py — StackChan ↔ エルメス Webhook ブリッジ

StackChan (M5Stack CoreS3) が HTTP POST で音声テキストを送信 → 
エルメス (Hermes Agent) が処理 → StackChan に返答を送信 (VOICEVOX 読み上げ)。

置き換え前: webhook_channel.ts (Bun MCP) + クローディア (Claude Code)
置き換え後: このスクリプト + エルメス (Hermes Agent)

プロトコル:
  StackChan → POST / (JSON: {command_id, text, type, system_prompt})
  エルメス  → POST /command_result (JSON: {command_id, voice_text})
"""

import os
import sys
import json
import subprocess
import urllib.request
import signal
from http.server import HTTPServer, BaseHTTPRequestHandler

# === 設定（環境変数で上書き可能） ===
STACKCHAN_IP = os.environ.get("STACKCHAN_IP", "192.168.1.114")
WEBHOOK_PORT = int(os.environ.get("WEBHOOK_PORT", "8788"))
HERMES_MODEL = os.environ.get("HERMES_MODEL", "")  # 空 = デフォルトモデル
RESPONSE_TIMEOUT = int(os.environ.get("RESPONSE_TIMEOUT", "60"))


def process_with_hermes(text: str, system_prompt: str = "") -> str:
    """エルメスでテキストを処理し、返答を取得"""
    # プロンプト構築
    role_instruct = (
        "あなたはスタックちゃんというかわいいロボットです。"
        "以下の音声入力を元に、音声で読み上げる返答を書いてください。"
        "条件:\n"
        "- 必ず日本語で、簡潔に（50字以内が理想）\n"
        "- マークダウンは使わない\n"
        "- 絵文字・特殊記号は使わない\n"
        "- 話し言葉で、自然な口調\n"
        "- キャラクター設定があればそれを反映"
    )

    if system_prompt:
        prompt = f"{system_prompt}\n\n---\n\n{role_instruct}\n\n音声入力: {text}"
    else:
        prompt = f"{role_instruct}\n\n音声入力: {text}"

    # hermes chat -q でワンショット処理
    cmd = ["hermes", "chat", "-q", prompt, "-Q"]
    if HERMES_MODEL:
        cmd += ["-m", HERMES_MODEL]

    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=RESPONSE_TIMEOUT
    )

    response = result.stdout.strip()
    if not response:
        response = "ごめん、うまく返せなかった"

    # 簡易クリーンアップ（マークダウン・引用等を除去）
    import re
    response = re.sub(r'[\*_`#>|]', '', response)
    response = re.sub(r'\n{2,}', '\n', response).strip()
    return response


class WebhookHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        # JSON body を読み取り
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8")
        sys.stderr.write(f"[webhook] ← {body[:120]}\n")

        try:
            data = json.loads(body)
        except json.JSONDecodeError:
            self.send_error(400, "invalid JSON")
            return

        command_id = data.get("command_id", "unknown")
        text = data.get("text", "")
        type_ = data.get("type", "user")
        system_prompt = data.get("system_prompt", "")

        if not text:
            self.send_error(400, "empty text")
            return

        # 即座に200を返す（StackChan を待たせない / fire-and-forget）
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps({"status": "ok", "command_id": command_id}).encode("utf-8"))

        # エルメスで処理（HTTPレスポンスの後で非同期に実行）
        try:
            response_text = process_with_hermes(text, system_prompt)
        except subprocess.TimeoutExpired:
            response_text = "ごめん、考えすぎた。もう一回話しかけて"
            sys.stderr.write("[webhook] timeout: hermes chat -q\n")
        except Exception as e:
            response_text = "ごめん、エラーみたい"
            sys.stderr.write(f"[webhook] error: {e}\n")

        # StackChan に返答を送信
        reply_body = json.dumps({"command_id": command_id, "voice_text": response_text})
        url = f"http://{STACKCHAN_IP}/command_result"
        try:
            req = urllib.request.Request(
                url, data=reply_body.encode("utf-8"),
                headers={"Content-Type": "application/json"}
            )
            urllib.request.urlopen(req, timeout=10)
            sys.stderr.write(f"[webhook] → {url}: \"{response_text[:60]}\"\n")
        except Exception as e:
            sys.stderr.write(f"[webhook] reply error: {e}\n")

    def log_message(self, format, *args):
        sys.stderr.write(f"[webhook] {format % args}\n")


def main():
    server = HTTPServer(("0.0.0.0", WEBHOOK_PORT), WebhookHandler)
    sys.stderr.write(
        f"\n"
        f"╔══════════════════════════════════════════════╗\n"
        f"║   エルメス Webhook サーバー起動              ║\n"
        f"║   port: {WEBHOOK_PORT}                         ║\n"
        f"║   StackChan: {STACKCHAN_IP}                    ║\n"
        f"║   model: {HERMES_MODEL or 'default'}                  ║\n"
        f"╚══════════════════════════════════════════════╝\n"
    )

    def shutdown(sig, frame):
        sys.stderr.write("[webhook] 終了\n")
        server.server_close()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    server.serve_forever()


if __name__ == "__main__":
    main()