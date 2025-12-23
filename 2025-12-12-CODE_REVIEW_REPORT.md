2025-12-12

# NLSR-fs コードレビューレポート

## 確認事項

### ① ソースコードに環境特有の記述（/relayなど）が含まれていないか

#### 検索結果

**問題のある箇所**:

1. **`src/publisher/sidecar-stats-handler.cpp:839`**
   ```cpp
   return ndn::Name("/relay");
   ```
   - **問題**: ハードコードされた`/relay`プレフィックス
   - **用途**: `getServiceFunctionName()`関数内で使用
   - **影響**: この環境特有の記述が含まれている

2. **`src/conf-parameter.hpp:596`**
   ```cpp
   return ndn::Name("/relay");  // Default for backward compatibility
   ```
   - **問題**: ハードコードされた`/relay`プレフィックス（後方互換性のためのデフォルト値）
   - **用途**: `getFirstServiceFunctionPrefix()`関数内で使用
   - **影響**: 設定ファイルに`function-prefix`が指定されていない場合のデフォルト値

#### 良い実装

1. **`src/route/name-prefix-table.cpp`**
   - サービスファンクションの判定は`PrefixInfo::isServiceFunction()`フラグを使用
   - ハードコードされたプレフィックス名に依存していない ✓

2. **`src/lsdb.cpp`**
   - `m_confParam.isServiceFunctionPrefix(prefixName)`を使用して判定
   - 設定ファイルベースの判定 ✓

3. **`src/conf-file-processor.cpp`**
   - `function-prefix`を設定ファイルから読み込み
   - ハードコードされたデフォルト値の追加を削除済み ✓

#### 推奨される修正

**修正1: `sidecar-stats-handler.cpp`**
```cpp
// 修正前
ndn::Name
SidecarStatsHandler::getServiceFunctionName() const
{
  return ndn::Name("/relay");
}

// 修正後: 設定ファイルから取得、または空のNameを返す
ndn::Name
SidecarStatsHandler::getServiceFunctionName() const
{
  if (m_confParam && !m_confParam->getServiceFunctionPrefixes().empty()) {
    return m_confParam->getServiceFunctionPrefixes().front();
  }
  return ndn::Name();  // 空のNameを返す
}
```

**修正2: `conf-parameter.hpp`**
```cpp
// 修正前
ndn::Name
getFirstServiceFunctionPrefix() const
{
  if (!m_serviceFunctionPrefixes.empty()) {
    return m_serviceFunctionPrefixes.front();
  }
  return ndn::Name("/relay");  // Default for backward compatibility
}

// 修正後: デフォルト値を削除
ndn::Name
getFirstServiceFunctionPrefix() const
{
  if (!m_serviceFunctionPrefixes.empty()) {
    return m_serviceFunctionPrefixes.front();
  }
  return ndn::Name();  // 空のNameを返す（設定ファイルで指定が必要）
}
```

### ② FunctionCostが正しく更新されるか、Node3/Node4でnode1/node2に対するコストが正しく計算される仕組みになっているか

#### FunctionCostの計算フロー

**1. サービスファンクションの判定**

`adjustNexthopCosts()`関数（`name-prefix-table.cpp:190-211`）:
```cpp
// NamePrefixTableEntry内の各RoutingTablePoolEntryを確認
for (const auto& rtpe : npte.getRteList()) {
  const ndn::Name& destRouterName = rtpe->getDestination();
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    const auto& prefixInfoList = nameLsa->getNpl().getPrefixInfo();
    for (const auto& prefixInfo : prefixInfoList) {
      if (prefixInfo.getName() == nameToCheck) {
        isServiceFunction = prefixInfo.isServiceFunction();
        break;
      }
    }
  }
}
```

**判定方法**: `PrefixInfo::isServiceFunction()`フラグを使用 ✓
- ハードコードされたプレフィックス名に依存していない
- NameLSAから動的に判定

**2. FunctionCostの計算**

各`RoutingTablePoolEntry`に対して個別に計算（`name-prefix-table.cpp:222-300`）:
```cpp
for (const auto& rtpe : npte.getRteList()) {
  const ndn::Name& destRouterName = rtpe->getDestination();
  
  // destRouterNameのNameLSAからFunctionCostを計算
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    
    // スタイルチェック
    bool isStale = false;
    if (sfInfo.lastUpdateTime != ndn::time::system_clock::time_point::min()) {
      auto timeSinceLastUpdate = ...;
      uint32_t staleThreshold = m_confParam.getUtilizationWindowSeconds() * 3;
      if (timeSinceLastUpdate > static_cast<int64_t>(staleThreshold)) {
        isStale = true;
      }
    }
    
    // FunctionCostを計算
    if (!isStale && (sfInfo.processingTime > 0.0 || ...)) {
      functionCost = sfInfo.processingTime * processingWeight +
                    sfInfo.load * loadWeight +
                    (sfInfo.usageCount / 100.0) * usageWeight;
    }
  }
}
```

