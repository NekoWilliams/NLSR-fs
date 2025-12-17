2025-12-12

# FIB更新検知の問題点の検証

## ユーザーの指摘

「FunctionCostは動的に変動する。node1, node2それぞれどちらもが高いコストに変更された場合、FIBはそれらを検知できない。この指摘は正しいですか？」

## 検証結果

### 指摘の内容

FunctionCostは動的に変動するため、node1とnode2の両方が高いコスト（高い利用率）に変更された場合、FIBがそれを検知できない可能性がある。

### 現在の実装の動作

#### 1. FIB更新のトリガー

FIB更新は以下の場合にトリガーされます：

1. **NameLSAが更新された場合** (`updateFromLsdb`が呼ばれる)
2. **Service Function情報が変更された場合** (`updateFromLsdb`内で`hasSfInfo`が検出される)

```cpp
// name-prefix-table.cpp:135-151
if (hasSfInfo || !allSfInfo.empty()) {
  // Find all entries for this router and update them
  for (auto& entry : m_table) {
    // ...
    m_fib.update(entry->getNamePrefix(), 
                adjustNexthopCosts(entry->getNexthopList(), entry->getNamePrefix(), *entry));
  }
}
```

#### 2. `adjustNexthopCosts`の動作

```cpp
// 両方にFunctionCostが適用されている、または両方とも適用されていない場合、コストが低い方を保持
if (adjustedCost < existingCost) {
  nextHopMap[faceUri] = std::make_pair(newNextHop, functionCost);
} else {
  // 既存のNextHopを保持
}
```

**重要な点:**
- 同じ`FaceUri`のNextHopが複数存在する場合、コストが低い方のみが保持される
- しかし、**異なる`FaceUri`のNextHopは両方とも保持される**

### 問題の分析

#### シナリオ1: node1とnode2が異なるFaceUriを持つ場合

```
初期状態:
node1経由: FaceUri=tcp4://10.104.74.134:6363, adjustedCost=75 (functionCost=50)
node2経由: FaceUri=tcp4://10.109.33.231:6363, adjustedCost=50 (functionCost=25)

両方のノードが高いコストになった場合:
node1経由: FaceUri=tcp4://10.104.74.134:6363, adjustedCost=150 (functionCost=100)
node2経由: FaceUri=tcp4://10.109.33.231:6363, adjustedCost=125 (functionCost=75)
```

この場合：
- 異なる`FaceUri`なので、両方とも`nextHopMap`に追加される
- `adjustNexthopCosts`は両方のNextHopを返す
- FIBは両方のNextHopを登録する
- **FIBは検知できる** ✓

#### シナリオ2: node1とnode2が同じFaceUriを持つ場合

```
初期状態:
node1経由: FaceUri=tcp4://10.104.74.134:6363, adjustedCost=75 (functionCost=50)
node2経由: FaceUri=tcp4://10.104.74.134:6363, adjustedCost=50 (functionCost=25)

両方のノードが高いコストになった場合:
node1経由: FaceUri=tcp4://10.104.74.134:6363, adjustedCost=150 (functionCost=100)
node2経由: FaceUri=tcp4://10.104.74.134:6363, adjustedCost=125 (functionCost=75)
```

この場合：
- 同じ`FaceUri`なので、コストが低い方（node2経由、adjustedCost=125）のみが保持される
- `adjustNexthopCosts`は1つのNextHopのみを返す
- FIBは1つのNextHopのみを登録する
- **FIBは検知できるが、node1経由のNextHopが失われる** ✗

### 結論

**ユーザーの指摘は部分的に正しいです。**

1. **異なる`FaceUri`の場合**: FIBは検知できる（問題なし）✓
2. **同じ`FaceUri`の場合**: FIBは検知できるが、コストが低い方のみが保持されるため、コストが高い方のNextHopが失われる（問題あり）✗

### 根本的な問題

同じ`FaceUri`のNextHopが複数存在する場合、現在の実装では1つしか保持されません。これは、異なる`destRouterName`（node1とnode2）からのNextHopが同じ`FaceUri`を持つ場合、どちらか一方が失われることを意味します。

### 解決策の検討

#### 解決策1: すべてのNextHopを保持（推奨）

同じ`FaceUri`のNextHopが複数存在する場合でも、**すべてのNextHopを保持**するように修正します。

**問題点:**
- `NexthopList::addNextHop`の実装により、同じ`FaceUri`のNextHopは1つしか保持されない
- `NexthopList`の設計を変更する必要がある

#### 解決策2: コストが高い方を優先

両方にFunctionCostが適用されている場合、コストが高い方を優先するように修正します。

**問題点:**
- これは負荷分散の目的に反する（利用率が高い方を優先することになる）

#### 解決策3: すべてのNextHopを保持するための新しいデータ構造

`adjustNexthopCosts`内で、同じ`FaceUri`のNextHopをすべて保持するための新しいデータ構造を使用します。

**実装方法:**
- `std::multimap<ndn::FaceUri, NextHop>`を使用
- すべてのNextHopを保持し、`NexthopList`に追加する際に、コストが低い順にソート

### 推奨される修正

**解決策1を推奨**します。同じ`FaceUri`のNextHopが複数存在する場合でも、すべてのNextHopを保持するように修正します。

ただし、これは`NexthopList`の設計と矛盾する可能性があるため、慎重に検討する必要があります。
