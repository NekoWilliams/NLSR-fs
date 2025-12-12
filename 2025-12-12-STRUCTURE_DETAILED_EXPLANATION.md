2025-12-12

# NLSRルーティング構造の詳細説明

## 1. NextHop構造体

### 1.1 基本構造

```cpp
class NextHop {
  ndn::FaceUri m_connectingFaceUri;  // 接続先のFace URI
  double m_routeCost;                // ルートコスト（浮動小数点）
  bool m_isHyperbolic;               // 双曲線ルーティングかどうか
  mutable ndn::Block m_wire;          // シリアライズ用のワイヤ形式
};
```

### 1.2 役割と責任

- **目的**: 単一のネクストホップ（次の転送先）を表現
- **情報保持**:
  - `m_connectingFaceUri`: パケットを転送する先のFace URI（例: `udp4://192.168.1.1:6363`）
  - `m_routeCost`: このネクストホップ経由でのルートコスト（累積コスト）
  - `m_isHyperbolic`: 双曲線ルーティングかLink Stateルーティングかのフラグ

### 1.3 重要なメソッド

- `getRouteCost()`: 浮動小数点のコストを取得
- `getRouteCostAsAdjustedInteger()`: NFD用に整数化されたコストを取得（1000倍して精度を保持）
- `getConnectingFaceUri()`: Face URIを取得

### 1.4 コストの計算と変換

```cpp
// 浮動小数点コスト（例: 25.132）を整数（25132）に変換
uint64_t getRouteCostAsAdjustedInteger() const {
  return static_cast<uint64_t>(round(m_routeCost * 1000));
}
```

**注意点**: 
- NFDは整数コストしか受け付けないため、小数部分を保持するために1000倍している
- 例: `25.132` → `25132`（0.001の精度を保持）

### 1.5 問題点

- **`destRouterName`情報が欠如**: `NextHop`は「次のFace」しか保持せず、「最終的な宛先ルーター」の情報を持たない
- **FunctionCost適用の困難**: 各`NextHop`がどの`destRouterName`に対応するかを判断できない

---

## 2. NamePrefixTableEntry

### 2.1 基本構造

```cpp
class NamePrefixTableEntry {
  ndn::Name m_namePrefix;                                    // 名前プレフィックス（例: /relay）
  std::list<std::shared_ptr<RoutingTablePoolEntry>> m_rteList;  // ルーティングテーブルプールエントリのリスト
  NexthopList m_nexthopList;                                 // 生成されたネクストホップリスト
};
```

### 2.2 役割と責任

- **目的**: 特定の名前プレフィックス（例: `/relay`）に対するルーティング情報を管理
- **情報保持**:
  - `m_namePrefix`: このエントリが管理する名前プレフィックス
  - `m_rteList`: このプレフィックスをアドバタイズしている各ルーターへの経路情報（`RoutingTablePoolEntry`のリスト）
  - `m_nexthopList`: `m_rteList`から生成された統合ネクストホップリスト

### 2.3 重要なメソッド

- `generateNhlfromRteList()`: `m_rteList`内のすべての`RoutingTablePoolEntry`から`NexthopList`を生成
- `addRoutingTableEntry()`: 新しい`RoutingTablePoolEntry`を追加
- `removeRoutingTableEntry()`: `RoutingTablePoolEntry`を削除

### 2.4 ネクストホップリストの生成

```cpp
void NamePrefixTableEntry::generateNhlfromRteList() {
  m_nexthopList.clear();
  // 各RoutingTablePoolEntry（異なるdestRouterNameに対応）から
  // すべてのNextHopを収集して統合
  for (auto rtpe : m_rteList) {
    for (auto nh : rtpe->getNexthopList().getNextHops()) {
      m_nexthopList.addNextHop(nh);
    }
  }
}
```

**重要な点**:
- `m_rteList`には複数の`RoutingTablePoolEntry`が含まれる（例: `node1`用、`node2`用）
- `generateNhlfromRteList()`は、これらのすべての`NextHop`を統合して1つの`NexthopList`を作成
- **問題**: 統合後、各`NextHop`がどの`destRouterName`に対応するかの情報が失われる

### 2.5 データフロー

```
NamePrefixTableEntry (/relay)
  ├─ RoutingTablePoolEntry (node1) → NexthopList {NextHop(face1, cost1)}
  └─ RoutingTablePoolEntry (node2) → NexthopList {NextHop(face2, cost2)}
         ↓ generateNhlfromRteList()
  NexthopList {NextHop(face1, cost1), NextHop(face2, cost2)}
         ↓ 問題: 各NextHopがどのdestRouterNameに対応するか不明
```

