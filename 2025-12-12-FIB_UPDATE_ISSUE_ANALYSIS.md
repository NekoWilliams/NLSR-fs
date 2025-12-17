2025-12-12

# FIB更新がされていない理由の詳細分析

## 検証結果のまとめ

### 確認できたこと

1. **FunctionCostの計算は正常に動作**
   - `adjustNexthopCosts`で計算されたコスト: 150, 125, 98.2594など
   - 計算ロジックは問題なし

2. **`Fib::update`は呼ばれている**
   - コスト変更は検出されている
   - unregister/registerは実行されている

3. **問題点: FIBに登録されるコストが古い**
   - `adjustNexthopCosts`で計算されたコスト（150など）がFIBに反映されていない
   - FIBに登録されるコストは古い値（25, 50, 75）のまま

## 根本原因の特定

### 問題の核心

ログを詳細に分析した結果、以下の問題が判明しました：

```
Adjusting cost for /relay to /ndn/jp/%C1.Router/node1: adjustedCost=150
Adjusting cost for /relay to /ndn/jp/%C1.Router/node2: adjustedCost=25
Adjusting cost for /relay to /ndn/jp/%C1.Router/node2: adjustedCost=75
Registering prefix: /relay faceUri: tcp4://10.104.74.134:6363 with cost: 25
Registering prefix: /relay faceUri: tcp4://10.109.33.231:6363 with cost: 75
```

**問題:**
- `adjustedCost=150`のNextHopがFIBに登録されていない
- 代わりに、`adjustedCost=25`と`adjustedCost=75`のNextHopのみが登録されている

### 原因の特定

`NexthopList::addNextHop`の実装を確認したところ、以下の問題が判明しました：

```cpp
// nexthop-list.hpp:56-70
void addNextHop(const NextHop& nh)
{
  auto it = std::find_if(m_nexthopList.begin(), m_nexthopList.end(),
    [&nh] (const auto& item) {
      return item.getConnectingFaceUri() == nh.getConnectingFaceUri();
    });
  if (it == m_nexthopList.end()) {
    m_nexthopList.insert(nh);
  }
  else if (it->getRouteCost() > nh.getRouteCost()) {
    m_nexthopList.erase(it);
    m_nexthopList.insert(nh);
  }
}
```

**問題点:**
1. `addNextHop`は同じ`FaceUri`のNextHopが既に存在する場合、新しいコストが**既存のコストより低い場合のみ**更新します
2. しかし、`adjustNexthopCosts`は複数の`RoutingTablePoolEntry`からNextHopを追加するため、同じ`FaceUri`のNextHopが複数存在する可能性があります
3. この場合、コストが低い方のみが保持され、コストが高い方（FunctionCostが適用された方）が除外されます

### 具体例

`adjustNexthopCosts`の実装を見ると：

```cpp
// name-prefix-table.cpp:190-284
for (const auto& rtpe : npte.getRteList()) {
  // ... FunctionCost計算 ...
  for (const auto& nh : rtpe->getNexthopList().getNextHops()) {
    double adjustedCost = originalCost + nexthopCost + functionCost;
    const NextHop newNextHop = NextHop(nh.getConnectingFaceUri(), adjustedCost);
    new_nhList.addNextHop(newNextHop);  // ← ここで問題が発生
  }
}
```

**問題のシナリオ:**
1. node1経由のNextHop: `FaceUri=tcp4://10.104.74.134:6363, adjustedCost=150` (functionCost=100適用)
2. node2経由のNextHop: `FaceUri=tcp4://10.104.74.134:6363, adjustedCost=25` (functionCost=0)

同じ`FaceUri`を持つNextHopが存在する場合、`addNextHop`はコストが低い方（25）のみを保持し、コストが高い方（150）を除外します。

## 解決策

### 解決策1: `adjustNexthopCosts`の実装を修正（推奨）

`adjustNexthopCosts`で、同じ`FaceUri`のNextHopが複数存在する場合、コストが**低い方**を保持するのではなく、**すべてのNextHopを保持**するように修正します。

ただし、これは`NexthopList`の設計と矛盾する可能性があります。

### 解決策2: `adjustNexthopCosts`のロジックを変更

`adjustNexthopCosts`で、各`RoutingTablePoolEntry`からNextHopを追加する際に、同じ`FaceUri`のNextHopが既に存在する場合、コストが**低い方**を保持するのではなく、**FunctionCostが適用された方**を優先するように修正します。

### 解決策3: `NexthopList::addNextHop`の動作を変更

`NexthopList::addNextHop`の動作を変更し、同じ`FaceUri`のNextHopが既に存在する場合、常に新しいコストで更新するようにします。

ただし、これは既存の動作を変更するため、他の部分に影響を与える可能性があります。

## 推奨される解決策

### 解決策2を推奨

`adjustNexthopCosts`の実装を修正し、同じ`FaceUri`のNextHopが複数存在する場合、**FunctionCostが適用された方**を優先するようにします。

具体的には：
1. 各`RoutingTablePoolEntry`からNextHopを追加する前に、既存のNextHopをチェック
2. 同じ`FaceUri`のNextHopが既に存在する場合、FunctionCostが適用されている方を優先
3. 両方にFunctionCostが適用されている場合、コストが低い方を保持

## 次のアクション

### 1. `adjustNexthopCosts`の実装を修正（優先度: 高）

同じ`FaceUri`のNextHopが複数存在する場合の処理を修正します。

### 2. デバッグログの追加（優先度: 中）

`adjustNexthopCosts`で、同じ`FaceUri`のNextHopが複数存在する場合を検出できるようにデバッグログを追加します。

### 3. テストの実行（優先度: 高）

修正後、再度テストを実行し、FIBエントリのコストが正しく更新されることを確認します。

