# オーキド写真講評版・バイナリエディタ入力表

対象はワークスペースの日本版『ポケットモンスター 赤』revision 1と、導入済みのSELECT単独実行型バイナリエディタです。

## 入力ブロック

| 開始アドレス | バイト数 | 値 | 用途 |
|---|---:|---|---|
| `DA00` | 9 | `21 EB D2 3E 80 22 36 DA C9` | installer |
| `DA80` | 10 | `0C 4B 9B 4B AC 4B BD 4B A0 DA` | pointer-table |
| `DAA0` | 59 | `08 3E 01 EA EA D0 CD 1E 31 CD AB 0E CD F5 3D 01 B9 C4 21 B6 DA C9 00 B5 B5 E7 7F BC E0 BC DE DD 4F D0 BE C3 7F B8 DA D9 C9 B6 E6 51 CC D1 CC D1 7F C5 D9 CE 34 75 50 08 C3 00 DB` | opening-text |
| `DB00` | 161 | `3E 43 CD 65 DB 38 5A FE 15 28 56 FE FF 28 52 11 10 0E CD 96 DB AF CD 65 DB 38 46 FE 15 28 42 FE FF 28 3E FE 17 28 35 FE 06 28 31 4F E6 C0 FE 80 20 2F 79 E6 3F 28 2A FE 40 30 26 4F 21 65 DC AF 57 AF CD 65 DB 38 1A 22 AA 57 0D 20 F4 AF CD 65 DB 38 0E BA 20 0B 36 57 21 50 DC C9 1B 7A B3 20 B1 21 20 DC C9 F3 E0 01 3E 81 E0 02 06 FF F0 02 CB 7F 28 10 05 20 F7 3E 01 E0 02 F0 0F CB 9F E0 0F 37 FB C9 F0 01 F5 F0 0F CB 9F E0 0F 06 20 05 20 FD F1 A7 FB C9 C5 01 00 18 0B 78 B1 20 FB C1 C9` | serial-code |
| `DC20` | 25 | `00 51 D1 D1 E6 7F BC E0 BC DE 26 4F B3 CF B8 7F D0 B4 DE D6 B3 2C E0 75 57` | failure-text |
| `DC50` | 21 | `00 51 B3 D1 E7 7F BA DA CA 4F B2 B2 7F BC E0 BC DE 2C E0 E7 51` | result-prefix |

合計 285 バイトです。`DC65`から64バイトは、最大63バイトの本文と終了コードを置く受信バッファとして実行時に書かれるため、入力不要です。

各シリアルバイトの後には短い待ち時間を設け、XIAOが次の送信バイトを確実に準備できるようにしている。

## 最大48バイト版からの更新

すでに285バイトの最大48バイト版を入力済みの場合は、次の1バイトだけ変更すればよい。

| アドレス | 旧値 | 新値 | 意味 |
|---|---:|---:|---|
| `DB38` | `31` | `40` | 本文長の上限判定を49未満から64未満へ変更 |

## 旧280バイト版からの更新

コード末尾が`DB9B`だった旧280バイト版を入力済みの場合は、全体を入れ直さず、次の22バイトだけを変更・追加すればよい。

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|
| `DB13:96` | `DB38:40` | `DB8D:06` | `DB8E:20` | `DB8F:05` | `DB90:20` | `DB91:FD` |
| `DB92:F1` | `DB93:A7` | `DB94:FB` | `DB95:C9` | `DB96:C5` | `DB97:01` | `DB98:00` |
| `DB99:18` | `DB9A:0B` | `DB9B:78` | `DB9C:B1` | `DB9D:20` | `DB9E:FB` | `DB9F:C1` |
| `DBA0:C9` |  |  |  |  |  |  |

## 1バイト単位の一覧

