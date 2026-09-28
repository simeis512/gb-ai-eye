const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const maxCaptionBytes = 63;
const captionBufferBytes = maxCaptionBytes + 1;
const textSource = fs.readFileSync(path.join(root, 'src/gameboy_text.cpp'), 'utf8');
const glyphs = new Map();
for (const match of textSource.matchAll(/\{0x([0-9a-f]+), 0x([0-9a-f]+)\}/gi)) {
  const codePoint = Number.parseInt(match[1], 16);
  if (!glyphs.has(codePoint)) glyphs.set(codePoint, Number.parseInt(match[2], 16));
}
const punctuation = new Map([
  [' ', 0x7f], ['　', 0x7f], ['！', 0xe7], ['？', 0xe6],
  ['。', 0xe8], ['…', 0x75], ['ー', 0xe3],
]);

function encodeText(text) {
  return [...text].map((character) => {
    if (punctuation.has(character)) return punctuation.get(character);
    const value = glyphs.get(character.codePointAt(0));
    if (value === undefined) throw new Error(`No Game Boy glyph for ${character}`);
    return value;
  });
}

const segments = [];
const labels = new Map();
const fixups = [];
let current;

function startSegment(name, origin) {
  current = {name, origin, bytes: [], lines: []};
  segments.push(current);
}
function label(name) {
  if (labels.has(name)) throw new Error(`Duplicate label: ${name}`);
  labels.set(name, current.origin + current.bytes.length);
  current.lines.push({offset: current.bytes.length, count: 0, source: `${name}:`});
}
function absoluteLabel(name, address) {
  if (labels.has(name)) throw new Error(`Duplicate label: ${name}`);
  labels.set(name, address);
}
function emit(bytes, source) {
  const values = Array.isArray(bytes) ? bytes : [...arguments].filter((v) => Number.isInteger(v));
  const offset = current.bytes.length;
  current.bytes.push(...values);
  if (source) current.lines.push({offset, count: values.length, source});
}
function emitLabelWord(name, source) {
  const offset = current.bytes.length;
  current.bytes.push(0, 0);
  fixups.push({type: 'word', segment: current, offset, label: name});
  if (source) current.lines.push({offset, count: 2, source});
}
function call(name) {
  const offset = current.bytes.length;
  current.bytes.push(0xcd, 0, 0);
  fixups.push({type: 'word', segment: current, offset: offset + 1, label: name});
  current.lines.push({offset, count: 3, source: `    call ${name}`});
}
function loadHl(name) {
  const offset = current.bytes.length;
  current.bytes.push(0x21, 0, 0);
  fixups.push({type: 'word', segment: current, offset: offset + 1, label: name});
  current.lines.push({offset, count: 3, source: `    ld hl, ${name}`});
}
function jump(name) {
  const offset = current.bytes.length;
  current.bytes.push(0xc3, 0, 0);
  fixups.push({type: 'word', segment: current, offset: offset + 1, label: name});
  current.lines.push({offset, count: 3, source: `    jp ${name}`});
}
function relative(opcode, name, source) {
  const offset = current.bytes.length;
  current.bytes.push(opcode, 0);
  fixups.push({type: 'relative', segment: current, offset: offset + 1, label: name});
  current.lines.push({offset, count: 2, source});
}
function text(bytes, source) {
  emit(bytes, `    db ${source}`);
}

startSegment('installer', 0xda00);
label('OakTextInstaller');
emit([0x21, 0xeb, 0xd2], '    ld hl, $D2EB');
emit([0x3e, 0x80], '    ld a, LOW(OakLabTextPointers)');
emit([0x22], '    ld [hli], a');
emit([0x36, 0xda], '    ld [hl], HIGH(OakLabTextPointers)');
emit([0xc9], '    ret');

startSegment('pointer-table', 0xda80);
label('OakLabTextPointers');
emit([0x0c, 0x4b, 0x9b, 0x4b, 0xac, 0x4b, 0xbd, 0x4b],
     '    dw $4B0C, $4B9B, $4BAC, $4BBD');
emit([0xa0, 0xda], '    dw OakPhotoText');

startSegment('opening-text', 0xdaa0);
label('OakPhotoText');
emit([0x08], '    db $08 ; execute inline setup');
emit([0x3e, 0x01, 0xea, 0xea, 0xd0],
     '    ld a, $01\n    ld [$D0EA], a ; enable automatic text-box drawing');
