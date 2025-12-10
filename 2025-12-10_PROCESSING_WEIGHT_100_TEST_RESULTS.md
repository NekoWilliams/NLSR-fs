# processing-weight=100でのテスト結果

## テスト実施日時
2025年12月10日

## テスト目的
1. `processing-weight`を100に調整した後の動作確認
2. 利用率計算が正しく動作しているか確認
3. FunctionCostが正しく適用されているか確認
4. トラフィック分散が機能しているか確認

## テスト結果

### 1. ランダム待機の確認 ✓

**node2のsidecarログ:**
- 複数のリクエストが記録されている
- 処理時間が記録されている（例: 0.042552秒）

**結果:** ランダム待機は正常に機能しています。

### 2. 利用率計算の確認 ✓

**node2のNLSRログ:**
```
Calculated utilization: 0.085104 (totalProcessingTime=0.085104s, windowSeconds=1, validEntries=2)
ServiceFunctionInfo: utilization=0.085104 (calculated from 2 entries)
ServiceFunctionInfo: processingTime=0.085104, load=0, usageCount=0
```

**結果:** 利用率計算は正常に動作しています。時間窓（1秒）内の総処理時間を計算し、利用率を算出しています。

### 3. NameLSA更新の確認 ✓

**node2のNLSRログ:**
```
Updated NameLSA with Service Function info: prefix=/relay, processingTime=0.0260789, load=0, usageCount=0
wireEncode: Encoding 1 Service Function info entries
wireEncode: Encoding Service Function: /relay, processingTime=0.0260789, load=0, usageCount=0
```

**結果:** NameLSAにService Function情報が正しく設定され、エンコードされています。

### 4. FunctionCost適用の問題 ✗

**問題点:**
- FIBのコストはまだ25と50のまま（FunctionCostが適用されていない）
- node3のNLSRログからFunctionCost計算の詳細ログが見つからない

**期待される動作:**
- 利用率0.0260789（約2.6%）の場合: FunctionCost = 0.0260789 * 100 = 2.6
- リンクコスト25の場合: 最終コスト = 25 + 2.6 = 27.6

**実際の動作:**
- FIBのコスト: 25（FunctionCostが適用されていない）

### 5. トラフィック分散の確認 ✗

**node1のsidecarログ:**
- ファイルサイズ: 0バイト
- 行数: 0行

**node2のsidecarログ:**
- ファイルサイズ: 12KB
- 行数: 31行

**結果:** すべてのトラフィックがnode2にルーティングされています。node1にはトラフィックが到達していません。

## 問題の分析

### 1. FunctionCostが適用されていない原因

**可能性:**
1. node3がnode2のNameLSAからService Function情報を正しく取得できていない
2. `adjustNexthopCosts`が呼ばれているが、Service Function情報が見つからない
3. NameLSAの伝播に時間がかかっている

### 2. トラフィック分散が機能していない原因

**原因:**
- FunctionCostが適用されていないため、リンクコストのみでルーティングが決定されている
- node2へのリンクコスト（25）がnode1へのリンクコスト（50）より低いため、すべてのトラフィックがnode2にルーティングされている

## 次のステップ

1. **node3のNameLSA受信を確認**
   - node3がnode2のNameLSAを正しく受信しているか
   - node3がService Function情報を正しくデコードしているか

2. **FunctionCost計算の詳細ログを確認**
   - `adjustNexthopCosts`が呼ばれた際のService Function情報の取得状況
   - FunctionCost計算の実行状況

3. **NameLSA伝播のタイミングを確認**
   - NameLSA更新から伝播までの時間
   - ルーティング計算のタイミング

## 期待される動作（修正後）

### 利用率2.6%（0.0260789）の場合

**FunctionCost計算:**
```
FunctionCost = 0.0260789 * 100 = 2.6
```

**最終コスト:**
```
node2への最終コスト = 25 + 2.6 = 27.6
node1への最終コスト = 50 + 0 = 50
```

**結果:** node2の方が低コスト（27.6 < 50）→ トラフィックがnode2にルーティング（現在の動作と同じ）

### 利用率が高くなった場合（例: 30%）

**FunctionCost計算:**
```
FunctionCost = 0.3 * 100 = 30
```

**最終コスト:**
```
node2への最終コスト = 25 + 30 = 55
node1への最終コスト = 50 + 0 = 50
```

**結果:** node1の方が低コスト（50 < 55）→ トラフィックがnode1にルーティング（負荷分散が機能）

## 修正が必要な点

1. **node3のNameLSA受信確認**
   - node3がnode2のNameLSAからService Function情報を正しく取得できているか確認
   - `wireDecode`が正しく動作しているか確認

2. **FunctionCost計算の実行確認**
   - `adjustNexthopCosts`内でService Function情報が正しく取得できているか確認
   - FunctionCost計算が実行されているか確認

3. **デバッグログの追加**
   - node3でのNameLSA受信時のデバッグログ
   - FunctionCost計算時の詳細ログ