---

## 3. RoutingTablePoolEntry

### 3.1 基本構造

```cpp
class RoutingTablePoolEntry : public RoutingTableEntry {
  uint64_t m_useCount;  // 参照カウント（複数のNamePrefixTableEntryで共有される）
  
  // 親クラス（RoutingTableEntry）から継承:
  ndn::Name m_destination;      // 宛先ルーター名（例: /ndn/node1）
  NexthopList m_nexthopList;   // この宛先へのネクストホップリスト
  
  // 双方向参照:
  std::unordered_map<ndn::Name, std::weak_ptr<NamePrefixTableEntry>> 
    namePrefixTableEntries;  // このRTPEを使用しているNPTエントリ
};
```

### 3.2 役割と責任

- **目的**: メモリ効率化のための共有プールエントリ
- **情報保持**:
  - `m_destination`: 宛先ルーター名（例: `/ndn/node1`、`/ndn/node2`）
  - `m_nexthopList`: この宛先ルーターへのネクストホップリスト
  - `m_useCount`: このエントリを参照している`NamePrefixTableEntry`の数
  - `namePrefixTableEntries`: このエントリを使用している`NamePrefixTableEntry`への弱参照

### 3.3 メモリ効率化の仕組み

- **共有**: 複数の`NamePrefixTableEntry`が同じ`RoutingTablePoolEntry`を共有可能
- **例**: `/relay`と`/compute`の両方が`node1`を経由する場合、同じ`RoutingTablePoolEntry(node1)`を共有

```
NamePrefixTableEntry (/relay)
  └─ RoutingTablePoolEntry (node1) ← 共有
         ↑
NamePrefixTableEntry (/compute)
  └─ RoutingTablePoolEntry (node1) ← 共有
```

### 3.4 重要なメソッド

- `getDestination()`: 宛先ルーター名を取得（これが`destRouterName`）
- `getNexthopList()`: この宛先へのネクストホップリストを取得
- `incrementUseCount()` / `decrementUseCount()`: 参照カウントの管理

### 3.5 データフロー

```
RoutingTablePoolEntry (node1)
  ├─ m_destination = /ndn/node1
  └─ m_nexthopList = {NextHop(face1, cost1), NextHop(face2, cost2)}
         ↓
NamePrefixTableEntry (/relay)
  └─ m_rteList = [RoutingTablePoolEntry(node1), RoutingTablePoolEntry(node2)]
         ↓ generateNhlfromRteList()
  m_nexthopList = {NextHop(face1, cost1), NextHop(face2, cost2), ...}
```

**重要な点**:
- `RoutingTablePoolEntry`は`destRouterName`（`m_destination`）を保持している
- しかし、`generateNhlfromRteList()`で統合された`NexthopList`には、各`NextHop`がどの`destRouterName`に対応するかの情報が含まれない

---

## 4. destRouterName

### 4.1 定義と意味

- **型**: `ndn::Name`
- **意味**: サービスファンクションを提供するルーターの名前（例: `/ndn/node1`、`/ndn/node2`）
- **用途**: FunctionCost計算時に、どのルーターの`NameLSA`から`ServiceFunctionInfo`を取得するかを指定

### 4.2 使用箇所

#### 4.2.1 `adjustNexthopCosts()`の引数として

```cpp
NexthopList adjustNexthopCosts(
  const NexthopList& nhlist,        // 調整対象のネクストホップリスト
  const ndn::Name& nameToCheck,     // チェックする名前プレフィックス（例: /relay）
  const ndn::Name& destRouterName   // 宛先ルーター名（例: /ndn/node1）
);
```

#### 4.2.2 `addEntry()`の引数として

```cpp
void addEntry(
  const ndn::Name& name,           // 名前プレフィックス（例: /relay）
  const ndn::Name& destRouter      // このプレフィックスをアドバタイズしているルーター
);
```

### 4.3 問題点

- **`adjustNexthopCosts()`の呼び出し時**: 1つの`destRouterName`が渡されるが、`nhlist`には複数の`NextHop`が含まれる可能性がある
- **各`NextHop`の対応関係不明**: `nhlist`内の各`NextHop`がどの`destRouterName`に対応するかを判断できない

### 4.4 現在の実装の問題

