#!/usr/bin/env bun
/**
 * webhook_channel.ts — StackChan ↔ クローディア Channels Webhook ブリッジ
 *
 * StackChan (CoreS3) が HTTP POST でテキストを送信 → クローディアのセッションに届く。
 * クローディアが reply ツールを呼ぶ → StackChan の /command_result に返答を送信する。
 *
 * 起動方法:
 *   claude --dangerously-load-development-channels server:webhook_channel \
 *          --channels plugin:discord@claude-plugins-official ...
 *
 * StackChan から送られる POST body (JSON):
 *   { "command_id": "1", "text": "こんにちは", "type": "user", "system_prompt": "" }
 *
 * StackChan の /command_result に送る body (JSON):
 *   { "command_id": "1", "voice_text": "こんにちは、マスター！" }
 */

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  ListToolsRequestSchema,
  CallToolRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";

// StackChan のデフォルト IP（SC_ExConfig.yaml の webhook_url で上書き可能）
const STACKCHAN_IP = process.env.STACKCHAN_IP ?? "192.168.1.114";
const WEBHOOK_PORT = parseInt(process.env.WEBHOOK_PORT ?? "8788");

const mcp = new Server(
  { name: "webhook_channel", version: "0.1.0" },
  {
    capabilities: {
      experimental: { "claude/channel": {} },
      tools: {},
    },
    instructions:
      "StackChan からの音声入力がチャンネルイベントとして届く。" +
      "イベントは <channel source=\"webhook_channel\" command_id=\"...\" type=\"...\"> の形式。" +
      "command_id と chat_id を必ず reply ツールに渡すこと。" +
      "type が 'user' ならユーザーの発話。'self_talk' なら自発発話のきっかけ。" +
      "返答は reply ツールで送る。ターミナル出力はStackChanには届かない。",
  }
);

// reply ツール: クローディアが呼ぶとStackChanに返答を送信する
mcp.setRequestHandler(ListToolsRequestSchema, async () => ({
  tools: [
    {
      name: "reply",
      description:
        "StackChan に返答テキストを送信する。クローディアがStackChanへ応答する唯一の手段。",
      inputSchema: {
        type: "object",
        properties: {
          command_id: {
            type: "string",
            description: "受信したイベントの command_id をそのまま渡す",
          },
          text: {
            type: "string",
            description: "StackChan が音声で読み上げるテキスト",
          },
        },
        required: ["command_id", "text"],
      },
    },
  ],
}));

mcp.setRequestHandler(CallToolRequestSchema, async (req) => {
  if (req.params.name !== "reply") {
    throw new Error(`unknown tool: ${req.params.name}`);
  }

  const { command_id, text } = req.params.arguments as {
    command_id: string;
    text: string;
  };

  const body = JSON.stringify({ command_id, voice_text: text });
  const url = `http://${STACKCHAN_IP}/command_result`;

  // fire-and-forget: StackChan は TTS 開始で HTTP レスポンスが遅れるため
  // レスポンスを待たず即座に完了扱いにする
  fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body,
  }).then((res) => {
    process.stderr.write(`[webhook] reply → ${url} status=${res.status}\n`);
  }).catch((e) => {
    process.stderr.write(`[webhook] reply error (ignored): ${e.message}\n`);
  });

  process.stderr.write(`[webhook] reply sent (fire-and-forget): "${text.substring(0, 40)}"\n`);
  return { content: [{ type: "text", text: "sent" }] };
});

await mcp.connect(new StdioServerTransport());

// HTTP サーバー: StackChan からのPOSTを受け取りクローディアに転送する
Bun.serve({
  port: WEBHOOK_PORT,
  hostname: "0.0.0.0", // LAN全体から受け付ける（StackChanはLAN経由でアクセス）
  async fetch(req) {
    if (req.method !== "POST") {
      return new Response("method not allowed", { status: 405 });
    }

    let body: string;
    try {
      body = await req.text();
    } catch {
      return new Response("bad request", { status: 400 });
    }

    process.stderr.write(`[webhook] received: ${body.substring(0, 100)}\n`);

    let command_id = "unknown";
    let type = "user";

    try {
      const json = JSON.parse(body);
      command_id = json.command_id ?? "unknown";
      type = json.type ?? "user";
    } catch {
      // JSON パース失敗はそのままテキストとして扱う
    }

    await mcp.notification({
      method: "notifications/claude/channel",
      params: {
        content: body,
        meta: { command_id, type },
      },
    });

    return new Response("ok");
  },
});

process.stderr.write(
  `[webhook] started on port ${WEBHOOK_PORT}, StackChan=${STACKCHAN_IP}\n`
);
