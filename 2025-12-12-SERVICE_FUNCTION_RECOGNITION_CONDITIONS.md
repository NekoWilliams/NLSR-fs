2025-12-12

# node3で/relayがサービスファンクションとして認識される条件

## 概要

node3が`/relay`をサービスファンクションとして認識し、FunctionCostを計算する具体的な条件について詳しく説明します。

---

## 認識される条件の全体像

`/relay`がサービスファンクションとして認識され、FunctionCostが計算されるには、**2つの条件**を満たす必要があります：

1. **設定条件**: node3の設定ファイルで`/relay`がサービスファンクションプレフィックスとして登録されている
2. **実行条件**: `adjustNexthopCosts`が呼ばれたときに、`nameToCheck`（処理対象のプレフィックス）が`/relay`と一致する

---

## 条件1: 設定条件（起動時）

### フロー

```
設定ファイル読み込み
  ↓
service-functionセクションの確認
  ↓
function-prefixの有無をチェック
  ↓
（未設定の場合）デフォルトで/relayを追加
  ↓
m_confParam.getServiceFunctionPrefixes()に/relayが登録される
```

### 詳細

**ファイル:** `conf-file-processor.cpp` (756-760行目)

```cpp
// If no function-prefix was specified, add default /relay for backward compatibility
if (!foundFunctionPrefix) {
  m_confParam.addServiceFunctionPrefix(ndn::Name("/relay"));
  std::cerr << "No function-prefix specified, using default: /relay" << std::endl;
}
```

**動作:**
- node3の設定ファイル（`nlsr-node3.conf`）には`function-prefix`の設定がない
- しかし、デフォルトで`/relay`がサービスファンクションプレフィックスとして追加される
- これにより、`m_confParam.getServiceFunctionPrefixes()`には`{/relay}`が含まれる

**確認方法:**
- 起動時のログに`"No function-prefix specified, using default: /relay"`が出力される
- `m_confParam.getServiceFunctionPrefixes()`を呼ぶと、`/relay`が含まれている

---

## 条件2: 実行条件（adjustNexthopCosts呼び出し時）

### adjustNexthopCostsが呼ばれるタイミング

`adjustNexthopCosts`は、以下の**3つのタイミング**で呼ばれます：