左がアドレス、右が入力値です。同じ内容をCSV形式でも `oak-photo-link-byte-list.csv` に保存しています。

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|
| `DA00:21` | `DA01:EB` | `DA02:D2` | `DA03:3E` | `DA04:80` | `DA05:22` | `DA06:36` | `DA07:DA` |
| `DA08:C9` | `DA80:0C` | `DA81:4B` | `DA82:9B` | `DA83:4B` | `DA84:AC` | `DA85:4B` | `DA86:BD` |
| `DA87:4B` | `DA88:A0` | `DA89:DA` | `DAA0:08` | `DAA1:3E` | `DAA2:01` | `DAA3:EA` | `DAA4:EA` |
| `DAA5:D0` | `DAA6:CD` | `DAA7:1E` | `DAA8:31` | `DAA9:CD` | `DAAA:AB` | `DAAB:0E` | `DAAC:CD` |
| `DAAD:F5` | `DAAE:3D` | `DAAF:01` | `DAB0:B9` | `DAB1:C4` | `DAB2:21` | `DAB3:B6` | `DAB4:DA` |
| `DAB5:C9` | `DAB6:00` | `DAB7:B5` | `DAB8:B5` | `DAB9:E7` | `DABA:7F` | `DABB:BC` | `DABC:E0` |
| `DABD:BC` | `DABE:DE` | `DABF:DD` | `DAC0:4F` | `DAC1:D0` | `DAC2:BE` | `DAC3:C3` | `DAC4:7F` |
| `DAC5:B8` | `DAC6:DA` | `DAC7:D9` | `DAC8:C9` | `DAC9:B6` | `DACA:E6` | `DACB:51` | `DACC:CC` |
| `DACD:D1` | `DACE:CC` | `DACF:D1` | `DAD0:7F` | `DAD1:C5` | `DAD2:D9` | `DAD3:CE` | `DAD4:34` |
| `DAD5:75` | `DAD6:50` | `DAD7:08` | `DAD8:C3` | `DAD9:00` | `DADA:DB` | `DB00:3E` | `DB01:43` |
| `DB02:CD` | `DB03:65` | `DB04:DB` | `DB05:38` | `DB06:5A` | `DB07:FE` | `DB08:15` | `DB09:28` |
| `DB0A:56` | `DB0B:FE` | `DB0C:FF` | `DB0D:28` | `DB0E:52` | `DB0F:11` | `DB10:10` | `DB11:0E` |
| `DB12:CD` | `DB13:96` | `DB14:DB` | `DB15:AF` | `DB16:CD` | `DB17:65` | `DB18:DB` | `DB19:38` |
| `DB1A:46` | `DB1B:FE` | `DB1C:15` | `DB1D:28` | `DB1E:42` | `DB1F:FE` | `DB20:FF` | `DB21:28` |
| `DB22:3E` | `DB23:FE` | `DB24:17` | `DB25:28` | `DB26:35` | `DB27:FE` | `DB28:06` | `DB29:28` |
| `DB2A:31` | `DB2B:4F` | `DB2C:E6` | `DB2D:C0` | `DB2E:FE` | `DB2F:80` | `DB30:20` | `DB31:2F` |
| `DB32:79` | `DB33:E6` | `DB34:3F` | `DB35:28` | `DB36:2A` | `DB37:FE` | `DB38:40` | `DB39:30` |
| `DB3A:26` | `DB3B:4F` | `DB3C:21` | `DB3D:65` | `DB3E:DC` | `DB3F:AF` | `DB40:57` | `DB41:AF` |
| `DB42:CD` | `DB43:65` | `DB44:DB` | `DB45:38` | `DB46:1A` | `DB47:22` | `DB48:AA` | `DB49:57` |
| `DB4A:0D` | `DB4B:20` | `DB4C:F4` | `DB4D:AF` | `DB4E:CD` | `DB4F:65` | `DB50:DB` | `DB51:38` |
| `DB52:0E` | `DB53:BA` | `DB54:20` | `DB55:0B` | `DB56:36` | `DB57:57` | `DB58:21` | `DB59:50` |
| `DB5A:DC` | `DB5B:C9` | `DB5C:1B` | `DB5D:7A` | `DB5E:B3` | `DB5F:20` | `DB60:B1` | `DB61:21` |
| `DB62:20` | `DB63:DC` | `DB64:C9` | `DB65:F3` | `DB66:E0` | `DB67:01` | `DB68:3E` | `DB69:81` |
| `DB6A:E0` | `DB6B:02` | `DB6C:06` | `DB6D:FF` | `DB6E:F0` | `DB6F:02` | `DB70:CB` | `DB71:7F` |
| `DB72:28` | `DB73:10` | `DB74:05` | `DB75:20` | `DB76:F7` | `DB77:3E` | `DB78:01` | `DB79:E0` |
| `DB7A:02` | `DB7B:F0` | `DB7C:0F` | `DB7D:CB` | `DB7E:9F` | `DB7F:E0` | `DB80:0F` | `DB81:37` |
| `DB82:FB` | `DB83:C9` | `DB84:F0` | `DB85:01` | `DB86:F5` | `DB87:F0` | `DB88:0F` | `DB89:CB` |
| `DB8A:9F` | `DB8B:E0` | `DB8C:0F` | `DB8D:06` | `DB8E:20` | `DB8F:05` | `DB90:20` | `DB91:FD` |
| `DB92:F1` | `DB93:A7` | `DB94:FB` | `DB95:C9` | `DB96:C5` | `DB97:01` | `DB98:00` | `DB99:18` |
| `DB9A:0B` | `DB9B:78` | `DB9C:B1` | `DB9D:20` | `DB9E:FB` | `DB9F:C1` | `DBA0:C9` | `DC20:00` |
| `DC21:51` | `DC22:D1` | `DC23:D1` | `DC24:E6` | `DC25:7F` | `DC26:BC` | `DC27:E0` | `DC28:BC` |
| `DC29:DE` | `DC2A:26` | `DC2B:4F` | `DC2C:B3` | `DC2D:CF` | `DC2E:B8` | `DC2F:7F` | `DC30:D0` |
| `DC31:B4` | `DC32:DE` | `DC33:D6` | `DC34:B3` | `DC35:2C` | `DC36:E0` | `DC37:75` | `DC38:57` |
| `DC50:00` | `DC51:51` | `DC52:B3` | `DC53:D1` | `DC54:E7` | `DC55:7F` | `DC56:BA` | `DC57:DA` |
| `DC58:CA` | `DC59:4F` | `DC5A:B2` | `DC5B:B2` | `DC5C:7F` | `DC5D:BC` | `DC5E:E0` | `DC5F:BC` |
| `DC60:DE` | `DC61:2C` | `DC62:E0` | `DC63:E7` | `DC64:51` |

## 実行手順

1. オーキド研究所の中で、なかよしバッヂからバイナリエディタを開く。
2. 上記の全ブロックを入力する。
3. カーソルを`DA00`へ移動する。
4. SELECT単独で`DA00`を実行する。
5. STARTでエディタを終了し、Bでメニューを閉じる。
6. XIAOをGame Boyへ接続してからオーキドに話しかける。

オーキドは撮影前に「おお！ しゃしんを みせて くれるのか？」と話し、撮影結果を待ちながら「ふむふむ なるほど…」と表示する。成功時は写真を褒めてからAIの講評を表示し、通信失敗時は自然な失敗台詞へ移る。
