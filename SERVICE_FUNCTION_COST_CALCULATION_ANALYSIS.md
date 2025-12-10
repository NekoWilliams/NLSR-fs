# Service Functionコスト計算の構造分析

## ① なぜファンクションコスト計算側が.confファイルに/relayプレフィックスの記載を必要とするのか

### 現在の実装の問題点

現在の実装では、`adjustNexthopCosts`関数内で以下のロジックが使用されています：

```cpp
bool isServiceFunction = m_confParam.isServiceFunctionPrefix(nameToCheck);
if (isServiceFunction) {
  // NameLSAからService Function情報を取得
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    // FunctionCostを計算
  }
}
```

**問題点：**
1. `isServiceFunctionPrefix(nameToCheck)`は、設定ファイルから読み込んだ`function-prefix`のリストと比較している
2. コスト計算側（node3, node4）の設定ファイルに`function-prefix`が記載されていない場合、`isServiceFunctionPrefix()`は`false`を返す
3. その結果、NameLSAにService Function情報が存在していても、FunctionCostが計算されない

**なぜこの実装になっているか：**
- 設定ファイルから読み込んだ`function-prefix`のリストを「信頼できるソース」として使用している
- しかし、これは設計上の誤りで、実際にはNameLSAに含まれるService Function情報が「信頼できるソース」であるべき

## ② 解決策：どのような構造に変更する必要があるか

### 現在の構造

```
┌─────────────────────────────────────────────────────────┐
│ コスト計算側（node3, node4）                            │
│                                                          │
│  adjustNexthopCosts()                                    │
│    ↓                                                     │
│  isServiceFunctionPrefix(nameToCheck)                   │
│    ↓                                                     │
│  [設定ファイルから読み込んだfunction-prefixリスト]      │
│    ↓                                                     │
│  NameLSAからService Function情報を取得                  │
│    ↓                                                     │
│  FunctionCostを計算                                      │
└─────────────────────────────────────────────────────────┘
```

**問題：** コスト計算側の設定ファイルに`function-prefix`が必要

### 変更後の構造

```
┌─────────────────────────────────────────────────────────┐
│ コスト計算側（node3, node4）                            │
│                                                          │
│  adjustNexthopCosts()                                    │
│    ↓                                                     │
│  NameLSAからService Function情報を取得                   │
│    ↓                                                     │
│  Service Function情報が存在するか？                     │
│    ↓                                                     │
│  [NameLSAに含まれる情報を信頼できるソースとして使用]    │
│    ↓                                                     │
│  FunctionCostを計算                                      │
└─────────────────────────────────────────────────────────┘
```

**改善点：** コスト計算側の設定ファイルに`function-prefix`は不要

### 具体的な変更内容

#### 1. `adjustNexthopCosts`関数の変更

**変更前：**
```cpp
bool isServiceFunction = m_confParam.isServiceFunctionPrefix(nameToCheck);
if (isServiceFunction) {
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    // FunctionCostを計算
  }
}
```

**変更後：**
```cpp
// NameLSAから直接Service Function情報を取得
auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
if (nameLsa) {
  ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
  
  // Service Function情報が存在するかチェック（NameLSAが信頼できるソース）
  if (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0) {
    // FunctionCostを計算
  }
}
```

#### 2. 重み付けパラメータの取得

重み付けパラメータ（`processingWeight`, `loadWeight`, `usageWeight`）については、以下の2つの選択肢があります：

**選択肢A：コスト計算側の設定ファイルから取得（現在の実装）**
- メリット：各ノードが独自の重み付けを設定できる
- デメリット：コスト計算側にも設定が必要

**選択肢B：NameLSAに含める**
- メリット：Service Function提供側が重み付けを指定できる
- デメリット：NameLSAの構造変更が必要

**推奨：選択肢A（現在の実装を維持）**
- コスト計算側が重み付けを制御できる方が柔軟性が高い
- ただし、`function-prefix`のチェックは不要

### 実装の変更箇所

1. **`src/route/name-prefix-table.cpp`**
   - `adjustNexthopCosts`関数から`isServiceFunctionPrefix()`のチェックを削除
   - NameLSAから直接Service Function情報を取得し、それが有効かどうかをチェック

2. **`src/conf-parameter.hpp`**
   - `isServiceFunctionPrefix()`メソッドは残す（Service Function提供側で使用される可能性があるため）
   - ただし、コスト計算側では使用しない

3. **設定ファイル**
   - node1, node2: `service-function`セクションに`function-prefix`と重み付けパラメータを記載（変更なし）
   - node3, node4: `service-function`セクションは不要（削除可能）
   - ただし、重み付けパラメータを使用する場合は、`service-function`セクションに重み付けのみを記載

### 変更のメリット

1. **設計の一貫性**
   - Service Function情報はNameLSAに含まれるため、それを信頼できるソースとして使用する
   - 設定ファイルは各ノードが提供するService Functionの情報を記載するためのもの

2. **設定の簡素化**
   - コスト計算側の設定ファイルに`function-prefix`を記載する必要がなくなる
   - ネットワーク全体で、Service Function提供側のみが設定を記載すればよい

3. **拡張性の向上**
   - 新しいService Functionプレフィックスを追加する際、コスト計算側の設定を変更する必要がない
   - NameLSAに含まれる情報を自動的に認識する