#### タイミング1: NameLSA更新時（Service Function情報がある場合）

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
    
    // If Service Function info exists, update existing entries to recalculate FunctionCost
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
              //                                                              ↑
              //                                                         nameToCheck
            }
          }
        }
      }
    }
  }
}
```

**呼び出し条件:**
- NameLSAが更新された（`updateType == LsdbUpdate::UPDATED`）
- 更新されたNameLSAにService Function情報が含まれている（`hasSfInfo || !allSfInfo.empty()`）
- 該当するルーター（node1またはnode2）のエントリが存在する
- エントリにNextHopが存在する（`entry->getNexthopList().size() > 0`）

**このときの`nameToCheck`:**
- `entry->getNamePrefix()`が`nameToCheck`として渡される
- 例: `/relay`がアドバタイズされている場合、`nameToCheck = /relay`

#### タイミング2: 新しいプレフィックスが追加されたとき

**ファイル:** `name-prefix-table.cpp` (334-400行目)

```cpp
void
NamePrefixTable::addEntry(const ndn::Name& name, const ndn::Name& destRouter)
{
  // ...
  
  if (nameItr == m_table.end()) {
    // 新しいエントリを作成
    npte = std::make_shared<NamePrefixTableEntry>(name);
    npte->addRoutingTableEntry(rtpePtr);
    npte->generateNhlfromRteList();
    m_table.push_back(npte);

    // If this entry has next hops, we need to inform the FIB
    if (npte->getNexthopList().size() > 0) {
      m_fib.update(name, adjustNexthopCosts(npte->getNexthopList(), name, *npte));
      //                                                              ↑
      //                                                         nameToCheck
    }
  }
  // ...
}
```

**呼び出し条件:**
- 新しいプレフィックスがNameLSAでアドバタイズされた
- そのプレフィックスのエントリがName Prefix Tableに追加された
- エントリにNextHopが存在する（`npte->getNexthopList().size() > 0`）

**このときの`nameToCheck`:**
- `name`（新しく追加されたプレフィックス）が`nameToCheck`として渡される
- 例: node1が`/relay`をアドバタイズした場合、`nameToCheck = /relay`

#### タイミング3: 既存のプレフィックスに新しいルーターが追加されたとき

**ファイル:** `name-prefix-table.cpp` (412-427行目)

```cpp
void
NamePrefixTable::addEntry(const ndn::Name& name, const ndn::Name& destRouter)
{
  // ...
  
  else {
    // 既存のエントリに新しいルーターを追加
    npte = *nameItr;
    (*nameItr)->addRoutingTableEntry(rtpePtr);
    (*nameItr)->generateNhlfromRteList();

    if ((*nameItr)->getNexthopList().size() > 0) {
      m_fib.update(name, adjustNexthopCosts((*nameItr)->getNexthopList(), name, **nameItr));
      //                                                              ↑
      //                                                         nameToCheck
    }
  }
}
```

**呼び出し条件:**
- 既存のプレフィックス（例: `/relay`）に対して、新しいルーター（例: node2）がアドバタイズを開始した
- エントリにNextHopが存在する（`(*nameItr)->getNexthopList().size() > 0`）

**このときの`nameToCheck`:**
- `name`（既存のプレフィックス）が`nameToCheck`として渡される
- 例: node2が`/relay`をアドバタイズした場合、`nameToCheck = /relay`

---

## プレフィックスマッチングの詳細

### adjustNexthopCosts内での判定

**ファイル:** `name-prefix-table.cpp` (185-198行目)

```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, const ndn::Name& nameToCheck, const NamePrefixTableEntry& npte)
{
  // サービスファンクションに関連のないNextHopの場合は、FunctionCostを適用せずにそのまま返す
  const auto& serviceFunctionPrefixes = m_confParam.getServiceFunctionPrefixes();
  // ↑ 設定ファイルから読み込まれたサービスファンクションプレフィックスのリスト
  //   例: {/relay}
  
  bool isServiceFunction = false;
  for (const auto& prefix : serviceFunctionPrefixes) {
    // prefix: 設定ファイルで登録されたサービスファンクションプレフィックス（例: /relay）
    // nameToCheck: 処理対象のプレフィックス（例: /relay）
    
    if (nameToCheck.isPrefixOf(prefix) || prefix.isPrefixOf(nameToCheck)) {
      // nameToCheck.isPrefixOf(prefix): nameToCheckがprefixのプレフィックスか？
      //  例: /relay.isPrefixOf(/relay) → true
      //  例: /relay.isPrefixOf(/relay/sub1) → false
      //
      // prefix.isPrefixOf(nameToCheck): prefixがnameToCheckのプレフィックスか？
      //  例: /relay.isPrefixOf(/relay) → true
      //  例: /relay.isPrefixOf(/relay/sub1) → true
      
      isServiceFunction = true;
      break;
    }
  }
  
  if (!isServiceFunction) {
    NLSR_LOG_DEBUG("adjustNexthopCosts: " << nameToCheck << " is not a service function prefix, returning original NextHopList");
    return nhlist;  // FunctionCostを計算せず、元のコストをそのまま返す
  }
  
  // サービスファンクション関連のNextHopのみ、FunctionCostを計算
  // ...
}
```

### マッチング例

#### 例1: `/relay`が処理対象の場合

```
serviceFunctionPrefixes = {/relay}
nameToCheck = /relay

判定:
  /relay.isPrefixOf(/relay) → true
  → isServiceFunction = true
  → FunctionCostを計算 ✓
```

#### 例2: `/relay/sub1`が処理対象の場合

```
serviceFunctionPrefixes = {/relay}
nameToCheck = /relay/sub1

判定:
  /relay/sub1.isPrefixOf(/relay) → false
  /relay.isPrefixOf(/relay/sub1) → true
  → isServiceFunction = true
  → FunctionCostを計算 ✓
```

#### 例3: `/sample2.txt`が処理対象の場合

```
serviceFunctionPrefixes = {/relay}
nameToCheck = /sample2.txt

判定:
  /sample2.txt.isPrefixOf(/relay) → false
  /relay.isPrefixOf(/sample2.txt) → false
  → isServiceFunction = false
  → FunctionCostを計算しない（元のコストをそのまま返す）✓
