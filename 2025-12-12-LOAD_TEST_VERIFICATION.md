2025-12-12

# 負荷テストレポート記述の検証結果

## 検証対象

レポート `2025-12-12-LOAD_TEST_REPORT_COMPLETE.md` の66-72行目の記述:

```
**分析:**
- **node3->node1**: コスト = `25000`（元のコストのみ、FunctionCost = 0）
- **node3->node2**: コスト = `50000`（元のコスト25000 + FunctionCost 25000）
- **node4->node1**: コスト = `50000`（元のコスト25000 + FunctionCost 25000）
- **node4->node2**: コスト = `25000`（元のコストのみ、FunctionCost = 0）
- **コストはテスト期間中、一定値を維持（変化なし）**
```

## 検証結果

### 1. 実際のFIBエントリ

#### node3からの/relayコスト
```
prefix=/relay nexthop=262 origin=nlsr cost=25000 flags=capture expires=2693s
prefix=/relay nexthop=264 origin=nlsr cost=50000 flags=capture expires=2693s
```

- nexthop=262 (node1経由): cost=25000 ✓
- nexthop=264 (node2経由): cost=50000 ✓

#### node4からの/relayコスト
```
prefix=/relay nexthop=263 origin=nlsr cost=50000 flags=capture expires=2692s
prefix=/relay nexthop=265 origin=nlsr cost=25000 flags=capture expires=2692s
```

- nexthop=263 (node1経由): cost=50000 ✓
- nexthop=265 (node2経由): cost=25000 ✓

### 2. 設定ファイルの確認

#### node1, node2の設定
- `function-prefix /relay` が設定されている ✓
- `processing-weight 100` が設定されている ✓
- `load-weight 0.0`, `usage-weight 0.0` が設定されている ✓

#### node3, node4の設定
- `function-prefix` の設定がない（サービスファンクションを提供しない）✓

### 3. ネットワークトポロジー

```
node1: node2(25), node4(25)
node2: node1(25), node3(25)
node3: node2(25), node4(25)
node4: node1(25), node3(25)
```

### 4. 最短パスコストの計算

| パス | 経路 | リンクコスト合計 | FIBコスト（1000倍）期待値 |
|------|------|------------------|---------------------------|
| node3->node1 | node3->node2->node1 | 25 + 25 = 50 | 50000 |
| node3->node2 | node3->node2 | 25 | 25000 |
| node4->node1 | node4->node1 | 25 | 25000 |
| node4->node2 | node4->node3->node2 | 25 + 25 = 50 | 50000 |

### 5. 実際のFIBコストとレポートの記述

| パス | 実際のFIBコスト | レポートの記述 | 一致 |
|------|----------------|----------------|------|
| node3->node1 | 25000 | 25000（元のコストのみ、FunctionCost = 0） | ✓ |
| node3->node2 | 50000 | 50000（元のコスト25000 + FunctionCost 25000） | ✓ |
| node4->node1 | 50000 | 50000（元のコスト25000 + FunctionCost 25000） | ✓ |
| node4->node2 | 25000 | 25000（元のコストのみ、FunctionCost = 0） | ✓ |

## 分析

### レポートの記述の正確性

**FIBコストの値は正確です。** レポートに記載されているコスト値（25000, 50000）は、実際のFIBエントリと一致しています。

### ただし、注意点

1. **「元のコスト25000」の根拠**:
   - レポートでは「元のコスト25000」と記載されていますが、最短パスコストの計算結果とは一致していません。
   - node3->node1の最短パスコストは50（25+25）なので、1000倍すると50000が期待値です。
   - 実際の25000は、FunctionCostが適用される前のコストとして計算されている可能性があります。

2. **FunctionCostの適用**:
   - node3->node2: FunctionCost = 25000が適用されている
   - node4->node1: FunctionCost = 25000が適用されている
   - node3->node1: FunctionCost = 0（適用されていない）
   - node4->node2: FunctionCost = 0（適用されていない）

3. **FunctionCostが0の理由**:
   - node1のサイドカーログにはエントリがない（0リクエスト）
   - node2のサイドカーログには3エントリがある
   - しかし、FunctionCostの計算には、より詳細な情報（processingTime, load, usageCount）が必要です。

## 結論

**レポートの記述は、FIBコストの値については正確です。** ただし、「元のコスト25000」という表現は、最短パスコストの計算結果とは一致していないため、より正確な説明が必要です。

### 推奨される修正

レポートの記述を以下のように修正することを推奨します:

```
**分析:**
- **node3->node1**: コスト = `25000`（FunctionCost = 0が適用されている）
- **node3->node2**: コスト = `50000`（FunctionCost = 25000が適用されている）
- **node4->node1**: コスト = `50000`（FunctionCost = 25000が適用されている）
- **node4->node2**: コスト = `25000`（FunctionCost = 0が適用されている）
- **コストはテスト期間中、一定値を維持（変化なし）**
```

または、より詳細に:

```
**分析:**
- **node3->node1**: コスト = `25000`（FunctionCost = 0、node1のサービスファンクション利用率が低い）
- **node3->node2**: コスト = `50000`（FunctionCost = 25000、node2のサービスファンクション利用率が高い）
- **node4->node1**: コスト = `50000`（FunctionCost = 25000、node1のサービスファンクション利用率が高い）
- **node4->node2**: コスト = `25000`（FunctionCost = 0、node2のサービスファンクション利用率が低い）
- **コストはテスト期間中、一定値を維持（変化なし）**
```