```cpp
// 現在の実装（問題あり）
void NamePrefixTable::addEntry(const ndn::Name& name, const ndn::Name& destRouter) {
  // ...
  npte->generateNhlfromRteList();  // 複数のdestRouterNameのNextHopを統合
  
  // 問題: destRouter（例: node1）を渡しているが、
  // nhlistにはnode1とnode2の両方のNextHopが含まれる可能性がある
  m_fib.update(name, adjustNexthopCosts(npte->getNexthopList(), name, destRouter));
}
```

**問題の具体例**:
- `/relay`プレフィックスに対して、`node1`と`node2`の両方がアドバタイズしている
- `generateNhlfromRteList()`で統合された`NexthopList`には、`node1`経由と`node2`経由の両方の`NextHop`が含まれる
- `adjustNexthopCosts()`に`destRouterName=node1`を渡すと、`node2`経由の`NextHop`にも`node1`のFunctionCostが適用されてしまう

---

## 5. adjustNexthopCosts()

### 5.1 関数シグネチャ

```cpp
NexthopList NamePrefixTable::adjustNexthopCosts(
  const NexthopList& nhlist,        // 調整対象のネクストホップリスト
  const ndn::Name& nameToCheck,     // チェックする名前プレフィックス（例: /relay）
  const ndn::Name& destRouterName   // 宛先ルーター名（例: /ndn/node1）
);
```

### 5.2 役割と責任

- **目的**: 各`NextHop`のコストにFunctionCostを加算
- **処理フロー**:
  1. `destRouterName`の`NameLSA`から`ServiceFunctionInfo`を取得
  2. FunctionCostを計算（`processingTime * processingWeight + load * loadWeight + ...`）
  3. 各`NextHop`のコストにFunctionCostを加算
  4. 新しい`NexthopList`を返す

### 5.3 現在の実装（修正後）

```cpp
NexthopList NamePrefixTable::adjustNexthopCosts(
  const NexthopList& nhlist, 
  const ndn::Name& nameToCheck, 
  const ndn::Name& destRouterName
) {
  NexthopList new_nhList;
  
  // 問題: 1つのdestRouterNameに対してFunctionCostを計算
  double functionCost = 0.0;
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    if (sfInfo.processingTime > 0.0 || ...) {
      functionCost = sfInfo.processingTime * sfInfo.processingWeight + ...;
    }
  }
  
  // 問題: nhlist内のすべてのNextHopに同じfunctionCostを適用
  for (const auto& nh : nhlist.getNextHops()) {
    double originalCost = nh.getRouteCost();
    double nexthopCost = m_nexthopCost[DestNameKey(destRouterName, nameToCheck)];
    
    // 修正試み: 各NextHopに対して個別にFunctionCostを計算
    // しかし、各NextHopがどのdestRouterNameに対応するかを判断できない
    double functionCostForThisRouter = 0.0;
    auto nameLsaForThisRouter = m_lsdb.findLsa<NameLsa>(destRouterName);  // 同じdestRouterNameを使用
    // ...
    
    double adjustedCost = originalCost + nexthopCost + functionCostForThisRouter;
    new_nhList.addNextHop(NextHop(nh.getConnectingFaceUri(), adjustedCost));
  }
  
  return new_nhList;
}
```

### 5.4 問題点の詳細

#### 問題1: 各NextHopの対応関係不明

- `nhlist`には複数の`NextHop`が含まれる可能性がある
- 各`NextHop`がどの`destRouterName`に対応するかを判断する情報がない
- 現在の実装では、すべての`NextHop`に対して同じ`destRouterName`のFunctionCostを適用している

#### 問題2: 呼び出し元でのdestRouterNameの選択

```cpp
// addEntry()での呼び出し
void NamePrefixTable::addEntry(const ndn::Name& name, const ndn::Name& destRouter) {
  // ...
  // 問題: destRouter（例: node1）を渡しているが、
  // nhlistにはnode1とnode2の両方のNextHopが含まれる可能性がある
  m_fib.update(name, adjustNexthopCosts(npte->getNexthopList(), name, destRouter));
}
```

**具体例**:
- `/relay`プレフィックスに対して、`node1`と`node2`の両方がアドバタイズ
- `m_rteList`には`RoutingTablePoolEntry(node1)`と`RoutingTablePoolEntry(node2)`が含まれる
- `generateNhlfromRteList()`で統合された`NexthopList`には、両方の`NextHop`が含まれる
- `addEntry(name="/relay", destRouter="/ndn/node1")`を呼び出すと、`node2`経由の`NextHop`にも`node1`のFunctionCostが適用される

