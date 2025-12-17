2025-12-12

# node3でのファンクションコスト計算フロー

## 概要

node3でファンクションコストの計算が起こるときのフローを説明します。特に以下の2点について詳しく説明します：

1. **node3で、コスト計算対象のプレフィックスがSFC関連のものであると認識する方法**
2. **node1, node2の新しいコストの上下が起こった際のコストの更新**

---

## ① node3で、コスト計算対象のプレフィックスがSFC関連のものであると認識する方法

### フロー概要

```
設定ファイル読み込み
  ↓
function-prefixの設定確認
  ↓
（未設定の場合）デフォルトで/relayを追加
  ↓
adjustNexthopCostsでプレフィックスマッチング
  ↓
SFC関連と判断された場合のみFunctionCostを計算
```

### 詳細な処理フロー

#### 1. 設定ファイルの読み込み（起動時）

**ファイル:** `conf-file-processor.cpp`

```cpp
// 756-760行目
// If no function-prefix was specified, add default /relay for backward compatibility
if (!foundFunctionPrefix) {
  m_confParam.addServiceFunctionPrefix(ndn::Name("/relay"));
  std::cerr << "No function-prefix specified, using default: /relay" << std::endl;
}
```

**動作:**
- node3の設定ファイル（`nlsr-node3.conf`）には`function-prefix`の設定がない
- しかし、デフォルトで`/relay`がサービスファンクションプレフィックスとして追加される
- これにより、node3でも`/relay`がサービスファンクションプレフィックスとして認識される

#### 2. adjustNexthopCostsでのプレフィックスチェック

**ファイル:** `name-prefix-table.cpp` (182-198行目)

```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, const ndn::Name& nameToCheck, const NamePrefixTableEntry& npte)
{
  // サービスファンクションに関連のないNextHopの場合は、FunctionCostを適用せずにそのまま返す
  const auto& serviceFunctionPrefixes = m_confParam.getServiceFunctionPrefixes();
  bool isServiceFunction = false;
  for (const auto& prefix : serviceFunctionPrefixes) {
    if (nameToCheck.isPrefixOf(prefix) || prefix.isPrefixOf(nameToCheck)) {
      isServiceFunction = true;
      break;
    }
  }
  
  if (!isServiceFunction) {
    NLSR_LOG_DEBUG("adjustNexthopCosts: " << nameToCheck << " is not a service function prefix, returning original NextHopList");
    return nhlist;
  }
  
  // サービスファンクション関連のNextHopのみ、FunctionCostを計算
  // ...
}
```

**動作:**
1. `m_confParam.getServiceFunctionPrefixes()`で、設定ファイルから読み込まれたサービスファンクションプレフィックスのリストを取得
2. `nameToCheck`（例: `/relay`）が、いずれかのサービスファンクションプレフィックスと一致するかチェック
   - `nameToCheck.isPrefixOf(prefix)`: `nameToCheck`が`prefix`のプレフィックスか
   - `prefix.isPrefixOf(nameToCheck)`: `prefix`が`nameToCheck`のプレフィックスか
3. 一致する場合、`isServiceFunction = true`となり、FunctionCostを計算する
4. 一致しない場合、元の`NexthopList`をそのまま返す（FunctionCostを計算しない）

**例:**
- `nameToCheck = /relay`, `serviceFunctionPrefixes = {/relay}`
  - `/relay.isPrefixOf(/relay)` → `true` → FunctionCostを計算 ✓
- `nameToCheck = /sample2.txt`, `serviceFunctionPrefixes = {/relay}`
  - どちらも`false` → FunctionCostを計算しない ✓
- `nameToCheck = /relay/sub1`, `serviceFunctionPrefixes = {/relay}`
  - `/relay.isPrefixOf(/relay/sub1)` → `true` → FunctionCostを計算 ✓

---

## ② node1, node2の新しいコストの上下が起こった際のコストの更新

### フロー概要

```
node1/node2: サイドカーログ更新
  ↓
SidecarStatsHandler: ログ解析・ServiceFunctionInfo計算
  ↓
NameLSA更新（ServiceFunctionInfo設定）
  ↓
NameLSA再構築・インストール（LSDBに反映）
  ↓
NameLSA同期（PSync経由でnode3に送信）
  ↓
node3: NameLSA受信
  ↓
Lsdb::installLsa（LSDBにインストール）
  ↓
Lsdb::onLsdbModified（LSDB変更通知）
  ↓
NamePrefixTable::updateFromLsdb（Name Prefix Table更新）
  ↓
NamePrefixTable::adjustNexthopCosts（FunctionCost計算）
  ↓
Fib::update（FIB更新）
```