**計算方法**: 
- 各`destRouterName`（node1, node2）に対して個別に計算 ✓
- 各`destRouterName`のNameLSAから`ServiceFunctionInfo`を取得 ✓
- スタイルチェックを実施 ✓

**3. NextHopへの適用**

各NextHopに対してFunctionCostを適用（`name-prefix-table.cpp:302-350`）:
```cpp
for (const auto& nh : rtpe->getNexthopList().getNextHops()) {
  double originalCost = nh.getRouteCost();
  double nexthopCost = m_nexthopCost[DestNameKey(destRouterName, nameToCheck)];
  double adjustedCost = originalCost + nexthopCost + functionCost;
  
  // (FaceUri, destRouterName)のペアをキーとして使用
  auto key = std::make_pair(faceUri, destRouterName);
  nextHopMap[key] = std::make_pair(newNextHop, functionCost);
}
```

**適用方法**:
- `(FaceUri, destRouterName)`のペアをキーとして使用 ✓
- 同じFaceUriでも異なる`destRouterName`のNextHopを区別 ✓
- 各NextHopに対して個別にFunctionCostを適用 ✓

#### Node3/Node4でのコスト計算の仕組み

**Node3から/relayへのコスト計算**:

1. **Node3のNamePrefixTableEntry**:
   - `/relay`プレフィックスに対する`NamePrefixTableEntry`を取得
   - このエントリには、node1経由とnode2経由の2つの`RoutingTablePoolEntry`が含まれる

2. **各RoutingTablePoolEntryに対して**:
   - **node1経由**: 
     - `destRouterName = /ndn/jp/%C1.Router/node1`
     - node1のNameLSAから`ServiceFunctionInfo`を取得
     - FunctionCostを計算（node1の`processingTime`, `load`, `usageCount`を使用）
   - **node2経由**:
     - `destRouterName = /ndn/jp/%C1.Router/node2`
     - node2のNameLSAから`ServiceFunctionInfo`を取得
     - FunctionCostを計算（node2の`processingTime`, `load`, `usageCount`を使用）

3. **NextHopのコスト調整**:
   - node1経由のNextHop: `adjustedCost = originalCost + nexthopCost + functionCost(node1)`
   - node2経由のNextHop: `adjustedCost = originalCost + nexthopCost + functionCost(node2)`

**Node4から/relayへのコスト計算**:

同様のフローで、Node4からnode1/node2へのコストを計算 ✓

#### テスト結果からの確認

**テスト結果**（`2025-12-12-FUNCTION_COST_TEST_RESULTS.md`より）:

1. **Node3のFIBエントリ**:
   - node2経由: cost = 25402 → 28136 (FunctionCostが更新された) ✓
   - node1経由: cost = 50000 (FunctionCost = 0、staleと判定)

2. **Node4のFIBエントリ**:
   - node2経由: cost = 25000
   - node1経由: cost = 50402 → 78136 (FunctionCostが更新された) ✓

3. **FunctionCostの計算ログ**:
   ```
   Adjusting cost for /relay to /ndn/jp/%C1.Router/node2: 
     originalCost=25, nexthopCost=0, functionCost=0.402282, adjustedCost=25.4023
   ```
   - node2経由のFunctionCostが正しく計算されている ✓

## 結論

### ① 環境特有の記述について

**問題**: 2箇所で`/relay`がハードコードされている
- `sidecar-stats-handler.cpp:839`
- `conf-parameter.hpp:596`

**推奨**: 上記の修正を実施して、設定ファイルベースの実装に統一する

### ② FunctionCostの計算・更新について

**結論**: ✓ 正しく実装されている

1. **サービスファンクションの判定**: `PrefixInfo::isServiceFunction()`フラグを使用（ハードコードなし）
2. **FunctionCostの計算**: 各`destRouterName`（node1, node2）に対して個別に計算
3. **NextHopへの適用**: `(FaceUri, destRouterName)`のペアをキーとして使用し、各NextHopに対して個別に適用
4. **Node3/Node4での計算**: 両方のノードで、node1/node2に対するコストが正しく計算される仕組みになっている

**テスト結果**: FunctionCostが動的に更新され、FIBに反映されていることが確認された

## 推奨される対応

1. **環境特有の記述の削除**: ✅ 修正完了
   - `sidecar-stats-handler.cpp:839`: `/relay`のハードコードを削除、空のNameを返すように変更
   - `conf-parameter.hpp:596`: `/relay`のハードコードを削除、空のNameを返すように変更
2. **テストの継続**: より長期的なテストを実施して、FunctionCostの動的更新を継続的に監視

