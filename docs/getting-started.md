# Getting Started

この手順は、XIAO ESP32S3 Senseの単体試験、Game Boy側の準備、配線、総合試験の順に進める。最初から全機器を接続せず、片側ずつ動作を確認すると問題箇所を切り分けやすい。

## 1. 対応範囲を確認する

- 日本版『ポケットモンスター 赤』revision 1
- 動画の手順でゲーム内バイナリエディタを導入できるセーブ環境
- Seeed Studio XIAO ESP32S3 Sense
- 2.4 GHz Wi-Fi
- OpenAI APIまたはGemini APIの利用可能なキー
- WindowsとPlatformIO。本プロジェクトの検証環境はWindows版PlatformIOである

ROMの版は[対応ROMとGame Boy側の前提](supported-rom.md)に従って、利用者自身のROMを検査する。

## 2. 必要な部品を用意する

| 部品 | 数 | 条件 |
|---|---:|---|
| XIAO ESP32S3 Sense | 1 | カメラ拡張ボードを取り付ける |
| Game BoyまたはGame Boy Pocket | 1 | 現在の実機確認はPocket |
| 日本版ポケットモンスター赤 | 1 | 対応ROM版を確認する |
| Game Boy通信ケーブル | 1 | 加工用。端子をテスターで識別する |
| 5 V/3.3 Vレベル変換回路 | 3信号分 | SCK、SOUT、SINを変換する |
| USB電源または1セルLiPo | 1 | XIAOへ独立給電する |
| 配線材、はんだ、絶縁材 | 適量 | 隣接パッドへ素線を出さない |
| テスター | 1 | 導通、短絡、電源電圧の確認に使用 |

詳細は[公開用ハードウェアガイド](hardware.md)と[電源・発熱・配線](power-and-wiring.md)を参照する。

## 3. 開発環境を準備する

Visual Studio CodeへPlatformIO IDEを導入するか、PlatformIO Coreを利用する。リポジトリを取得し、そのルートをPlatformIOプロジェクトとして開く。

認証情報の雛形をコピーする。

```powershell
Copy-Item .\include\secrets.example.h .\include\secrets.h
```

`include/secrets.h`を編集する。

```cpp
#define WIFI_SSID "アクセスポイント名"
#define WIFI_PASSWORD "パスワード"
#define GEMINI_API_KEY "Geminiを使う場合のAPIキー"
#define OPENAI_API_KEY "OpenAIを使う場合のAPIキー"
```

`include/secrets.h`はGitへ追加しない。公開リポジトリには`secrets.example.h`だけを含める。

## 4. AIプロバイダを選ぶ

`include/app_config.h`の`AI_PROVIDER`を設定する。

```cpp
constexpr AiProvider AI_PROVIDER = AiProvider::kOpenAI;
```

Geminiを使う場合は次へ変更する。

```cpp
constexpr AiProvider AI_PROVIDER = AiProvider::kGemini;
```

モデルIDも同じファイルにある。API側で利用できるモデルへ変更する場合は、画像入力に対応していることを確認する。

## 5. XIAOへ書き込む

XIAOをUSBでPCへ接続し、ポート名を確認する。次はCOM5の例である。

```powershell
pio run
pio run -t upload --upload-port COM5
pio device monitor --port COM5 -b 115200
```

正常に起動すると、次の情報が表示される。

```text
GB AI Eye / XIAO ESP32S3 Sense
AI provider: OpenAI
AI model: ...
Game Boy text normalization: pass
Camera and Wi-Fi stay off until a capture request
```

`Game Boy text normalization`が`FAIL`なら、実機へ接続せずファームウェアのビルド状態を確認する。

## 6. XIAOを単体試験する

シリアルモニターへ半角`c`を送る。XIAOはGame Boyを接続していなくても、撮影、Wi-Fi接続、AI API呼び出し、Game Boy文字コード変換まで実行できる。

成功時は、概ね次のログが出る。

```text
USB serial requested a capture
Camera frame: ...
Wi-Fi connected: ...
OpenAI attempt 1/3: ... glyphs, ... bytes, 0 unsupported
OpenAI: ...
Game Boy caption: ... bytes
Game Boy bytes: ...
```

この試験が成功するまでGame Boyへ接続しない。失敗時は[トラブルシューティング](troubleshooting.md)を参照する。

## 7. Game Boy側を準備する

バイナリエディタの作成は、次の動画を参照する。

- [初代ポケモン お手軽20分。バイナリエディタ作成チャート解説](https://youtu.be/qV1EJU6howc)

動画の手順を完了し、ゲーム内でアドレス移動、値の編集、カーソル位置からの実行、ゲームへの復帰ができることを確認する。

続いて[バイナリエディタ入力表](../analysis/oak-photo-link-input.md)の285バイトを入力する。入力後は`DA00`を実行し、エディタを終了する。詳しくは[対応ROMとGame Boy側の前提](supported-rom.md)を参照する。

## 8. 通信ケーブルを配線する

Game Boy、XIAO、電池、USBをすべて外した状態で作業する。必要な信号は次のとおり。

| Game Boy端子 | 信号 | XIAO |
|---:|---|---|
| 2 | SOUT | レベル変換を介してD1 / GPIO2 |
| 3 | SIN | レベル変換を介してD3 / GPIO4 |
| 5 | SCK | レベル変換を介してD0 / GPIO1 |
| 6 | GND | GND |
| 1 | VCC | 必要な場合だけレベルシフタのHV電源 |

Game Boy端子1からXIAO本体へ給電しない。XIAOはUSBまたはBAT端子の1セルLiPoから給電する。

電源投入前にテスターで次を確認する。

- SOUT、SIN、SCKが互いに短絡していない
- 各信号とGND、VCCが短絡していない
- より線の素線が隣のパッドへ触れていない
- Game Boy端子6、レベルシフタGND、XIAO GNDが導通する

## 9. 総合試験する

1. XIAOへ電源を入れ、LEDが2秒ごとに短く1回点灯するまで待つ。
2. Game Boyへ通信ケーブルを接続する。
3. オーキド研究所で対象のオーキドへ話しかける。
4. 「ふむふむ なるほど…」が表示されたとき、XIAOのLEDが処理中表示へ変わることを確認する。
5. Wi-Fi接続後はLEDが短く2回点滅する。
6. 講評生成後はLEDが点灯し、Game Boyが本文を受信すると待機表示へ戻る。
7. Game Boyに成功台詞とAI講評が表示されることを確認する。

AI講評は15～36表示文字で、18文字目の後へ改行が入る。Game Boy側の通信本文は最大63バイトである。

## 10. 成功条件

- XIAO単体の`c`試験が成功する
- 起動時の文字正規化自己診断が`pass`
- Game Boyの撮影要求時にUSBログへ`Game Boy requested a capture`が出る
- AI回答の`unsupported`が0
- Game Boyへ最大2行の講評が表示される
- 次の撮影でも前回の結果を使わず、新しく撮影する

ここまで確認できれば基本導入は完了である。通信の仕組みを調べる場合は[アーキテクチャ](architecture.md)、Game Boyコードを読む場合は[プログラム解説書](../analysis/oak-photo-link-program-guide.md)へ進む。
