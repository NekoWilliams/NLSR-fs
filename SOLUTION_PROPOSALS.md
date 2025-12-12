2025-12-12

# 解決策の提案

## 問題の再確認

現在の問題：
- `adjustNexthopCosts`は1つの`destRouterName`に対して呼ばれる
- しかし、`nhlist`には複数のNextHopが含まれており、それぞれが異なる`destRouterName`（node1とnode2）への経路を含む可能性がある
- 現在の実装では、すべてのNextHopに対して同じ`destRouterName`のFunctionCostを計算している

## 解決策の案

### 案1: adjustNexthopCostsのシグネチャを変更し、NamePrefixTableEntryへの参照を追加

**変更内容:**
- `adjustNexthopCosts`のシグネチャを変更し、`NamePrefixTableEntry`への参照を追加
- 各NextHopがどの`RoutingTablePoolEntry`（`destRouterName`）に属するかを判断
- 各`RoutingTablePoolEntry`に対して個別にFunctionCostを計算

**実装方法:**
```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, 
                                     const ndn::Name& nameToCheck, 
                                     const NamePrefixTableEntry& npte)
{
  NexthopList new_nhList;
  
  // 各RoutingTablePoolEntryに対して個別にFunctionCostを計算
  for (const auto& rtpe : npte.getRteList()) {
    const ndn::Name& destRouterName = rtpe->getDestination();
    double functionCost = 0.0;
    
    // destRouterNameのNameLSAからFunctionCostを計算
    auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
    if (nameLsa) {
      ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
      if (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0) {
        functionCost = sfInfo.processingTime * sfInfo.processingWeight +
                      sfInfo.load * sfInfo.loadWeight +
                      (sfInfo.usageCount / 100.0) * sfInfo.usageWeight;
      }
    }
    
    // このRoutingTablePoolEntryのNextHopに対してFunctionCostを適用
    for (const auto& nh : rtpe->getNexthopList().getNextHops()) {
      double originalCost = nh.getRouteCost();
      double nexthopCost = m_nexthopCost[DestNameKey(destRouterName, nameToCheck)];
      double adjustedCost = originalCost + nexthopCost + functionCost;
      
      const NextHop newNextHop = NextHop(nh.getConnectingFaceUri(), adjustedCost);
      new_nhList.addNextHop(newNextHop);
    }
  }
  
  return new_nhList;
}
```

**メリット:**
- 各NextHopがどの`destRouterName`に対応するかを正確に判断可能
- 各`destRouterName`に対して個別にFunctionCostを計算可能

**デメリット:**
- `adjustNexthopCosts`のシグネチャ変更が必要
- すべての呼び出し元を修正する必要がある（4箇所）

**実装難易度:** 中

---

### 案2: adjustNexthopCostsの呼び出し方を変更

**変更内容:**
- 各`RoutingTablePoolEntry`に対して個別に`adjustNexthopCosts`を呼び出す
- 結果をマージしてFIBに反映

**実装方法:**
```cpp
// updateFromLsdb内で
if (entry->getNexthopList().size() > 0) {
  NexthopList mergedNexthopList;
  
  // 各RoutingTablePoolEntryに対して個別にadjustNexthopCostsを呼び出す
  for (const auto& rtpe : entry->getRteList()) {
    if (rtpe->getDestination() == lsa->getOriginRouter()) {
      NexthopList adjustedList = adjustNexthopCosts(
        rtpe->getNexthopList(), 
        entry->getNamePrefix(), 
        rtpe->getDestination()
      );
      
      // 結果をマージ
      for (const auto& nh : adjustedList.getNextHops()) {
        mergedNexthopList.addNextHop(nh);
      }
    }
  }
  
  m_fib.update(entry->getNamePrefix(), mergedNexthopList);
}
```

**メリット:**
- `adjustNexthopCosts`のシグネチャ変更が不要
- 各`destRouterName`に対して個別にFunctionCostを計算可能

**デメリット:**
- 呼び出し元の変更が必要（4箇所）
- 複数の呼び出し結果をマージする処理が必要
- `addEntry`や`removeEntry`でも同様の変更が必要

**実装難易度:** 中

---

### 案3: NextHop構造体にdestRouterName情報を追加

