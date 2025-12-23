2025-12-12

# ファンクションコスト計算・更新テスト結果

## テスト概要

- **目的**: ファンクションコストが正しく計算・アップデートされているか確認
- **実施日時**: 2025-12-23 15:49
- **テストスクリプト**: `test_function_cost_simple.sh`

## テスト結果

### ✅ 成功項目

#### 1. データ転送成功
- **結果**: ✓ 成功
- **詳細**: 
  - データサイズ: 336 bytes
  - 経過時間: 1秒
  - ndncatchunksが正常に動作（タイムアウト5秒設定）

#### 2. サイドカーログの更新
- **Node1**: ログが更新されていない（古いエントリのみ）
- **Node2**: ✓ ログが更新された
  - 新しいエントリが追加: `in_time: 2025-12-23 15:49:10.828173`
  - データサイズ: 24 bytes

#### 3. FunctionCostの計算と更新

**初期状態（1回目のリクエスト後）**:
- **node2経由**: functionCost = 0.402282
  - processingTime = 0.00402282
  - processingWeight = 100
  - functionCost = 0.00402282 * 100 = 0.402282
- **node1経由**: functionCost = 0 (staleと判定)

**5回のリクエスト後**:
- **node2経由**: functionCost = 3.13593
  - 複数のリクエストにより、processingTimeが増加
  - functionCostが動的に更新されている ✓
- **node1経由**: functionCost = 0 (staleと判定、ログが更新されていない)

#### 4. FIBのコスト更新

**Node3のFIBエントリ**:
- **初期**: 
  - node2経由: cost = 25402 (25000 + 402)
  - node1経由: cost = 50000
- **5回のリクエスト後**:
  - node2経由: cost = 28136 (25000 + 3136) ✓ 更新された
  - node1経由: cost = 50000 (変化なし)

**Node4のFIBエントリ**:
- **初期**: 
  - node2経由: cost = 25000
  - node1経由: cost = 50402 (50000 + 402)
- **5回のリクエスト後**:
  - node2経由: cost = 25000 (変化なし、node4からnode2への直接パス)
  - node1経由: cost = 78136 (50000 + 28136) ✓ 更新された

### ⚠️ 問題点

#### 1. Node1のログが更新されていない
- **現象**: Node1のサイドカーログに新しいエントリが追加されていない
- **影響**: Node1経由のFunctionCostが常に0（staleと判定）
- **原因**: 
  - トラフィックがNode2にのみ到達している可能性
  - または、Node1のsidecarがログを書き込んでいない

#### 2. Node1経由のFunctionCostが0のまま
- **現象**: Node1経由のNextHopのfunctionCostが常に0
- **ログ**: `Service Function info is stale for /relay (destRouterName=/ndn/jp/%C1.Router/node1), functionCost=0`
- **原因**: Node1のログが更新されていないため、ServiceFunctionInfoがstaleと判定される

## 分析

### FunctionCostの計算式

```
functionCost = processingTime * processingWeight + 
               load * loadWeight + 
               (usageCount / 100.0) * usageWeight
```

**Node2の例**:
- processingTime = 0.00402282 (1回目のリクエスト)
- processingWeight = 100
- functionCost = 0.00402282 * 100 = 0.402282

**5回のリクエスト後**:
- processingTimeが増加（複数のリクエストの平均）
- functionCost = 3.13593

### コストの構成

**FIBエントリのコスト**:
```
adjustedCost = originalCost + nexthopCost + functionCost
```

**Node3からNode2経由の例**:
- originalCost = 25000
- nexthopCost = 0
- functionCost = 0.402282 (初期) → 3.13593 (5回後)
- adjustedCost = 25402 (初期) → 28136 (5回後)

## 結論

### ✅ 成功した点

1. **FunctionCostの計算**: 正しく計算されている
2. **FunctionCostの動的更新**: トラフィックに応じて更新されている
3. **FIBのコスト更新**: FunctionCostの変化がFIBに反映されている
4. **スタイルデータの閾値調整**: 180秒の閾値が機能している（Node2のログが更新されているため）

### ⚠️ 改善が必要な点

1. **Node1のログ更新**: Node1のサイドカーログが更新されていない
   - トラフィックがNode2にのみ到達している可能性
   - または、Node1のsidecarがログを書き込んでいない

2. **トラフィック分散**: トラフィックがNode2に集中している
   - コストが低いため、Node2が選択され続けている
   - Node1にもトラフィックを分散させる必要がある

## 次のステップ

1. **Node1のログ更新を確認**: 
   - Node1のsidecarが正常に動作しているか確認
   - Node1にトラフィックが到達しているか確認

2. **トラフィック分散の改善**:
   - 初期コストを調整して、Node1にもトラフィックが到達するようにする
   - または、強制的にNode1にトラフィックを送信してテストする

3. **長期的なテスト**:
   - より長い期間のテストを実施
   - FunctionCostの動的変化を継続的に監視

