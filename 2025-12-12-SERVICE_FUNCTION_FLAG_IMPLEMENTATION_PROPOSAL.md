2025-12-12

# サービスファンクションフラグ実装案

## 概要

`/relay`のハードコードを削除し、プレフィックスにフラグを付けてサービスファンクションを識別する実装案を提案します。

## 問題点

1. **ハードコードされた`/relay`**: `conf-file-processor.cpp`でデフォルト値として`/relay`がハードコードされている
2. **環境依存**: `/relay`は環境限定のプレフィックスであり、NLSR-fsのソースコードに含まれるべきではない
3. **設定の複雑さ**: node3が`/relay`を認識するために、設定ファイルで`function-prefix`を指定する必要がある

## 実装案

### 案1: PrefixInfoにサービスファンクションフラグを追加（推奨）

#### 概要

`PrefixInfo`に`isServiceFunction`フラグを追加し、NameLSAでこのフラグをエンコード/デコードします。これにより、node1, node2が`/relay`をアドバタイズする際に、このフラグを`true`に設定し、node3がNameLSAを受信した際に、このフラグを確認してサービスファンクションかどうかを判断します。

#### メリット

- **環境非依存**: プレフィックス名に依存せず、フラグで判断するため、任意のプレフィックスをサービスファンクションとして扱える
- **設定の簡略化**: node3は設定ファイルで`function-prefix`を指定する必要がない
- **明確な意図**: フラグにより、サービスファンクションかどうかが明確になる

#### デメリット

- **TLVの拡張**: 新しいTLVタイプを追加する必要がある（後方互換性の考慮が必要）
- **実装の複雑さ**: `PrefixInfo`のエンコード/デコード処理を変更する必要がある

#### 実装詳細

##### 1. PrefixInfoにフラグを追加

**ファイル:** `name-prefix-list.hpp`

```cpp
class PrefixInfo : private boost::equality_comparable<PrefixInfo>
{
public:
  // ... 既存のメソッド ...

  bool isServiceFunction() const
  {
    return m_isServiceFunction;
  }

  void setIsServiceFunction(bool isServiceFunction)
  {
    m_wire.reset();
    m_isServiceFunction = isServiceFunction;
  }

private:
  ndn::Name m_prefixName;
  double m_prefixCost;
  bool m_isServiceFunction = false;  // デフォルトはfalse（後方互換性のため）

  mutable ndn::Block m_wire;
};
```

##### 2. TLVタイプの追加

**ファイル:** `tlv-nlsr.hpp`

```cpp
enum {
  // ... 既存のTLVタイプ ...
  PrefixInfo = 128,
  IsServiceFunction = 129,  // 新しいTLVタイプ
  // ...
};
```

##### 3. PrefixInfoのエンコード/デコード処理を変更

**ファイル:** `name-prefix-list.cpp`

```cpp
template<ndn::encoding::Tag TAG>
size_t
PrefixInfo::wireEncode(ndn::EncodingImpl<TAG>& encoder) const
{
  size_t totalLength = 0;

  // IsServiceFunctionフラグをエンコード（オプショナル、trueの場合のみ）
  if (m_isServiceFunction) {
    totalLength += prependBooleanBlock(encoder, nlsr::tlv::IsServiceFunction, true);
  }

  // 既存のエンコード処理（cost, name）
  totalLength += prependDoubleBlock(encoder, nlsr::tlv::Cost, m_prefixCost);
  totalLength += prependBlock(encoder, m_prefixName.wireEncode());

  totalLength += encoder.prependVarNumber(totalLength);
  totalLength += encoder.prependVarNumber(nlsr::tlv::PrefixInfo);

  return totalLength;
}

void
PrefixInfo::wireDecode(const ndn::Block& wire)
{
  // ... 既存のデコード処理 ...

  // IsServiceFunctionフラグをデコード（オプショナル）
  m_isServiceFunction = false;  // デフォルトはfalse
  for (auto it = wire.elements_begin(); it != wire.elements_end(); ++it) {
    if (it->type() == nlsr::tlv::IsServiceFunction) {
      m_isServiceFunction = readBoolean(*it);
      break;
    }
  }
}
```

##### 4. アドバタイズ時にフラグを設定

**ファイル:** `lsdb.cpp` (buildAndInstallOwnNameLsa)

```cpp
void
Lsdb::buildAndInstallOwnNameLsa()
{
  NamePrefixList npl;
  for (const auto& name : m_confParam.getNamePrefixList().getNames()) {
    // サービスファンクションプレフィックスかどうかを確認
    bool isServiceFunction = m_confParam.isServiceFunctionPrefix(name);
    
    PrefixInfo prefixInfo(name, 0);
    prefixInfo.setIsServiceFunction(isServiceFunction);
    npl.insert(prefixInfo);
  }

  // ... 既存の処理 ...
}
```