### 5.5 呼び出し箇所

1. **`addEntry()`**: 新しいエントリを追加する際
2. **`removeEntry()`**: エントリを削除する際
3. **`updateFromLsdb()`**: LSA更新時に既存エントリを更新する際

すべての呼び出し箇所で、同じ問題が発生する可能性がある。

---

## 6. データフローと問題の全体像

### 6.1 正常なデータフロー

```
1. RoutingTable計算
   └─ RoutingTableEntry(node1) → NexthopList {NextHop(face1, cost1)}
   └─ RoutingTableEntry(node2) → NexthopList {NextHop(face2, cost2)}

2. NamePrefixTable::addEntry("/relay", "node1")
   └─ RoutingTablePoolEntry(node1)を作成/取得
   └─ NamePrefixTableEntry(/relay)に追加

3. NamePrefixTable::addEntry("/relay", "node2")
   └─ RoutingTablePoolEntry(node2)を作成/取得
   └─ NamePrefixTableEntry(/relay)に追加

4. generateNhlfromRteList()
   └─ RoutingTablePoolEntry(node1)のNextHop + RoutingTablePoolEntry(node2)のNextHop
   └─ 統合されたNexthopList {NextHop(face1, cost1), NextHop(face2, cost2)}

5. adjustNexthopCosts(nhlist, "/relay", "node1")  ← 問題: node1を指定しているが...
   └─ node1のFunctionCostを計算
   └─ すべてのNextHopに適用  ← 問題: node2経由のNextHopにもnode1のFunctionCostが適用される
```

### 6.2 問題の根本原因

1. **情報の損失**: `generateNhlfromRteList()`で統合する際、各`NextHop`がどの`destRouterName`に対応するかの情報が失われる
2. **不適切な呼び出し**: `adjustNexthopCosts()`に1つの`destRouterName`を渡すが、`nhlist`には複数の`destRouterName`に対応する`NextHop`が含まれる
3. **構造的な制約**: `NextHop`構造体に`destRouterName`情報が含まれていない

---

## 7. 解決策の方向性

### 7.1 必要な情報

各`NextHop`に対して、以下の情報が必要:
- どの`destRouterName`に対応するか
- その`destRouterName`の`NameLSA`から`ServiceFunctionInfo`を取得
- その`ServiceFunctionInfo`を使用してFunctionCostを計算

### 7.2 解決策の選択肢

1. **案1: `adjustNexthopCosts()`のシグネチャを変更**
   - `NamePrefixTableEntry`への参照を追加
   - `m_rteList`から各`NextHop`の対応する`destRouterName`を判断

2. **案2: `NextHop`に`destRouterName`情報を追加**
   - `NextHop`構造体を拡張
   - メモリ使用量が増加する可能性

3. **案3: `adjustNexthopCosts()`の呼び出し方を変更**
   - 各`RoutingTablePoolEntry`に対して個別に呼び出す
   - 呼び出し元の修正が必要

---

## 8. まとめ

### 8.1 各要素の関係

```
NamePrefixTableEntry (/relay)
  ├─ RoutingTablePoolEntry (node1) → NexthopList {NextHop(face1, cost1)}
  │     └─ m_destination = /ndn/node1  ← destRouterName情報
  └─ RoutingTablePoolEntry (node2) → NexthopList {NextHop(face2, cost2)}
        └─ m_destination = /ndn/node2  ← destRouterName情報
              ↓ generateNhlfromRteList()
  NexthopList {NextHop(face1, cost1), NextHop(face2, cost2)}
              ↓ 問題: destRouterName情報が失われる
  adjustNexthopCosts(nhlist, "/relay", "node1")
              ↓ 問題: node2経由のNextHopにもnode1のFunctionCostが適用される
```

### 8.2 問題の本質

- **情報の損失**: `generateNhlfromRteList()`で統合する際、各`NextHop`の`destRouterName`対応関係が失われる
- **不適切な設計**: `adjustNexthopCosts()`が1つの`destRouterName`しか受け取れないが、`nhlist`には複数の`destRouterName`に対応する`NextHop`が含まれる

### 8.3 解決に必要な変更

1. 各`NextHop`がどの`destRouterName`に対応するかを判断する方法を提供
2. `adjustNexthopCosts()`を、各`NextHop`に対して個別にFunctionCostを計算できるように修正