```

---

## 具体的なシナリオ

### シナリオ1: node1が/relayをアドバタイズしたとき

1. **node1**: `/relay`をアドバタイズ（NameLSAに`/relay`を含める）
2. **node3**: NameLSAを受信
3. **node3**: `updateFromLsdb`が呼ばれる
4. **node3**: `addEntry("/relay", "/ndn/jp/%C1.Router/node1")`が呼ばれる
5. **node3**: `adjustNexthopCosts(nhlist, "/relay", npte)`が呼ばれる
6. **node3**: プレフィックスマッチング
   - `serviceFunctionPrefixes = {/relay}`
   - `nameToCheck = /relay`
   - `/relay.isPrefixOf(/relay)` → `true`
   - `isServiceFunction = true`
7. **node3**: FunctionCostを計算
8. **node3**: FIBを更新

**結果:** `/relay`がサービスファンクションとして認識され、FunctionCostが計算される ✓

### シナリオ2: node1のService Function情報が更新されたとき

1. **node1**: サイドカーログが更新され、ServiceFunctionInfoが計算される
2. **node1**: NameLSAを更新（ServiceFunctionInfoを含める）
3. **node3**: 更新されたNameLSAを受信
4. **node3**: `updateFromLsdb`が呼ばれる
5. **node3**: Service Function情報が存在することを検出
6. **node3**: `/relay`のエントリを更新
7. **node3**: `adjustNexthopCosts(nhlist, "/relay", npte)`が呼ばれる
8. **node3**: プレフィックスマッチング
   - `serviceFunctionPrefixes = {/relay}`
   - `nameToCheck = /relay`
   - `/relay.isPrefixOf(/relay)` → `true`
   - `isServiceFunction = true`
9. **node3**: FunctionCostを再計算（新しいServiceFunctionInfoを使用）
10. **node3**: FIBを更新

**結果:** `/relay`がサービスファンクションとして認識され、FunctionCostが再計算される ✓

### シナリオ3: node2が/relayをアドバタイズしたとき（既にnode1がアドバタイズしている）

1. **node2**: `/relay`をアドバタイズ（NameLSAに`/relay`を含める）
2. **node3**: NameLSAを受信
3. **node3**: `updateFromLsdb`が呼ばれる
4. **node3**: `/relay`のエントリが既に存在するため、`addEntry("/relay", "/ndn/jp/%C1.Router/node2")`が呼ばれる
5. **node3**: 既存のエントリにnode2のルーティング情報を追加
6. **node3**: `adjustNexthopCosts(nhlist, "/relay", npte)`が呼ばれる
7. **node3**: プレフィックスマッチング
   - `serviceFunctionPrefixes = {/relay}`
   - `nameToCheck = /relay`
   - `/relay.isPrefixOf(/relay)` → `true`
   - `isServiceFunction = true`
8. **node3**: FunctionCostを計算（node1経由とnode2経由の両方に対して）
9. **node3**: FIBを更新

**結果:** `/relay`がサービスファンクションとして認識され、FunctionCostが計算される ✓

### シナリオ4: node2が/sample2.txtをアドバタイズしたとき

1. **node2**: `/sample2.txt`をアドバタイズ（NameLSAに`/sample2.txt`を含める）
2. **node3**: NameLSAを受信
3. **node3**: `updateFromLsdb`が呼ばれる
4. **node3**: `addEntry("/sample2.txt", "/ndn/jp/%C1.Router/node2")`が呼ばれる
5. **node3**: `adjustNexthopCosts(nhlist, "/sample2.txt", npte)`が呼ばれる
6. **node3**: プレフィックスマッチング
   - `serviceFunctionPrefixes = {/relay}`
   - `nameToCheck = /sample2.txt`
   - `/sample2.txt.isPrefixOf(/relay)` → `false`
   - `/relay.isPrefixOf(/sample2.txt)` → `false`
   - `isServiceFunction = false`
7. **node3**: FunctionCostを計算しない（元のコストをそのまま返す）
8. **node3**: FIBを更新（FunctionCostなし）

**結果:** `/sample2.txt`はサービスファンクションとして認識されず、FunctionCostは計算されない ✓

---

## まとめ

### `/relay`がサービスファンクションとして認識される条件

1. **設定条件（起動時）:**
   - node3の設定ファイルで`function-prefix`が未設定の場合、デフォルトで`/relay`が追加される
   - `m_confParam.getServiceFunctionPrefixes()`に`/relay`が含まれる

2. **実行条件（adjustNexthopCosts呼び出し時）:**
   - `adjustNexthopCosts`が呼ばれる（NameLSA更新時、新しいプレフィックス追加時、既存プレフィックスに新しいルーター追加時）
   - `nameToCheck`（処理対象のプレフィックス）が`/relay`または`/relay`のサブプレフィックス
   - プレフィックスマッチングで`nameToCheck.isPrefixOf(prefix) || prefix.isPrefixOf(nameToCheck)`が`true`

### FunctionCostが計算される条件

上記の2つの条件を満たした場合、さらに以下が満たされるとFunctionCostが計算されます：

1. **ServiceFunctionInfoが存在する:**
   - `destRouterName`（node1またはnode2）のNameLSAにServiceFunctionInfoが含まれている

2. **ServiceFunctionInfoが有効:**
   - `processingTime > 0.0`または`load > 0.0`または`usageCount > 0`
   - `lastUpdateTime`が古すぎない（`utilization-window-seconds * 2`以内）

### 重要なポイント

- **node3は設定ファイルに`function-prefix`がなくても、デフォルトで`/relay`を認識する**
- **`adjustNexthopCosts`は、すべてのプレフィックスに対して呼ばれるが、サービスファンクションプレフィックスと一致する場合のみFunctionCostを計算する**
- **`/relay`のサブプレフィックス（例: `/relay/sub1`）もサービスファンクションとして認識される**
- **`/sample2.txt`のような通常のプレフィックスは、サービスファンクションとして認識されない**

