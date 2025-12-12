2025-12-12

# NameLSA内容確認結果

## 検証実施日時
2025年12月12日

## NameLSAの内容確認

### node1のNameLSA
**wireEncode時のログ:**
```
wireEncode: Encoding 1 Service Function info entries
wireEncode: Encoding Service Function: /relay, processingTime=0, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
```

**分析:**
- Service Function情報はNameLSAに含まれている ✓
- `processingTime=0`（利用率が0）
- `processingWeight=100`（設定ファイルの値が正しく反映されている）✓

### node3が受信したNameLSA（node1から）
**wireDecode時のログ:**
```
wireDecode: Decoded Service Function name: /relay
wireDecode: Stored Service Function info: /relay -> processingTime=0, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
wireDecode: Completed decode. Service Function info entries: 1
```

**分析:**
- node3はnode1のNameLSAを正しく受信している ✓
- Service Function情報を正しくデコードしている ✓
- `processingTime=0`のため、FunctionCostも0になる

### node2のNameLSA
**確認結果:**
- ログが取得できなかった（node2のプロセス出力を確認できなかった）

### node3のadjustNexthopCosts呼び出し
**確認結果:**
- `adjustNexthopCosts`の呼び出しログが見つからない
- `Processing RoutingTablePoolEntry`のログが見つからない
- `FunctionCost calculated`のログが見つからない

## 問題の分析

### 問題1: processingTimeが0になっている

**原因:**
- node1のsidecarログ: 0行（トラフィックが到達していない）
- node2のsidecarログ: 8行（トラフィックが到達している）

**分析:**
- すべてのトラフィックがnode2にルーティングされている
- node1にはトラフィックが到達していないため、利用率が計算されない
- 結果として、node1のNameLSAの`processingTime=0`になる

### 問題2: adjustNexthopCostsが呼ばれていない

**症状:**
- `adjustNexthopCosts`の呼び出しログが見つからない
- `Processing RoutingTablePoolEntry`のログが見つからない

**原因の可能性:**
1. `addEntry`や`updateFromLsdb`が呼ばれていない
2. ルーティング計算が実行されていない
3. NameLSA更新が検知されていない

### 問題3: トラフィック分散が機能していない

**症状:**
- node1にはトラフィックが到達していない
- node2にのみトラフィックが到達している

**原因:**
- FunctionCostが適用されていないため、リンクコストのみでルーティングが決定されている
- node2へのリンクコスト（25）がnode1へのリンクコスト（50）より低いため、すべてのトラフィックがnode2にルーティングされている

## 確認できたこと

### ✓ NameLSAにService Function情報が含まれている
- node1のNameLSAにService Function情報が含まれている
- `processingWeight=100`が正しく設定されている

### ✓ node3がNameLSAを正しく受信している
- node3はnode1のNameLSAを正しく受信している
- Service Function情報を正しくデコードしている

### ✓ NameLSAの伝播は正常
- NameLSAは正常に伝播されている
- wireEncode/wireDecodeは正常に動作している

## 確認できなかったこと

### ✗ adjustNexthopCostsが呼ばれているか
- `adjustNexthopCosts`の呼び出しログが見つからない
- ルーティング計算が実行されているか不明

### ✗ node2のNameLSAの内容
- node2のNameLSAの内容を確認できなかった
- node2の`processingTime`が0かどうか不明

## 次の検証ステップ

### 1. adjustNexthopCostsの呼び出し確認
- `addEntry`や`updateFromLsdb`が呼ばれているか確認
- ルーティング計算が実行されているか確認

### 2. node2のNameLSAの確認
- node2のNameLSAの内容を確認
- node2の`processingTime`を確認

### 3. ルーティング計算のタイミング確認
- NameLSA更新時にルーティング計算がスケジュールされているか確認
- `updateFromLsdb`が正しく呼ばれているか確認

### 4. トラフィック分散の確認
- node1にトラフィックが到達するように、FunctionCostを適用する必要がある
- まず、`adjustNexthopCosts`が呼ばれるようにする必要がある

## 結論

NameLSAにはService Function情報が正しく含まれており、node3も正しく受信しています。しかし、`processingTime=0`のため、FunctionCostも0になっています。

問題は、`adjustNexthopCosts`が呼ばれていない可能性があります。次のステップとして、`adjustNexthopCosts`の呼び出しを確認する必要があります。

