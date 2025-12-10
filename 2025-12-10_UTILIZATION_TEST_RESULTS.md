# 利用率ベースFunctionCostテスト結果

## テスト実施日時
2025年12月10日

## テスト目的
1. ランダム待機（0~0.5秒）が機能しているか確認
2. 利用率計算が正しく動作しているか確認
3. FunctionCostが正しく適用されているか確認
4. トラフィック分散が機能しているか確認

## テスト結果

### 1. ランダム待機の確認 ✓

**node2のサービスログ:**
```
service time: 0.014195680618286133
service time: 0.2894749641418457
service time: 0.35771918296813965
service time: 0.4397568702697754
```

**結果:** ランダム待機は正常に機能しています。0~0.5秒の範囲内でランダムな待機時間が設定されています。

### 2. 利用率計算の確認 ✓

**node2のNLSRログ:**
```
Calculated utilization: 0.363991 (totalProcessingTime=0.363991s, windowSeconds=1, validEntries=1)
ServiceFunctionInfo: utilization=0.363991 (calculated from 1 entries)
ServiceFunctionInfo: processingTime=0.363991, load=0, usageCount=0
```

**結果:** 利用率計算は正常に動作しています。時間窓（1秒）内の総処理時間を計算し、利用率を算出しています。

### 3. FunctionCost適用の問題 ✗

**問題点:**
- 利用率は計算されている（0.363991など）
- しかし、NameLSA更新時には `processingTime=0` となっている
- `wireEncode`時に "Encoding 0 Service Function info entries" となっている

**原因:**
`buildAndInstallOwnNameLsa()`で新しいNameLSAを作成する際、既存のNameLSAからService Function情報をコピーする際に、条件チェック（`processingTime > 0.0 || load > 0.0 || usageCount > 0`）により、値が0の場合はコピーされていませんでした。

**修正内容:**
1. すべてのService Function情報をコピーするように変更
2. 条件チェックを削除（値が0でもコピー）
3. `getAllServiceFunctionInfo()`を使用してすべてのエントリをコピー
4. デバッグログを追加

### 4. トラフィック分散の確認 ✗

**node1のsidecarログ:**
- ファイルサイズ: 0バイト
- 行数: 0行

**node2のsidecarログ:**
- ファイルサイズ: 16KB
- 行数: 41行

**結果:** すべてのトラフィックがnode2にルーティングされています。node1にはトラフィックが到達していません。

**原因:**
- FunctionCostが適用されていないため、リンクコストのみでルーティングが決定されている
- node2へのリンクコスト（25）がnode1へのリンクコスト（50）より低いため、すべてのトラフィックがnode2にルーティングされている

## 修正後の期待される動作

### 1. Service Function情報の保持
- `setServiceFunctionInfo()`で設定した情報が、`buildAndInstallOwnNameLsa()`で新しいNameLSAを作成する際にも保持される
- すべてのService Function情報が正しくコピーされる

### 2. FunctionCostの適用
- 利用率が正しく計算され、NameLSAに反映される
- FunctionCostが正しく適用され、ルーティングコストに反映される
- トラフィックが分散される

### 3. トラフィック分散
- node1とnode2の両方にトラフィックが分散される
- 利用率が高いFunctionには高いコストが設定され、トラフィックが分散される

## 次のステップ

1. **ビルドとデプロイ**
   - 修正したコードをビルド
   - Podを再デプロイ

2. **再テスト**
   - ランダム待機が機能しているか確認
   - 利用率が正しく計算されているか確認
   - FunctionCostが正しく適用されているか確認
   - トラフィック分散が機能しているか確認

3. **調整**
   - 待機時間の範囲を調整（必要に応じて）
   - 時間窓のサイズを調整（必要に応じて）
   - FunctionCostの重みを調整（必要に応じて）

## 修正ファイル

- `/home/katsutoshi/NLSR-fs/src/lsdb.cpp`
  - `buildAndInstallOwnNameLsa()`メソッドを修正
  - すべてのService Function情報をコピーするように変更

