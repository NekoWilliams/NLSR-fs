2025-12-12

# 分散制御原則への適合性評価

## プロジェクトの目標

**ファンクションコストに基づく経路選択が、各ルータの独立した判断のもと完全に分散的に制御されること**

## 分散制御の原則

1. **各ルータの独立性**: 各ルータは外部の中央制御に依存せず、自分のローカル情報のみを使用して判断する
2. **ローカル情報の使用**: 各ルータは自分のLSDB（Link State Database）に保存された情報のみを使用する
3. **自律的な判断**: 各ルータは他のルータと通信することなく、自分の判断で経路選択を行う
4. **情報の分散**: 必要な情報（Service Function情報など）は各ルータのLSDBに分散して保存されている

## 現在の実装の分散制御性

### LSDB（Link State Database）の役割

- **各ルータがローカルに保持**: 各ルータは自分のLSDBを保持し、他のルータから受信したLSAを保存する
- **分散情報の集約**: 各ルータはLSDBから他のルータの`NameLSA`を取得し、そこに含まれる`ServiceFunctionInfo`を使用する
- **独立した判断**: 各ルータは自分のLSDBの情報のみを使用してFunctionCostを計算する

### FunctionCost計算の分散性

```cpp
// 各ルータが自分のLSDBから情報を取得
auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);

// 各ルータが独立してFunctionCostを計算
functionCost = sfInfo.processingTime * sfInfo.processingWeight +
               sfInfo.load * sfInfo.loadWeight +
               (sfInfo.usageCount / 100.0) * sfInfo.usageWeight;
```

**評価**: ✅ **完全に分散制御の原則に即している**
- 各ルータは自分のLSDBから情報を取得
- 外部の中央制御に依存しない
- 各ルータが独立して判断している

## 各案の分散制御性評価

### 案1: adjustNexthopCostsのシグネチャを変更し、NamePrefixTableEntryへの参照を追加

**実装内容:**
```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, 
                                     const ndn::Name& nameToCheck, 
                                     const NamePrefixTableEntry& npte)
{
  // 各RoutingTablePoolEntryに対して個別にFunctionCostを計算
  for (const auto& rtpe : npte.getRteList()) {
    const ndn::Name& destRouterName = rtpe->getDestination();
    
    // 各ルータが自分のLSDBから情報を取得
    auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    
    // 各ルータが独立してFunctionCostを計算
    functionCost = sfInfo.processingTime * sfInfo.processingWeight + ...;
  }
}
```

**分散制御性の評価:**

| 評価項目 | 評価 | 説明 |
|---------|------|------|
| 各ルータの独立性 | ✅ 完全に適合 | 各ルータは自分のLSDBから情報を取得し、独立して判断する |
| ローカル情報の使用 | ✅ 完全に適合 | `m_lsdb`は各ルータのローカルLSDBで、外部に依存しない |
| 自律的な判断 | ✅ 完全に適合 | 各ルータは他のルータと通信することなく、自分の判断でFunctionCostを計算する |
| 情報の分散 | ✅ 完全に適合 | Service Function情報は各ルータの`NameLSA`に分散して保存されている |

**結論**: ✅ **完全に分散制御の原則に即している**

---

### 案2: adjustNexthopCostsの呼び出し方を変更

**実装内容:**
```cpp
// 各RoutingTablePoolEntryに対して個別にadjustNexthopCostsを呼び出す
for (const auto& rtpe : entry->getRteList()) {
  NexthopList adjustedList = adjustNexthopCosts(
    rtpe->getNexthopList(), 
    entry->getNamePrefix(), 
    rtpe->getDestination()  // destRouterName
  );
  // ...
}
```

**分散制御性の評価:**

| 評価項目 | 評価 | 説明 |
|---------|------|------|
| 各ルータの独立性 | ✅ 完全に適合 | 各ルータは自分のLSDBから情報を取得し、独立して判断する |
| ローカル情報の使用 | ✅ 完全に適合 | `m_lsdb`は各ルータのローカルLSDBで、外部に依存しない |
| 自律的な判断 | ✅ 完全に適合 | 各ルータは他のルータと通信することなく、自分の判断でFunctionCostを計算する |
| 情報の分散 | ✅ 完全に適合 | Service Function情報は各ルータの`NameLSA`に分散して保存されている |

