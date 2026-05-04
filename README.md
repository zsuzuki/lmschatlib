# lmschatlib

LM Studio の REST API ( `/api/v1/chat` ) を使う C++20 のシンプルなチャットライブラリです。
サーバー側のステート (`response_id`) を使って会話を継続できます。

## 特徴

- トークンは必須ではありません (未指定なら Authorization ヘッダーなし)
- `LMSCHAT_API_TOKEN` 環境変数で API トークンを渡せる
- `response_id` を `previous_response_id` として使うチャット継続
- ユーザーから `previous_response_id` を指定すれば上書き可能
- LM Studio の `integrations` による MCP / プラグイン呼び出し
- 複数プラグインを同時指定可能
- VLM 向けの画像入力 (`data:image/...;base64,...`) に対応
- スタッツは read-only で取得可能
- URL は `http://localhost:1234` がデフォルト。変更可能
- 依存最小・mac/linux 向け (POSIX sockets)
- サブモジュールとして組み込みやすい CMake 構成

## 使い方

### サブモジュールとして組み込み

```cmake
add_subdirectory(path/to/lmschatlib)

target_link_libraries(your_target PRIVATE lmschat)
```

### C++ 例

```cpp
#include "lmschat/client.h"

lmschat::ClientConfig config;
config.base_url = "http://localhost:1234";
if (const char* env_token = std::getenv("LMSCHAT_API_TOKEN");
    env_token != nullptr && env_token[0] != '\0') {
  config.api_token = std::string(env_token);
}
// config.api_token = std::string("YOUR_LM_STUDIO_TOKEN");

lmschat::Client client(config);
lmschat::ChatSession session(client, "zai-org/glm-4.7-flash");

lmschat::ChatOptions options;
options.integrations = {
  lmschat::Integration::plugin("mcp/fetch"),
  lmschat::Integration::plugin("mcp/playwright", {"browser_navigate", "browser_snapshot"}),
};

auto r1 = session.send("こんにちは。必要ならWebを確認して。", options);
// options.reasoning = lmschat::Reasoning::On;
// auto r1 = session.send("こんにちは", options);
// 次回は自動的に previous_response_id を使う
auto r2 = session.send("次の質問です");
```

画像付きで聞く例:

```cpp
lmschat::ImageInput image = lmschat::ImageInput::from_base64(
  "image/png",
  "BASE64_ENCODED_IMAGE_DATA");

auto response = client.chat(
  "qwen2-vl-2b-instruct",
  "この画像には何が写っていますか？",
  {std::move(image)});
```

## API 概要

- `Client::chat(model, input, previous_response_id, reasoning)`
  - 1 回のリクエストを送る
- `Client::chat(model, input, ChatOptions)`
  - `reasoning` / `previous_response_id` / `integrations` をまとめて送る
- `Client::chat(model, input, std::vector<ImageInput>, ChatOptions)`
  - 画像を `input` に含めて送る
- `ChatSession::send(message)`
  - 直前の `response_id` を自動で引き継いで送る
- `ChatSession::send(message, std::vector<ImageInput>, ChatOptions)`
  - 会話継続しながら画像付きメッセージを送る
- `ChatSession::send(message, reasoning)`
  - reasoning を指定して送る (`off` / `low` / `medium` / `high` / `on`)
  - モデルによって許可値が異なるため、未対応値を指定すると 400 が返る
- `ChatSession::send(message, ChatOptions)`
  - `integrations` を含む詳細オプションを指定して送る
- `ChatSession::send(message, std::vector<Integration>)`
  - 複数プラグイン/MCP を簡単に渡す
- `ChatSession::send(message, override_previous_response_id)`
  - 明示的に `previous_response_id` を上書きできる
- `ChatSession::send(message, override_previous_response_id, reasoning)`
  - `previous_response_id` と reasoning を同時に指定できる
- `ChatSession::last_stats()`
  - 最新レスポンスの stats を取得

`OutputChunk` は以下を持ちます:

- `type`
- `content` (`<think>...</think>` を除去した本文)
- `thinking` (`<think>...</think>` で囲まれた思考テキスト)
- `tool` (`tool_call` 時のツール名)
- `arguments_json` (`tool_call` 時の arguments)
- `output_json` (`tool_result` 相当の payload がある場合の raw JSON)
- `provider_info_json` (LM Studio が返す provider 情報の raw JSON)

## LM Studio integrations の指定

LM Studio の API token を使う場合:

```bash
export LMSCHAT_API_TOKEN="your-lm-studio-token"
```

文字列プラグイン指定:

```cpp
auto response = session.send("天気を調べて", {
  lmschat::Integration::plugin("mcp/fetch"),
  lmschat::Integration::plugin("mcp/weather")
});
```

`allowed_tools` を制限した指定:

```cpp
lmschat::ChatOptions options;
options.integrations = {
  lmschat::Integration::plugin(
    "mcp/playwright",
    {"browser_navigate", "browser_snapshot"})
};
auto response = session.send("docs を開いて要点をまとめて", options);
```

ephemeral MCP server 指定:

```cpp
lmschat::ChatOptions options;
options.integrations = {
  lmschat::Integration::ephemeral_mcp(
    "internal-api",
    "http://localhost:8787/mcp",
    {"search_docs"},
    {{"Authorization", "Bearer TOKEN"}})
};
auto response = session.send("社内仕様を確認して", options);
```

## 画像入力

LM Studio REST API の画像入力は base64 data URL を使います。このライブラリでは `ImageInput::from_base64()` か `ImageInput::from_data_url()` で作成します。

```cpp
lmschat::ImageInput image = lmschat::ImageInput::from_data_url(
  "data:image/jpeg;base64,...");

lmschat::ChatOptions options;
options.reasoning = lmschat::Reasoning::Off;

auto response = session.send(
  "画像を短く説明して",
  {std::move(image)},
  options);
```

LM Studio が扱える画像形式は JPEG / PNG / WebP です。画像入力には VLM (Vision-Language Model) を使ってください。

## 注意

- HTTPS は未対応です (ローカル前提の最小実装)
- エラー時は `std::runtime_error` を投げます

## ビルド

```bash
cmake -S . -B build
cmake --build build
```

例をビルドする場合:

```bash
cmake -S . -B build -DLMSCHAT_BUILD_EXAMPLES=ON
cmake --build build
```

1行チャット例 (`examples/one_shot.cpp`) を使う場合:

```bash
./build/lmschat_one_shot "こんにちは"
./build/lmschat_one_shot --reasoning high "Rustの所有権を1分で説明して"
./build/lmschat_one_shot --plugin mcp/fetch --plugin mcp/playwright "Webで確認して"
LMSCHAT_API_TOKEN="your-lm-studio-token" ./build/lmschat_one_shot --plugin mcp/pagemcp "このページを読んで"
LMSCHAT_MODEL="openai/gpt-oss-20b" ./build/lmschat_one_shot "環境変数でモデル指定"
./build/lmschat_one_shot --model "openai/gpt-oss-20b" "オプション指定が優先"
```

画像質問例 (`examples/image_question.cpp`) を使う場合:

```bash
./build/lmschat_image_question ./image.png "この画像には何が写っていますか？"
LMSCHAT_MODEL="qwen2-vl-2b-instruct" ./build/lmschat_image_question ./image.jpg "要点だけ説明して"
```