emit([0xcd, 0x1e, 0x31], '    call $311E ; draw the standard dialogue box');
emit([0xcd, 0xab, 0x0e], '    call $0EAB ; update sprites');
emit([0xcd, 0xf5, 0x3d], '    call $3DF5 ; delay 3 frames');
emit([0x01, 0xb9, 0xc4], '    ld bc, $C4B9 ; standard text origin');
loadHl('OakPhotoTextBody');
emit([0xc9], '    ret ; resume the text-command processor');
label('OakPhotoTextBody');
emit([0x00], '    db $00 ; inline text');
text(encodeText('おお！ しゃしんを'), '"おお！ しゃしんを"');
emit([0x4f], '    db $4F ; line');
text(encodeText('みせて くれるのか？'), '"みせて くれるのか？"');
emit([0x51], '    db $51 ; paragraph and wait');
text(encodeText('ふむふむ なるほど…'), '"ふむふむ なるほど…"');
emit([0x50, 0x08], '    db $50, $08 ; end string, execute inline code');
jump('OakPhotoHook');

startSegment('serial-code', 0xdb00);
label('OakPhotoHook');
emit([0x3e, 0x43], '    ld a, $43 ; CAPTURE');
call('SerialByte');
relative(0x38, 'Failure', '    jr c, Failure');
emit([0xfe, 0x15], '    cp $15 ; NAK');
relative(0x28, 'Failure', '    jr z, Failure');
emit([0xfe, 0xff], '    cp $FF ; disconnected link');
relative(0x28, 'Failure', '    jr z, Failure');
emit([0x11, 0x10, 0x0e], '    ld de, $0E10 ; about 150 seconds');
label('PollStatus');
call('PollDelay');
emit([0xaf], '    xor a ; dummy byte');
call('SerialByte');
relative(0x38, 'Failure', '    jr c, Failure');
emit([0xfe, 0x15], '    cp $15 ; NAK');
relative(0x28, 'Failure', '    jr z, Failure');
emit([0xfe, 0xff], '    cp $FF ; disconnected link');
relative(0x28, 'Failure', '    jr z, Failure');
emit([0xfe, 0x17], '    cp $17 ; BUSY');
relative(0x28, 'WaitMore', '    jr z, WaitMore');
emit([0xfe, 0x06], '    cp $06 ; ACK, request accepted');
relative(0x28, 'WaitMore', '    jr z, WaitMore');
emit([0x4f], '    ld c, a');
emit([0xe6, 0xc0], '    and $C0');
emit([0xfe, 0x80], '    cp $80 ; data header');
relative(0x20, 'Failure', '    jr nz, Failure');
emit([0x79], '    ld a, c');
emit([0xe6, 0x3f], '    and $3F ; caption length');
relative(0x28, 'Failure', '    jr z, Failure');
emit([0xfe, maxCaptionBytes + 1], `    cp ${maxCaptionBytes + 1}`);
relative(0x30, 'Failure', '    jr nc, Failure');
emit([0x4f], '    ld c, a');
loadHl('CaptionBuffer');
emit([0xaf, 0x57], '    xor a\n    ld d, a ; checksum');
label('ReceiveCaption');
emit([0xaf], '    xor a');
call('SerialByte');
relative(0x38, 'Failure', '    jr c, Failure');
emit([0x22], '    ld [hli], a');
emit([0xaa, 0x57], '    xor d\n    ld d, a');
emit([0x0d], '    dec c');
relative(0x20, 'ReceiveCaption', '    jr nz, ReceiveCaption');
emit([0xaf], '    xor a');
call('SerialByte');
relative(0x38, 'Failure', '    jr c, Failure');
emit([0xba], '    cp d');
relative(0x20, 'Failure', '    jr nz, Failure');
emit([0x36, 0x57], '    ld [hl], $57 ; done text');
loadHl('ResultScript');
emit([0xc9], '    ret');
label('WaitMore');
emit([0x1b, 0x7a, 0xb3], '    dec de\n    ld a, d\n    or e');
relative(0x20, 'PollStatus', '    jr nz, PollStatus');
label('Failure');
loadHl('FailureScript');
emit([0xc9], '    ret');

label('SerialByte');
emit([0xf3], '    di');
emit([0xe0, 0x01], '    ldh [$01], a ; SB');
emit([0x3e, 0x81], '    ld a, $81');
emit([0xe0, 0x02], '    ldh [$02], a ; SC, internal clock');
emit([0x06, 0xff], '    ld b, $FF');
label('SerialWait');
emit([0xf0, 0x02, 0xcb, 0x7f], '    ldh a, [$02]\n    bit 7, a');
relative(0x28, 'SerialDone', '    jr z, SerialDone');
emit([0x05], '    dec b');
relative(0x20, 'SerialWait', '    jr nz, SerialWait');
emit([0x3e, 0x01, 0xe0, 0x02], '    ld a, $01\n    ldh [$02], a ; abort timed-out transfer');
emit([0xf0, 0x0f, 0xcb, 0x9f, 0xe0, 0x0f],
     '    ldh a, [$0F]\n    res 3, a\n    ldh [$0F], a');