### 詳細な処理フロー

#### ステップ1: node1/node2でのサイドカーログ更新

**ファイル:** `sidecar-stats-handler.cpp`

**処理:**
1. `SidecarStatsHandler`が5秒ごとにログファイルを監視
2. ログファイルのハッシュ値が変更された場合、ログを解析
3. `parseSidecarLogWithTimeWindow`でログエントリを解析
4. `convertStatsToServiceFunctionInfo`でServiceFunctionInfoを計算
   - `processingTime`: 処理時間の合計 / 時間窓の長さ
   - `load`, `usageCount`: ログから取得
   - `lastUpdateTime`: 最新のログエントリのタイムスタンプ

#### ステップ2: NameLSA更新

**ファイル:** `sidecar-stats-handler.cpp` (843-920行目)

```cpp
void
SidecarStatsHandler::updateNameLsaWithStats()
{
  // ServiceFunctionInfoを計算
  ServiceFunctionInfo sfInfo = convertStatsToServiceFunctionInfo();
  
  // NameLSAを取得
  auto nameLsa = m_lsdb->findLsa<NameLsa>(m_confParam->getRouterPrefix());
  
  // ServiceFunctionInfoを設定
  nameLsa->setServiceFunctionInfo(serviceFunctionPrefix, sfInfo);
  
  // NameLSAを再構築・インストール
  m_lsdb->rebuildAndInstallOwnNameLsa();
}
```

**動作:**
- `nameLsa->setServiceFunctionInfo`でServiceFunctionInfoを設定
- `m_lsdb->rebuildAndInstallOwnNameLsa()`でNameLSAを再構築・インストール

#### ステップ3: NameLSA同期（PSync経由）

**ファイル:** `lsdb.cpp`

**処理:**
- PSyncプロトコルを使用して、NameLSAの更新を他のノード（node3）に同期
- node3がNameLSAの更新を受信

#### ステップ4: node3でのNameLSA受信・インストール

**ファイル:** `lsdb.cpp` (271-329行目)

```cpp
void
Lsdb::installLsa(std::shared_ptr<Lsa> lsa)
{
  // ...
  // Else this is a known name LSA, so we are updating it.
  else if (chkLsa->getSeqNo() < lsa->getSeqNo()) {
    // Update existing LSA
    chkLsa->update(*lsa);
    
    // ...
    
    // Pass the updated LSA (chkLsa) instead of the new LSA (lsa)
    // This ensures that updateFromLsdb receives the actual updated LSA from LSDB
    onLsdbModified(chkLsa, LsdbUpdate::UPDATED, namesToAdd, namesToRemove);
  }
}
```

**動作:**
1. `chkLsa->update(*lsa)`で既存のNameLSAを更新
   - `NameLsa::update()`でServiceFunctionInfoが保持される（空の場合は既存の情報を保持）
2. `onLsdbModified`でLSDB変更を通知

#### ステップ5: Name Prefix Table更新

**ファイル:** `name-prefix-table.cpp` (108-152行目)

```cpp
void
NamePrefixTable::updateFromLsdb(const ndn::Name& lsaKey, LsdbUpdate updateType)
{
  // ...
  
  if (updateType == LsdbUpdate::UPDATED && lsa->getType() == Lsa::Type::NAME) {
    auto nlsa = std::static_pointer_cast<NameLsa>(lsa);
    
    // Check if Service Function information exists in the updated NameLSA
    const auto& allSfInfo = nlsa->getAllServiceFunctionInfo();
    bool hasSfInfo = false;
    for (const auto& [serviceName, sfInfo] : allSfInfo) {
      if (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0) {
        hasSfInfo = true;
        break;
      }
    }
    
    if (hasSfInfo || !allSfInfo.empty()) {
      // Find all entries for this router and update them
      for (auto& entry : m_table) {
        auto rtpeList = entry->getRteList();
        for (const auto& rtpe : rtpeList) {
          if (rtpe->getDestination() == lsa->getOriginRouter()) {
            entry->generateNhlfromRteList();
            if (entry->getNexthopList().size() > 0) {
              m_fib.update(entry->getNamePrefix(), 
                          adjustNexthopCosts(entry->getNexthopList(), entry->getNamePrefix(), *entry));
            }
          }
        }
      }
    }
  }
}
```

