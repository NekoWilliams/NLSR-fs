2025-12-12

# FunctionCost適用時の優先順位ロジックの説明

## 質問

既存・新規両方のNextHopにFunctionCostが適用されている場合、コストが低い方を優先するのはなぜですか？

## 回答

### FunctionCostの目的

FunctionCostの目的は、**利用率が高いノードへのトラフィックを減らし、利用率が低いノードへのトラフィックを増やす**ことです（負荷分散）。

### コスト計算の式

```
adjustedCost = originalCost + nexthopCost + functionCost
```

ここで：
- `functionCost = processingTime * processingWeight + load * loadWeight + (usageCount / 100.0) * usageWeight`
- `processingTime`は利用率（0.0～1.0）を表す

### 両方にFunctionCostが適用されている場合

両方のNextHopにFunctionCostが適用されている場合：

1. **adjustedCostが低い方** = FunctionCostが低い方 = 利用率が低い方
2. **adjustedCostが高い方** = FunctionCostが高い方 = 利用率が高い方

### なぜコストが低い方を優先するのか

**コストが低い方を優先するのは、負荷分散の目的に合致しています。**

- **コストが低い方** = 利用率が低い方 = そのノードを優先すべき（トラフィックを増やす）
- **コストが高い方** = 利用率が高い方 = そのノードを避けるべき（トラフィックを減らす）

### 具体例

```
既存のNextHop: adjustedCost = 25 + 0 + 50 = 75 (functionCost=50, 利用率=50%)
新しいNextHop: adjustedCost = 25 + 0 + 100 = 125 (functionCost=100, 利用率=100%)
```

この場合：
- 既存のNextHop（コスト=75）を優先
- これは、利用率が低い方（50%）を優先することになり、負荷分散の目的に合致

### 結論

両方にFunctionCostが適用されている場合、コストが低い方を優先するのは、**利用率が低い方を優先する**という負荷分散の目的に合致しているため、正しい動作です。

## 補足：他のケースとの比較

### ケース1: 一方のみにFunctionCostが適用されている場合

```
既存のNextHop: adjustedCost = 25 + 0 + 0 = 25 (functionCost=0, FunctionCost未適用)
新しいNextHop: adjustedCost = 25 + 0 + 100 = 125 (functionCost=100, FunctionCost適用)
```

この場合：
- 新しいNextHop（FunctionCost適用）を優先
- 理由：FunctionCostが適用されている方が、より正確な負荷情報を反映している

### ケース2: 両方ともFunctionCostが適用されていない場合

```
既存のNextHop: adjustedCost = 25 + 0 + 0 = 25 (functionCost=0)
新しいNextHop: adjustedCost = 25 + 0 + 0 = 25 (functionCost=0)
```

この場合：
- コストが低い方を優先（通常のルーティングロジック）

### ケース3: 両方にFunctionCostが適用されている場合（今回の質問）

```
既存のNextHop: adjustedCost = 25 + 0 + 50 = 75 (functionCost=50, 利用率=50%)
新しいNextHop: adjustedCost = 25 + 0 + 100 = 125 (functionCost=100, 利用率=100%)
```

この場合：
- コストが低い方（既存のNextHop）を優先
- 理由：利用率が低い方を優先することで、負荷分散が実現される

## まとめ

両方にFunctionCostが適用されている場合、コストが低い方を優先するのは、**利用率が低い方を優先する**という負荷分散の目的に合致しているため、正しい動作です。