**結論**: ✅ **完全に分散制御の原則に即している**

---

### 案3: NextHop構造体にdestRouterName情報を追加

**実装内容:**
```cpp
// NextHopにdestRouterNameを追加
class NextHop {
  ndn::Name m_destRouterName;  // 追加
  // ...
};

// adjustNexthopCostsで使用
for (const auto& nh : nhlist.getNextHops()) {
  const ndn::Name& destRouterName = nh.getDestRouterName();
  
  // 各ルータが自分のLSDBから情報を取得
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  // ...
}
```

**分散制御性の評価:**

| 評価項目 | 評価 | 説明 |
|---------|------|------|
| 各ルータの独立性 | ✅ 完全に適合 | 各ルータは自分のLSDBから情報を取得し、独立して判断する |
| ローカル情報の使用 | ✅ 完全に適合 | `m_lsdb`は各ルータのローカルLSDBで、外部に依存しない |
| 自律的な判断 | ✅ 完全に適合 | 各ルータは他のルータと通信することなく、自分の判断でFunctionCostを計算する |
| 情報の分散 | ✅ 完全に適合 | Service Function情報は各ルータの`NameLSA`に分散して保存されている |

**結論**: ✅ **完全に分散制御の原則に即している**

---

### 案4: generateNhlfromRteListを変更して、destRouterName情報を保持

**実装内容:**
```cpp
// NamePrefixTableEntryにマップを追加
class NamePrefixTableEntry {
  std::map<ndn::FaceUri, ndn::Name> m_nexthopToDestRouterMap;  // 追加
  // ...
};

// adjustNexthopCostsで使用
for (const auto& nh : nhlist.getNextHops()) {
  const ndn::Name& destRouterName = m_nexthopToDestRouterMap[nh.getConnectingFaceUri()];
  
  // 各ルータが自分のLSDBから情報を取得
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  // ...
}
```

**分散制御性の評価:**

| 評価項目 | 評価 | 説明 |
|---------|------|------|
| 各ルータの独立性 | ✅ 完全に適合 | 各ルータは自分のLSDBから情報を取得し、独立して判断する |
| ローカル情報の使用 | ✅ 完全に適合 | `m_lsdb`は各ルータのローカルLSDBで、外部に依存しない |
| 自律的な判断 | ✅ 完全に適合 | 各ルータは他のルータと通信することなく、自分の判断でFunctionCostを計算する |
| 情報の分散 | ✅ 完全に適合 | Service Function情報は各ルータの`NameLSA`に分散して保存されている |

**結論**: ✅ **完全に分散制御の原則に即している**

---

## 総合評価

### すべての案が分散制御の原則に完全に適合

**理由:**

1. **LSDBの分散性**: すべての案で、各ルータは自分のローカルLSDB（`m_lsdb`）から`NameLSA`を取得している。LSDBは各ルータがローカルに保持するデータベースで、他のルータから受信したLSAを保存している。これは完全に分散的な情報管理である。

2. **独立した判断**: すべての案で、各ルータは外部の中央制御に依存せず、自分のLSDBの情報のみを使用してFunctionCostを計算している。

3. **情報の分散**: Service Function情報（`processingTime`、`load`、`usageCount`、`processingWeight`など）は各ルータの`NameLSA`に含まれており、LSDBを通じて他のルータに分散されている。

4. **自律的な経路選択**: 各ルータは他のルータと直接通信することなく、自分のLSDBの情報のみを使用して経路選択を行う。

### 分散制御の原則に反する要素は存在しない

- ❌ 中央制御サーバーへの依存: なし
- ❌ 外部APIへの依存: なし
- ❌ 他のルータとの直接通信: なし（LSDBへの情報の保存は、NLSRの標準的なLSA同期メカニズムによる）
- ❌ グローバルな状態管理: なし

### 結論

**すべての案（案1〜案4）は、プロジェクトの目標である「ファンクションコストに基づく経路選択が、各ルータの独立した判断のもと完全に分散的に制御されること」に完全に適合しています。**

各案の違いは、実装方法やコードの構造のみであり、分散制御の原則への適合性には差がありません。

## 推奨事項

分散制御の原則への適合性はすべての案で同等であるため、実装の難易度、コードの保守性、既存コードへの影響などを考慮して選択することを推奨します。

