2025-12-12

# 最終テスト結果（時間窓60秒、エントリタイムスタンプ基準）

## テスト実施日時
2025-12-12 08:20-08:24頃

## テスト手順

### 1. トラフィック生成
- node2でファイル作成: `/sample2.txt`
- node2でアドバタイズ: `nlsrc advertise /sample2.txt`
- node2でData発行: `ndnputchunks /sample2.txt < /sample2.txt`
- node3から取得: `ndncatchunks /relay/sample2.txt`

### 2. 結果

#### ✓ トラフィック生成成功
```
Hello, world
All segments have been received.
Time elapsed: 0.00762 seconds
```

#### ✓ Sidecarログ
**node2のsidecarログ:**
- エントリ数: 3行
- 最新エントリ: `"in_time": "2025-12-12 08:24:07.073347", "out_time": "2025-12-12 08:24:07.404796"`
- 処理時間: 約0.33秒

#### ✓ 利用率計算（node2）
```
Parsed 1 entries within time window (total lines: 3, total entries: 3)
Calculated utilization: 0.00552415 (totalProcessingTime=0.331449s, windowSeconds=60, validEntries=1)
ServiceFunctionInfo: utilization=0.00552415
```

**分析:**
- ✓ 60秒の時間窓でエントリが取得できている
- ✓ エントリのタイムスタンプ基準で時間窓が設定されている
- ✓ 利用率が正しく計算されている（0.00552415 = 0.331449秒 / 60秒）

#### ✓ NameLSAへの反映（node2）
```
wireEncode: Encoding Service Function: /relay, processingTime=0.00552415, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
```

**分析:**
- ✓ Service Function情報がNameLSAに含まれている
- ✓ `processingTime=0.00552415`が正しく設定されている
- ✓ `processingWeight=100`が正しく設定されている

#### ✓ node3での受信とFunctionCost計算
```
wireDecode: Stored Service Function info: /relay -> processingTime=0.00552415, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
FunctionCost calculated for /relay prefix to /ndn/jp/%C1.Router/node2: processingTime=0.00552415, functionCost=0.552415
Adjusting cost for /relay to /ndn/jp/%C1.Router/node2: originalCost=25, nexthopCost=0, functionCost=0.552415, adjustedCost=25.5524
Adjusting cost for /relay to /ndn/jp/%C1.Router/node1: originalCost=50, nexthopCost=0, functionCost=0, adjustedCost=50
```

**分析:**
- ✓ node3がService Function情報を受信している
- ✓ node2へのFunctionCost: `0.552415` (processingTime=0.00552415 * processingWeight=100)
- ✓ node1へのFunctionCost: `0` (node1にはService Function情報がないため)
- ✓ 各`destRouterName`に対して個別にFunctionCostを計算している

#### ✓ FIBエントリ（node3）
```
/relay nexthops={faceid=262 (cost=25552), faceid=264 (cost=50000)}
```

**分析:**
- ✓ cost=25552 = 25.5524 * 1000（FunctionCostが適用されている）
- ✓ cost=50000 = 50.0 * 1000（node1経由、FunctionCost=0）
- ✓ FunctionCostが正しくFIBに反映されている

## 確認できたこと

### 1. ✓ エントリのタイムスタンプ基準の時間窓
- 最新のエントリのタイムスタンプを基準に時間窓が設定されている
- 60秒の時間窓でエントリが取得できている
- エントリのタイムスタンプとファンクション利用率計算に使われる時刻が同じ

### 2. ✓ 利用率計算
- `totalProcessingTime=0.331449秒`が正しく計算されている
- `utilization=0.00552415` (0.331449秒 / 60秒)が正しく計算されている
- 時間窓内のエントリが正しく取得できている

### 3. ✓ Service Function情報の伝播
- node2のNameLSAにService Function情報が含まれている
- node3がService Function情報を受信している
- `processingWeight=100`が正しく伝播されている

### 4. ✓ FunctionCost計算
- node2へのFunctionCost: `0.552415` (正しく計算されている)
- node1へのFunctionCost: `0` (Service Function情報がないため)
- 各`destRouterName`に対して個別にFunctionCostを計算している

### 5. ✓ FIBエントリへの反映
- FunctionCostが正しくFIBエントリに反映されている
- cost=25552 (25.5524 * 1000)が正しく設定されている

## 修正内容の確認

### ✓ 時間窓の設定方法
- エントリのタイムスタンプを基準に時間窓が設定されている
- 最新のエントリのタイムスタンプから60秒前までのエントリを取得

### ✓ 60秒の時間窓
- 設定ファイル: `utilization-window-seconds  60`
- より長い時間窓により、より多くのエントリが含まれる

### ✓ 古い情報の処理
- 最新のエントリのタイムスタンプを`lastUpdateTime`に設定
- 一定時間以上経過している場合の処理は、今回のテストでは確認できなかった（エントリが新しいため）

## 結論

すべての修正が正しく動作していることが確認できました：

1. ✓ エントリのタイムスタンプ基準で時間窓が設定されている
2. ✓ 60秒の時間窓でエントリが取得できている
3. ✓ 利用率が正しく計算されている
4. ✓ Service Function情報が正しく伝播されている
5. ✓ FunctionCostが正しく計算されている
6. ✓ FunctionCostが正しくFIBエントリに反映されている

修正は成功しています！

