# lmschatlib

LM Studio の REST API ( /api/v1/chat ) を使う C++20 のシンプルなチャットライブラリです。
サーバー側のステート (`response_id`) を使って会話を継続できます。

## 特徴

- トークンは必須ではありません (未指定なら Authorization ヘッダーなし)
- `response_id` を `previous_response_id` として使うチャット継続
- ユーザーから `previous_response_id` を指定すれば上書き可能
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
// config.api_token = std::string("YOUR_TOKEN");

lmschat::Client client(config);
lmschat::ChatSession session(client, "zai-org/glm-4.7-flash");

auto r1 = session.send("こんにちは");
// auto r1 = session.send("こんにちは", lmschat::Reasoning::On);
// 次回は自動的に previous_response_id を使う
auto r2 = session.send("次の質問です");
```

## API 概要

- `Client::chat(model, input, previous_response_id, reasoning)`
  - 1 回のリクエストを送る
- `ChatSession::send(message)`
  - 直前の `response_id` を自動で引き継いで送る
- `ChatSession::send(message, reasoning)`
  - reasoning を指定して送る (`off` / `low` / `medium` / `high` / `on`)
  - モデルによって許可値が異なるため、未対応値を指定すると 400 が返る
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