emit([0x37, 0xfb, 0xc9], '    scf\n    ei\n    ret');
label('SerialDone');
emit([0xf0, 0x01, 0xf5], '    ldh a, [$01]\n    push af');
emit([0xf0, 0x0f, 0xcb, 0x9f, 0xe0, 0x0f],
     '    ldh a, [$0F]\n    res 3, a\n    ldh [$0F], a');
emit([0x06, 0x20], '    ld b, $20 ; let the link peer prepare its next byte');
label('SerialSettle');
emit([0x05], '    dec b');
relative(0x20, 'SerialSettle', '    jr nz, SerialSettle');
emit([0xf1, 0xa7, 0xfb, 0xc9], '    pop af\n    and a\n    ei\n    ret');

label('PollDelay');
emit([0xc5, 0x01, 0x00, 0x18], '    push bc\n    ld bc, $1800');
label('PollDelayLoop');
emit([0x0b, 0x78, 0xb1], '    dec bc\n    ld a, b\n    or c');
relative(0x20, 'PollDelayLoop', '    jr nz, PollDelayLoop');
emit([0xc1, 0xc9], '    pop bc\n    ret');

startSegment('failure-text', 0xdc20);
label('FailureScript');
emit([0x00, 0x51], '    db $00, $51 ; new paragraph');
text(encodeText('むむ？ しゃしんが'), '"むむ？ しゃしんが"');
emit([0x4f], '    db $4F ; line');
text(encodeText('うまく みえんようじゃ…'), '"うまく みえんようじゃ…"');
emit([0x57], '    db $57 ; done');

startSegment('result-prefix', 0xdc50);
label('ResultScript');
emit([0x00, 0x51], '    db $00, $51 ; new paragraph');
text(encodeText('うむ！ これは'), '"うむ！ これは"');
emit([0x4f], '    db $4F ; line');
text(encodeText('いい しゃしんじゃ！'), '"いい しゃしんじゃ！"');
emit([0x51], '    db $51 ; show caption on the next page');
absoluteLabel('CaptionBuffer', current.origin + current.bytes.length);

for (const fixup of fixups) {
  const target = labels.get(fixup.label);
  if (target === undefined) throw new Error(`Unknown label: ${fixup.label}`);
  if (fixup.type === 'word') {
    fixup.segment.bytes[fixup.offset] = target & 0xff;
    fixup.segment.bytes[fixup.offset + 1] = target >>> 8;
  } else {
    const sourceAfter = fixup.segment.origin + fixup.offset + 1;
    const displacement = target - sourceAfter;
    if (displacement < -128 || displacement > 127) {
      throw new Error(`Relative jump to ${fixup.label} is out of range: ${displacement}`);
    }
    fixup.segment.bytes[fixup.offset] = displacement & 0xff;
  }
}

for (const segment of segments) {
  if (segment.origin < 0xc000 || segment.origin + segment.bytes.length > 0xdce0) {
    throw new Error(`${segment.name} overlaps the binary editor or leaves WRAM`);
  }
}
const sorted = [...segments].sort((a, b) => a.origin - b.origin);
for (let i = 1; i < sorted.length; ++i) {
  if (sorted[i - 1].origin + sorted[i - 1].bytes.length > sorted[i].origin) {
    throw new Error(`${sorted[i - 1].name} overlaps ${sorted[i].name}`);
  }
}
if (labels.get('CaptionBuffer') + captionBufferBytes > 0xdce0) {
  throw new Error('Caption buffer overlaps the installed binary editor');
}

const hex = (value, width = 2) => value.toString(16).toUpperCase().padStart(width, '0');
const patches = sorted.map((segment) => ({
  address: hex(segment.origin, 4),
  hex: segment.bytes.map((value) => hex(value)).join(' '),
  purpose: segment.name,
}));

const byteRows = [];
for (const segment of sorted) {
  segment.bytes.forEach((value, index) => byteRows.push({
    address: hex(segment.origin + index, 4), value: hex(value), part: segment.name,
  }));
}
const csv = ['address,value,part', ...byteRows.map((row) =>
  `${row.address},${row.value},${row.part}`)].join('\r\n') + '\r\n';

