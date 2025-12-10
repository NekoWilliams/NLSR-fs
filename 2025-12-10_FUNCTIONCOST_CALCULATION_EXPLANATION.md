# FunctionCost計算の詳細説明

## 計算式

FunctionCostは以下の式で計算されます：

```cpp
functionCost = processingTime * processingWeight +
              load * loadWeight +
              (usageCount / 100.0) * usageWeight
```

## 現在の設定

設定ファイル（`nlsr-node1.conf`, `nlsr-node2.conf`）から：

```conf
processing-weight  10000  ; weight for utilization (0.0 ~ 1.0, time-based function utilization)
load-weight       0.0    ; weight for load index (disabled for this test)
usage-weight      0.0    ; weight for usage count (disabled for this test)
```

## 利用率30%（0.3）の場合の計算例

### 入力値
- `processingTime = 0.3`（利用率30%）
- `load = 0.0`（load-weightが0.0のため無視）
- `usageCount = 0`（usage-weightが0.0のため無視）

### 計算
```
functionCost = 0.3 * 10000 + 0.0 * 0.0 + (0 / 100.0) * 0.0
            = 3000 + 0 + 0
            = 3000
```

**結果: FunctionCost = 3000**

## 最終的なルーティングコスト

最終的なコストは以下の式で計算されます：

```cpp
adjustedCost = originalCost + nexthopCost + functionCost
```

### 例: リンクコストが25の場合

```
adjustedCost = 25 + 0 + 3000
             = 3025
```

**結果: 最終的なルーティングコスト = 3025**

## 利用率とFunctionCostの関係

| 利用率 | processingTime | FunctionCost | リンクコスト25の場合の最終コスト |
|--------|----------------|--------------|--------------------------------|
| 0%     | 0.0            | 0            | 25                             |
| 10%    | 0.1            | 1000         | 1025                           |
| 20%    | 0.2            | 2000         | 2025                           |
| 30%    | 0.3            | 3000         | 3025                           |
| 40%    | 0.4            | 4000         | 4025                           |
| 50%    | 0.5            | 5000         | 5025                           |
| 60%    | 0.6            | 6000         | 6025                           |
| 70%    | 0.7            | 7000         | 7025                           |
| 80%    | 0.8            | 8000         | 8025                           |
| 90%    | 0.9            | 9000         | 9025                           |
| 100%   | 1.0            | 10000        | 10025                          |

## トラフィック分散への影響

### 例: node1とnode2の両方が/relayを提供している場合

**初期状態（利用率0%）:**
- node1へのリンクコスト: 50
- node2へのリンクコスト: 25
- node2の方が低コスト → すべてのトラフィックがnode2にルーティング

**node2の利用率が30%になった場合:**
- node1へのリンクコスト: 50（FunctionCost = 0）
- node2へのリンクコスト: 25 + 3000 = 3025（FunctionCost = 3000）
- node1の方が低コスト → すべてのトラフィックがnode1にルーティング

**node1の利用率が30%、node2の利用率が20%の場合:**
- node1へのリンクコスト: 50 + 3000 = 3050（FunctionCost = 3000）
- node2へのリンクコスト: 25 + 2000 = 2025（FunctionCost = 2000）
- node2の方が低コスト → すべてのトラフィックがnode2にルーティング

## 注意点

1. **利用率の範囲**: 利用率は0.0 ~ 1.0の範囲で、1.0を超える場合は1.0にクランプされます。

2. **FunctionCostの影響**: `processing-weight = 10000`という大きな値により、FunctionCostがリンクコストを大幅に上回ります。これにより、利用率の小さな違いでもルーティングが大きく変化します。

3. **負荷分散の効果**: 利用率が高いFunctionには高いコストが設定され、トラフィックが利用率の低いFunctionに分散されます。

4. **重みの調整**: `processing-weight`の値を調整することで、FunctionCostの影響を調整できます。
   - 小さい値（例: 100）: FunctionCostの影響が小さい
   - 大きい値（例: 10000）: FunctionCostの影響が大きい

## コード内の実装

```cpp
// name-prefix-table.cpp
functionCost = sfInfo.processingTime * processingWeight +
              sfInfo.load * loadWeight +
              (sfInfo.usageCount / 100.0) * usageWeight;

adjustedCost = originalCost + nexthopCost + functionCost;
```

この計算により、利用率に基づいて動的にルーティングコストが調整され、負荷分散が実現されます。

