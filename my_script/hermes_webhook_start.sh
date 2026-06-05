#!/usr/bin/env bash
# hermes_webhook_start.sh — エルメス Webhook サーバー起動スクリプト
# WSL (Ubuntu) 上で実行すること

set -e

cd "$(dirname "$0")"
SCRIPT_DIR="$(pwd)"
PIDFILE="${SCRIPT_DIR}/hermes_webhook.pid"
LOGFILE="${SCRIPT_DIR}/hermes_webhook.log"

# デフォルト値（環境変数で上書き可能）
export STACKCHAN_IP="${STACKCHAN_IP:-192.168.1.114}"
export WEBHOOK_PORT="${WEBHOOK_PORT:-8788}"
export HERMES_MODEL="${HERMES_MODEL:-}"
export RESPONSE_TIMEOUT="${RESPONSE_TIMEOUT:-60}"

case "${1:-start}" in
  start)
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
      echo "エルメス Webhook サーバーは既に起動しています (PID: $(cat "$PIDFILE"))"
      exit 0
    fi
    echo "エルメス Webhook サーバーを起動します..."
    echo "  StackChan: ${STACKCHAN_IP}"
    echo "  Port:      ${WEBHOOK_PORT}"
    echo "  Model:     ${HERMES_MODEL:-default}"
    echo "  Log:       ${LOGFILE}"
    nohup python3 -u "${SCRIPT_DIR}/hermes_webhook_server.py" >> "$LOGFILE" 2>&1 &
    echo $! > "$PIDFILE"
    sleep 2
    if kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
      echo "✅ 起動完了 (PID: $(cat "$PIDFILE"))"
    else
      echo "❌ 起動に失敗しました。ログを確認: ${LOGFILE}"
      exit 1
    fi
    ;;
  stop)
    if [ ! -f "$PIDFILE" ]; then
      echo "サーバーは起動していません"
      exit 0
    fi
    PID="$(cat "$PIDFILE")"
    echo "エルメス Webhook サーバーを停止します... (PID: ${PID})"
    kill "$PID" 2>/dev/null || true
    rm -f "$PIDFILE"
    echo "✅ 停止完了"
    ;;
  status)
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
      echo "✅ 稼働中 (PID: $(cat "$PIDFILE"))"
      echo "  Log: ${LOGFILE}"
      tail -3 "$LOGFILE" 2>/dev/null || true
    else
      echo "❌ 停止中"
    fi
    ;;
  restart)
    "$0" stop
    sleep 1
    "$0" start
    ;;
  *)
    echo "使い方: $0 {start|stop|status|restart}"
    exit 1
    ;;
esac