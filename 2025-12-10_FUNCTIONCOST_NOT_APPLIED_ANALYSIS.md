# FunctionCostが適用されない問題の分析

## 問題の概要

`processing-weight`を100に調整した後、利用率計算は正常に動作し、NameLSAにもService Function情報が正しく設定されていますが、FunctionCostがルーティングコストに適用されていません。

## テスト結果の詳細

### 1. 利用率計算 ✓

**node2のNLSRログ:**
```
Calculated utilization: 0.0260789 (totalProcessingTime=0.0260789s, windowSeconds=1, validEntries=1)
Updated NameLSA with Service Function info: prefix=/relay, processingTime=0.0260789, load=0, usageCount=0
```

**結果:** 利用率計算は正常に動作しています。

### 2. NameLSA更新 ✓

**node2のNLSRログ:**
```
wireEncode: Encoding 1 Service Function info entries
wireEncode: Encoding Service Function: /relay, processingTime=0.0260789, load=0, usageCount=0
```

**結果:** NameLSAにService Function情報が正しく設定され、エンコードされています。

### 3. NameLSA伝播 ✓

**node3のNLSRログ:**
```
wireDecode: Decoded ProcessingTime: 0.0260789 (value_size=8)
wireDecode: Stored Service Function info: /relay -> processingTime=0.0260789, load=0, usageCount=0
Service Function info updated for /relay: processingTime=0.0260789, load=0, usageCount=0
```

**結果:** node3はnode2のNameLSAを正しく受信し、Service Function情報を正しくデコードしています。

### 4. FunctionCost計算 ✗

**node3のNLSRログ（adjustNexthopCosts呼び出し時）:**
```
adjustNexthopCosts called: nameToCheck=/relay, destRouterName=/ndn/jp/%C1.Router/node2
NameLSA found for /ndn/jp/%C1.Router/node2
NameLSA Service Function info map size: 0
Service Function info NOT found for /relay in map
No Service Function info in NameLSA for /relay (all values are zero)
Adjusting cost for /relay to /ndn/jp/%C1.Router/node2: originalCost=25, nexthopCost=0, functionCost=0, adjustedCost=25
```

**問題点:**
- `adjustNexthopCosts`が呼ばれた時点（1765389229）では、node2のNameLSAにService Function情報がまだ含まれていませんでした
- その後、node2がNameLSAを更新した（1765389382）が、その時点で`adjustNexthopCosts`が呼ばれていません

## 問題の原因

### タイミングの問題

1. **初期ルーティング計算時（1765389229）:**
   - `adjustNexthopCosts`が呼ばれた
   - この時点では、node2のNameLSAにService Function情報がまだ含まれていない（map size: 0）
   - FunctionCost = 0として計算された

2. **NameLSA更新時（1765389382）:**
   - node2がNameLSAを更新し、Service Function情報を含めた
   - node3が更新されたNameLSAを受信し、Service Function情報を正しくデコードした
   - **しかし、ルーティング計算が再実行されていない**

### 根本原因

**NameLSAが更新された後、ルーティング計算が再実行されていない**

現在の実装では、NameLSAが更新された際に、ルーティング計算をスケジュールする処理が不足している可能性があります。

## 解決策

### 1. NameLSA更新時のルーティング計算スケジュール

NameLSAが更新された際に、ルーティング計算をスケジュールする必要があります。

**実装場所:**
- `Lsdb::installLsa()` - LSAがインストールされた際
- `NameLsa::update()` - NameLSAが更新された際

**確認が必要なコード:**
- `src/lsdb.cpp` - `installLsa()`メソッド
- `src/lsa/name-lsa.cpp` - `update()`メソッド
- `src/route/routing-table.cpp` - ルーティング計算のスケジュール

### 2. ルーティング計算のタイミング

現在の実装では、以下のタイミングでルーティング計算が実行されます：
- Adjacency LSA更新時
- Coordinate LSA更新時（Hyperbolic Routing使用時）
- **NameLSA更新時（確認が必要）**

### 3. デバッグログの追加

ルーティング計算がスケジュールされた際のログを追加し、NameLSA更新時にルーティング計算が実行されているか確認します。

## 期待される動作（修正後）

### 1. NameLSA更新時

1. node2がNameLSAを更新（Service Function情報を含む）
2. node3が更新されたNameLSAを受信
3. **ルーティング計算がスケジュールされる**
4. `adjustNexthopCosts`が呼ばれる
5. node2のNameLSAからService Function情報を取得
6. FunctionCostを計算（例: 0.0260789 * 100 = 2.6）
7. 最終コストを計算（例: 25 + 2.6 = 27.6）
8. FIBが更新される

### 2. ルーティングコストの更新

**修正前:**
```
/relay nexthops={faceid=262 (cost=25), faceid=264 (cost=50)}
```

**修正後（利用率2.6%の場合）:**
```
/relay nexthops={faceid=262 (cost=27.6), faceid=264 (cost=50)}
```

**修正後（利用率30%の場合）:**
```
/relay nexthops={faceid=262 (cost=55), faceid=264 (cost=50)}
→ node1の方が低コスト → トラフィックがnode1にルーティング
```

## 次のステップ

1. **コード確認**
   - `Lsdb::installLsa()`でNameLSA更新時にルーティング計算がスケジュールされているか確認
   - `NameLsa::update()`でService Function情報が更新された際の処理を確認

2. **修正実装**
   - NameLSA更新時にルーティング計算をスケジュールする処理を追加
   - デバッグログを追加して動作を確認

3. **再テスト**
   - 修正後の動作を確認
   - FunctionCostが正しく適用されているか確認
   - トラフィック分散が機能しているか確認