const groupedRows = [];
for (let index = 0; index < byteRows.length; index += 8) {
  const rows = byteRows.slice(index, index + 8);
  groupedRows.push(`| ${rows.map((row) => `\`${row.address}:${row.value}\``).join(' | ')} |`);
}
const blockRows = patches.map((entry) => {
  const count = entry.hex.split(' ').length;
  return `| \`${entry.address}\` | ${count} | \`${entry.hex}\` | ${entry.purpose} |`;
});
const markdown = `# オーキド写真講評版・バイナリエディタ入力表

対象はワークスペースの日本版『ポケットモンスター 赤』revision 1と、導入済みのSELECT単独実行型バイナリエディタです。

## 入力ブロック

| 開始アドレス | バイト数 | 値 | 用途 |
|---|---:|---|---|
${blockRows.join('\n')}

合計 ${byteRows.length} バイトです。\`${hex(labels.get('CaptionBuffer'), 4)}\`から${captionBufferBytes}バイトは、最大${maxCaptionBytes}バイトの本文と終了コードを置く受信バッファとして実行時に書かれるため、入力不要です。

各シリアルバイトの後には短い待ち時間を設け、XIAOが次の送信バイトを確実に準備できるようにしている。

## 最大48バイト版からの更新

すでに285バイトの最大48バイト版を入力済みの場合は、次の1バイトだけ変更すればよい。

| アドレス | 旧値 | 新値 | 意味 |
|---|---:|---:|---|
| \`DB38\` | \`31\` | \`40\` | 本文長の上限判定を49未満から64未満へ変更 |

## 旧280バイト版からの更新

コード末尾が\`DB9B\`だった旧280バイト版を入力済みの場合は、全体を入れ直さず、次の22バイトだけを変更・追加すればよい。

| 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|
| \`DB13:96\` | \`DB38:40\` | \`DB8D:06\` | \`DB8E:20\` | \`DB8F:05\` | \`DB90:20\` | \`DB91:FD\` |
| \`DB92:F1\` | \`DB93:A7\` | \`DB94:FB\` | \`DB95:C9\` | \`DB96:C5\` | \`DB97:01\` | \`DB98:00\` |
| \`DB99:18\` | \`DB9A:0B\` | \`DB9B:78\` | \`DB9C:B1\` | \`DB9D:20\` | \`DB9E:FB\` | \`DB9F:C1\` |
| \`DBA0:C9\` |  |  |  |  |  |  |

## 1バイト単位の一覧

左がアドレス、右が入力値です。同じ内容をCSV形式でも \`oak-photo-link-byte-list.csv\` に保存しています。

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|
${groupedRows.join('\n')}

## 実行手順

1. オーキド研究所の中で、なかよしバッヂからバイナリエディタを開く。
2. 上記の全ブロックを入力する。
3. カーソルを\`DA00\`へ移動する。
4. SELECT単独で\`DA00\`を実行する。
5. STARTでエディタを終了し、Bでメニューを閉じる。
6. XIAOをGame Boyへ接続してからオーキドに話しかける。

オーキドは撮影前に「おお！ しゃしんを みせて くれるのか？」と話し、撮影結果を待ちながら「ふむふむ なるほど…」と表示する。成功時は写真を褒めてからAIの講評を表示し、通信失敗時は自然な失敗台詞へ移る。
`;

const listing = [];
listing.push('; Generated by tools/build-oak-photo-payload.cjs');
listing.push(`; CaptionBuffer = $${hex(labels.get('CaptionBuffer'), 4)}, maximum ${maxCaptionBytes} bytes + $57`);
for (const segment of sorted) {
  listing.push('', `; ${segment.name}, $${hex(segment.origin, 4)}`);
  for (const line of segment.lines) {
    const address = segment.origin + line.offset;
    if (line.count === 0) {
      listing.push(`${line.source} ; $${hex(address, 4)}`);
    } else {
      const bytes = segment.bytes.slice(line.offset, line.offset + line.count)
        .map((value) => hex(value)).join(' ');
      listing.push(`${hex(address, 4)}: ${bytes.padEnd(24)} ; ${line.source.trim()}`);
    }
  }
}
listing.push('');

const result = {
  patches: JSON.stringify(patches, null, 2) + '\n',
  csv,
  markdown,
  listing: listing.join('\n'),
  metadata: {
    totalBytes: byteRows.length,
    codeEnd: hex(labels.get('PollDelayLoop') + 7, 4),
    captionBuffer: hex(labels.get('CaptionBuffer'), 4),
    maxCaptionBytes,
  },
};

if (process.argv.includes('--write')) {
  fs.writeFileSync(path.join(root, 'analysis/oak-photo-link-patches.json'), result.patches);
  fs.writeFileSync(path.join(root, 'analysis/oak-photo-link-byte-list.csv'), result.csv);
  fs.writeFileSync(path.join(root, 'analysis/oak-photo-link-input.md'), result.markdown);
  fs.writeFileSync(path.join(root, 'payloads/oak-photo-link.asm'), result.listing);
  process.stdout.write(JSON.stringify(result.metadata, null, 2) + '\n');
} else {
  process.stdout.write(JSON.stringify(result));
}
