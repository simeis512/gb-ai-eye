# トラブルシューティング

## 最初に確認する順序

1. Game Boyを外し、XIAO単体の`c`撮影を試す。
2. USBシリアルログでカメラ、Wi-Fi、AI、文字変換のどこまで進んだか確認する。
3. 全電源を外して通信ケーブルの導通と短絡を確認する。
4. レベルシフタのLV、HV、GND電源を確認する。
5. Game Boy接続時の`last`、`resyncs`、`edgeFaults`を確認する。

## LED表示

| LED | 状態 | 次に確認すること |
|---|---|---|
| 2秒ごとに短く1回 | 待機中 | 正常。撮影要求を待っている |
| 0.2秒間隔 | カメラ起動またはWi-Fi接続中 | 長時間続くならSSID、パスワード、2.4 GHzを確認 |
| 短く2回 | Wi-Fi接続済み、AI応答待ち | APIキー、残高、モデル、回線を確認 |
| 点灯したまま | 講評生成済み、Game Boy受信待ち | SCKとXIAO→GBのSIN経路を確認 |
| 短く3回 | 設定、撮影、通信、APIエラー | USBシリアルの`Capture failed`を確認 |

## 症状別

### XIAO単体で`c`を送っても動かない

- シリアルモニターが115200 bpsか確認する。
- `Edit include/secrets.h before using capture requests`なら`secrets.h`を作成する。
- 3回点滅なら`Capture failed:`の後の理由を読む。
- カメラ初期化エラーならSense拡張ボードとカメラのフレキケーブルを挿し直す。

### Wi-Fi接続中の点滅から進まない

- 2.4 GHzのSSIDを使用する。
- SSIDとパスワードの全角・半角、末尾空白を確認する。
- USBログの`Wi-Fi connection timed out`を確認する。
- 電波が弱い場合はアクセスポイントへ近づける。

### Wi-Fi接続後、AI待ちで失敗する

- 選択したプロバイダのAPIキーを設定しているか確認する。
- OpenAIのクレジット残高と利用上限、Geminiの割当を確認する。
- `HTTP 401`はキー、`HTTP 429`は割当やレート制限、`credit_balance_exhausted`は残高を確認する。
- 設定したモデルが画像入力に対応し、APIアカウントから利用できるか確認する。

### 「ふむふむ なるほど…」になってもLEDが待機表示のまま

XIAOがGame Boyの`43`を認識していない。

- GB端子2 SOUTからXIAO D1までの経路を確認する。
- レベルシフタのLV/VCCAへ3V3、HV/VCCBへ必要な電源が入っているか確認する。
- SOUTとSINの短絡を確認する。
- 加工ケーブルでは色ではなくプラグ端子との導通で特定する。

USBログで`Game Boy requested a capture`が出なければ、この経路を重点的に調べる。

### LEDは処理中になるがGame Boyへ結果が出ない

- USBログにAI回答と`Game Boy caption`が出ているか確認する。
- LEDが点灯したままなら、Game BoyのSCKとXIAO D3からGB端子3 SINまでを確認する。
- `edgeFaults`が増える場合はクロック線、GND、レベル変換、長い配線を確認する。
- `resyncs`が撮影ごとに少数増えること自体より、転送中に連続して増える状態を調べる。

### 長く待った後に会話へ戻る

Game Boy側が有効なデータヘッダーを受け取れず、状態待ちが時間切れになった可能性がある。XIAOのLEDとUSBログを確認し、XIAOが撮影要求を受けたか、AI処理が成功したか、Game Boyへフレームを返したかの順に切り分ける。

### 2回目以降に待機が飛ばされる

最新のXIAOファームウェアへ更新する。現在の実装は新しい`43`を受信したバイト境界で前回フレームを破棄し、最低4秒`BUSY`を維持する。古いファームウェアでは前回のデータヘッダーを新しい結果と誤認する場合があった。

### 「シシシシ…」など同じ文字が続く

通信バイト境界が崩れたか、SOUTとSINが短絡している可能性が高い。

- 全電源を外し、LV側とHV側の隣接チャンネル間を導通検査する。
- より線の素線が隣へ触れていないか拡大確認する。
- `edgeFaults`と`resyncs`を確認する。
- 最新ファームウェアの欠落エッジ検出を使用する。

### 濁点・半濁点がずれる、改行位置がおかしい

- 起動ログが`Game Boy text normalization: pass`か確認する。
- 最新のXIAOファームウェアを使用する。
- 最大63バイト対応のGame Boy側更新として`DB38:40`になっているか確認する。
- AI回答は合成済み文字と結合文字のどちらでもXIAO側で正規化される。

### `ロ`が`ワ`、`ル`が`レ`に見える

古い文字コード表を使用している可能性がある。最新版の`src/gameboy_text.cpp`を含むファームウェアを書き込む。現在の対応はカタカナの`ル=A6`、`レ=A7`、`ロ=A8`、`ワ=A9`である。

## USBログの見方

5秒ごとの状態行には次が含まれる。

```text
Link transfers=... overflows=... resyncs=... edgeFaults=...
rx=... last=... rxValid=... unexpected=...
capture=... stream=... chip=...C
```

| 項目 | 意味 |
|---|---|
| `transfers` | 完了したGame Boyリンク転送数 |
| `overflows` | 受信キューがあふれた回数 |
| `resyncs` | 無通信区間後にバイト境界を取り直した回数 |
| `edgeFaults` | 同じクロックレベルを連続検出した回数 |
| `last` | 最後にGame Boyから受信した値。撮影要求は`43` |
| `capture` | 撮影・AIタスクが動作中か |
| `stream` | Game Boyへ本文フレームを送信中か |
| `chip` | ESP32-S3内部温度。表面温度ではなく変化の目安 |

配線と電源の詳しい測定方法は[電源・発熱・配線](power-and-wiring.md)を参照する。

## 診断ログを自動取得する

Python 3を使用できる場合は、補助ツール用依存関係をインストールする。

```powershell
python -m pip install -r requirements-tools.txt
```

COM5をリセットし、起動ログだけを8秒取得する例:

```powershell
python tools/Probe-XiaoCapture.py --port COM5 --seconds 8 --skip-capture
```

起動4秒後に`c`を送り、撮影診断を55秒取得する例:

```powershell
python tools/Probe-XiaoCapture.py --port COM5 --seconds 55
```