**動作:**
1. 更新されたNameLSAにServiceFunctionInfoが含まれているかチェック
2. 含まれている場合、該当するルーター（node1またはnode2）のエントリを更新
3. `adjustNexthopCosts`を呼び出してFunctionCostを計算
4. `Fib::update`でFIBを更新

#### ステップ6: FunctionCost計算

**ファイル:** `name-prefix-table.cpp` (200-332行目)

```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, const ndn::Name& nameToCheck, const NamePrefixTableEntry& npte)
{
  // サービスファンクションプレフィックスかチェック（前述）
  // ...
  
  // 各RoutingTablePoolEntryに対して個別にFunctionCostを計算
  for (const auto& rtpe : npte.getRteList()) {
    const ndn::Name& destRouterName = rtpe->getDestination();
    
    // destRouterNameのNameLSAからFunctionCostを計算
    double functionCost = 0.0;
    auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
    if (nameLsa) {
      ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
      
      // Check if Service Function info is stale
      bool isStale = false;
      if (sfInfo.lastUpdateTime != ndn::time::system_clock::time_point::min()) {
        auto now = ndn::time::system_clock::now();
        auto timeSinceLastUpdate = boost::chrono::duration_cast<boost::chrono::seconds>(now - sfInfo.lastUpdateTime).count();
        uint32_t staleThreshold = m_confParam.getUtilizationWindowSeconds() * 2;
        
        if (timeSinceLastUpdate > static_cast<int64_t>(staleThreshold)) {
          isStale = true;
        }
      }
      
      // Calculate function cost if Service Function info is available and not stale
      if (!isStale && (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0)) {
        double processingWeight = sfInfo.processingWeight;
        double loadWeight = sfInfo.loadWeight;
        double usageWeight = sfInfo.usageWeight;
        
        functionCost = sfInfo.processingTime * processingWeight +
                      sfInfo.load * loadWeight +
                      (sfInfo.usageCount / 100.0) * usageWeight;
      }
    }
    
    // このRoutingTablePoolEntryのNextHopに対してFunctionCostを適用
    for (const auto& nh : rtpe->getNexthopList().getNextHops()) {
      double originalCost = nh.getRouteCost();
      double nexthopCost = m_nexthopCost[DestNameKey(destRouterName, nameToCheck)];
      double adjustedCost = originalCost + nexthopCost + functionCost;
      
      // NextHopを更新（FaceUri, destRouterNameのペアで区別）
      // ...
    }
  }
  
  return new_nhList;
}
```

**動作:**
1. 各`RoutingTablePoolEntry`（node1経由、node2経由など）に対して、個別にFunctionCostを計算
2. `destRouterName`（node1またはnode2）のNameLSAから`ServiceFunctionInfo`を取得
3. 古い情報（`lastUpdateTime`が`utilization-window-seconds * 2`より古い）の場合は`functionCost = 0`
4. 有効な情報の場合、`functionCost = processingTime * processingWeight + load * loadWeight + (usageCount / 100.0) * usageWeight`
5. `adjustedCost = originalCost + nexthopCost + functionCost`でコストを調整
6. `(FaceUri, destRouterName)`のペアをキーとして、NextHopを更新

#### ステップ7: FIB更新

**ファイル:** `fib.cpp`

**処理:**
- `Fib::update`で、計算された`adjustedCost`を使用してFIBエントリを更新
- NFDにルーティング情報を登録

---

## まとめ

### ① SFC関連プレフィックスの認識

1. **設定ファイル読み込み時**: `function-prefix`が未設定の場合、デフォルトで`/relay`が追加される
2. **adjustNexthopCosts呼び出し時**: `m_confParam.getServiceFunctionPrefixes()`でプレフィックスリストを取得し、`nameToCheck`とマッチング
3. **マッチング結果**: 一致する場合のみFunctionCostを計算、一致しない場合は元のコストをそのまま使用

### ② コスト更新フロー

1. **node1/node2**: サイドカーログ更新 → ServiceFunctionInfo計算 → NameLSA更新
2. **同期**: PSync経由でnode3にNameLSA更新を送信
3. **node3**: NameLSA受信 → LSDB更新 → Name Prefix Table更新 → FunctionCost計算 → FIB更新

### 重要なポイント

- **node3は設定ファイルに`function-prefix`がなくても、デフォルトで`/relay`を認識する**
- **node1, node2のコスト更新は、NameLSAの更新を通じてnode3に伝播する**
- **FunctionCostは、各`destRouterName`（node1, node2）ごとに個別に計算される**
- **同じ`FaceUri`でも異なる`destRouterName`のNextHopは区別される**