##### 5. adjustNexthopCostsでの判定を変更

**ファイル:** `name-prefix-table.cpp`

```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, const ndn::Name& nameToCheck, const NamePrefixTableEntry& npte)
{
  // サービスファンクションかどうかを判定
  // NameLSAからPrefixInfoを取得して、isServiceFunctionフラグを確認
  bool isServiceFunction = false;
  
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
      if (isServiceFunction) {
        break;
      }
    }
  }
  
  if (!isServiceFunction) {
    NLSR_LOG_DEBUG("adjustNexthopCosts: " << nameToCheck << " is not a service function, returning original NextHopList");
    return nhlist;
  }
  
  // サービスファンクション関連のNextHopのみ、FunctionCostを計算
  // ... 既存の処理 ...
}
```

##### 6. デフォルト値の削除

**ファイル:** `conf-file-processor.cpp`

```cpp
// デフォルト値の追加を削除
// if (!foundFunctionPrefix) {
//   m_confParam.addServiceFunctionPrefix(ndn::Name("/relay"));
//   std::cerr << "No function-prefix specified, using default: /relay" << std::endl;
// }
```

---

### 案2: 設定ファイルでfunction-prefixを必須にする（シンプルな案）

#### 概要

`conf-file-processor.cpp`からデフォルトの`/relay`を削除し、すべてのノード（node1, node2, node3）で`function-prefix`を明示的に指定する必要があります。

#### メリット

- **シンプル**: 実装が簡単
- **明確**: 設定ファイルで明示的に指定するため、意図が明確

#### デメリット

- **設定の複雑さ**: すべてのノードで`function-prefix`を指定する必要がある
- **環境依存**: 設定ファイルに環境依存の情報が含まれる

#### 実装詳細

**ファイル:** `conf-file-processor.cpp`

```cpp
// デフォルト値の追加を削除
// if (!foundFunctionPrefix) {
//   m_confParam.addServiceFunctionPrefix(ndn::Name("/relay"));
//   std::cerr << "No function-prefix specified, using default: /relay" << std::endl;
// }

// オプション: function-prefixが指定されていない場合にエラーを出す
if (!foundFunctionPrefix) {
  std::cerr << "Warning: No function-prefix specified in service-function section. "
            << "Service function routing will be disabled." << std::endl;
}
```

---

### 案3: 案1と案2の組み合わせ（推奨）

#### 概要

案1（PrefixInfoにフラグを追加）と案2（デフォルト値を削除）を組み合わせます。

#### メリット

- **環境非依存**: フラグにより、プレフィックス名に依存しない
- **設定の柔軟性**: 設定ファイルで`function-prefix`を指定するか、フラグを使用するかを選択できる
- **後方互換性**: フラグが存在しない場合は`false`として扱う（後方互換性）

#### 実装詳細

案1と案2の実装を組み合わせます。

---

## 推奨実装: 案1（PrefixInfoにフラグを追加）

### 理由

1. **環境非依存**: プレフィックス名に依存せず、フラグで判断するため、任意のプレフィックスをサービスファンクションとして扱える
2. **設定の簡略化**: node3は設定ファイルで`function-prefix`を指定する必要がない
3. **明確な意図**: フラグにより、サービスファンクションかどうかが明確になる
4. **拡張性**: 将来的に他のフラグを追加する場合にも対応しやすい

### 実装手順

1. **PrefixInfoにフラグを追加** (`name-prefix-list.hpp`)
2. **TLVタイプを追加** (`tlv-nlsr.hpp`)
3. **エンコード/デコード処理を変更** (`name-prefix-list.cpp`)
4. **アドバタイズ時にフラグを設定** (`lsdb.cpp`)
5. **adjustNexthopCostsでの判定を変更** (`name-prefix-table.cpp`)
6. **デフォルト値を削除** (`conf-file-processor.cpp`)

### 後方互換性

- フラグが存在しない場合は`false`として扱う（既存のNameLSAとの互換性）
- フラグが`true`の場合のみエンコードする（オプショナルTLV）

---

## まとめ

### 推奨: 案1（PrefixInfoにフラグを追加）

- **環境非依存**: プレフィックス名に依存しない
- **設定の簡略化**: node3は設定ファイルで`function-prefix`を指定する必要がない
- **明確な意図**: フラグにより、サービスファンクションかどうかが明確になる
- **拡張性**: 将来的に他のフラグを追加する場合にも対応しやすい

### 実装の影響範囲

- `name-prefix-list.hpp`: PrefixInfoにフラグを追加
- `name-prefix-list.cpp`: エンコード/デコード処理を変更
- `tlv-nlsr.hpp`: 新しいTLVタイプを追加
- `lsdb.cpp`: アドバタイズ時にフラグを設定
- `name-prefix-table.cpp`: adjustNexthopCostsでの判定を変更
- `conf-file-processor.cpp`: デフォルト値を削除