**変更内容:**
- `NextHop`構造体に`destRouterName`メンバーを追加
- `generateNhlfromRteList`で各NextHopに`destRouterName`を設定

**実装方法:**
```cpp
// nexthop.hpp
class NextHop {
  // ...
  void setDestRouterName(const ndn::Name& destRouterName);
  const ndn::Name& getDestRouterName() const;
  
private:
  ndn::Name m_destRouterName;  // 追加
  // ...
};

// name-prefix-table-entry.cpp
void NamePrefixTableEntry::generateNhlfromRteList()
{
  m_nexthopList.clear();
  for (auto iterator = m_rteList.begin(); iterator != m_rteList.end(); ++iterator) {
    const ndn::Name& destRouterName = (*iterator)->getDestination();
    for (auto nhItr = (*iterator)->getNexthopList().getNextHops().begin();
         nhItr != (*iterator)->getNexthopList().getNextHops().end();
         ++nhItr) {
      NextHop nh = *nhItr;
      nh.setDestRouterName(destRouterName);  // destRouterNameを設定
      m_nexthopList.addNextHop(nh);
    }
  }
}

// name-prefix-table.cpp
for (const auto& nh : nhlist.getNextHops()) {
  const ndn::Name& destRouterName = nh.getDestRouterName();  // destRouterNameを取得
  // ...
}
```

**メリット:**
- 各NextHopに対応する`destRouterName`が明確
- `adjustNexthopCosts`の実装がシンプルになる

**デメリット:**
- 大きな変更が必要
- 既存のコードへの影響が大きい
- `NextHop`の比較演算子なども変更が必要
- `NextHop`のシリアライゼーション（wireEncode/wireDecode）も変更が必要

**実装難易度:** 高

---

### 案4: generateNhlfromRteListを変更して、destRouterName情報を保持

**変更内容:**
- `NexthopList`を拡張して、各NextHopに対応する`destRouterName`を保持するマップを追加
- `adjustNexthopCosts`でこのマップを使用

**実装方法:**
```cpp
// name-prefix-table-entry.hpp
class NamePrefixTableEntry {
  // ...
  std::map<ndn::FaceUri, ndn::Name> m_nexthopToDestRouterMap;  // 追加
  // ...
};

// name-prefix-table-entry.cpp
void NamePrefixTableEntry::generateNhlfromRteList()
{
  m_nexthopList.clear();
  m_nexthopToDestRouterMap.clear();  // クリア
  
  for (auto iterator = m_rteList.begin(); iterator != m_rteList.end(); ++iterator) {
    const ndn::Name& destRouterName = (*iterator)->getDestination();
    for (auto nhItr = (*iterator)->getNexthopList().getNextHops().begin();
         nhItr != (*iterator)->getNexthopList().getNextHops().end();
         ++nhItr) {
      m_nexthopList.addNextHop((*nhItr));
      m_nexthopToDestRouterMap[nhItr->getConnectingFaceUri()] = destRouterName;  // マップに追加
    }
  }
}
```

**メリット:**
- 各NextHopに対応する`destRouterName`を保持可能
- `NextHop`構造体の変更が不要

**デメリット:**
- `NamePrefixTableEntry`の構造変更が必要
- `FaceUri`が同じで`destRouterName`が異なる場合の処理が複雑

**実装難易度:** 中〜高

---

## 推奨案

**案1**を推奨します。理由：
1. 各NextHopがどの`destRouterName`に対応するかを正確に判断可能
2. 実装が比較的シンプル
3. 既存のコードへの影響が限定的（呼び出し元の修正のみ）

ただし、**案2**も実用的です。`adjustNexthopCosts`のシグネチャ変更が不要で、各`RoutingTablePoolEntry`に対して個別に処理できるためです。

## 完全な解決が難しい場合

現在の実装では、`nhlist`に含まれるNextHopがどの`destRouterName`に対応するかを判断する情報が不足しています。そのため、以下のいずれかの変更が必要です：

1. `adjustNexthopCosts`に追加の情報（`NamePrefixTableEntry`への参照など）を渡す
2. `NextHop`構造体に`destRouterName`情報を追加する
3. `adjustNexthopCosts`の呼び出し方を変更し、各`destRouterName`に対して個別に呼び出す

これらの変更なしには、完全な解決は困難です。

