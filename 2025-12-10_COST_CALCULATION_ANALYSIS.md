# コスト計算方式の分析レポート

## 質問1: FunctionCostはそれぞれ別々になっているか？

### 回答: **はい、それぞれ別々に計算されています**

### コード確認結果

`adjustNexthopCosts`関数（`src/route/name-prefix-table.cpp:125-189`）の動作：

```cpp
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, 
                                    const ndn::Name& nameToCheck, 
                                    const ndn::Name& destRouterName)
{
  // destRouterName（node1またはnode2）ごとにNameLSAを取得
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    // そのNameLSAからService Function情報を取得
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    
    // FunctionCostを計算
    functionCost = sfInfo.processingTime * processingWeight +
                   sfInfo.load * loadWeight +
                   (sfInfo.usageCount / 100.0) * usageWeight;
  }
  // ...
}
```

### 動作の流れ

1. **node1へのパスの場合:**
   - `adjustNexthopCosts(..., /relay, /ndn/jp/%C1.Router/node1)`が呼ばれる
   - node1のNameLSAからService Function情報を取得
   - node1の`processingTime`, `load`, `usageCount`を使用してFunctionCostを計算

2. **node2へのパスの場合:**
   - `adjustNexthopCosts(..., /relay, /ndn/jp/%C1.Router/node2)`が呼ばれる
   - node2のNameLSAからService Function情報を取得
   - node2の`processingTime`, `load`, `usageCount`を使用してFunctionCostを計算

### 結論

**それぞれのノード（node1, node2）のService Function情報に基づいて、別々のFunctionCostが計算されます。** これにより、各ノードの負荷に応じたトラフィック分散が可能になります。

---

## 質問2: コンテンツ/relayへのコスト決定はどのように行われているか？

### 回答: **/relayがアドバタイズされる際に、各ノードでコストが計算されます**

### コード確認結果

#### 1. アドバタイズ時の処理

`NamePrefixTable::addEntry`関数（`src/route/name-prefix-table.cpp:191-268`）の動作：

```cpp
void NamePrefixTable::addEntry(const ndn::Name& name, const ndn::Name& destRouter)
{
  // /relayがアドバタイズされると、この関数が呼ばれる
  // ...
  
  // FIBを更新する際に、adjustNexthopCostsを呼ぶ
  if (npte->getNexthopList().size() > 0) {
    m_fib.update(name, adjustNexthopCosts(npte->getNexthopList(), name, destRouter));
  }
}
```

#### 2. コスト計算のタイミング

- **NameLSAの受信時:** `NamePrefixTable::processNameLsa`が呼ばれる
- **エントリ追加時:** `addEntry`が呼ばれ、`adjustNexthopCosts`でコストが計算される
- **FIB更新時:** 計算されたコストがFIBに反映される

### 動作の流れ

1. **node1が/relayをアドバタイズ**
   - NameLSAに`/relay`がPrefixInfoとして含まれる
   - NameLSAが他のノードに伝播

2. **node3がNameLSAを受信**
   - `NamePrefixTable::processNameLsa`が呼ばれる
   - `addEntry(/relay, /ndn/jp/%C1.Router/node1)`が呼ばれる
   - `adjustNexthopCosts`でnode1へのFunctionCostが計算される
   - FIBが更新される

3. **node2が/relayをアドバタイズ**
   - 同様の処理がnode2についても行われる
   - node2へのFunctionCostが計算される

### 結論

**/relayがアドバタイズされる際に、各ノード（node3, node4）でコストが計算されます。** これは、各ノードがNameLSAからService Function情報を取得し、独自にFunctionCostを計算する方式です。

---

## 質問3: コスト計算方式の比較と提案

### 現在の方式（分散計算方式）

**概要:**
- 各ノード（node3, node4）がNameLSAからService Function情報を取得
- 各ノードが独自にFunctionCostを計算
- 計算結果をFIBに反映

**メリット:**
- ✅ 各ノードが独自の重み付けパラメータ（processingWeight, loadWeight, usageWeight）を使用可能
- ✅ 計算の分散により、Service Function提供側（node1, node2）の負荷が少ない
- ✅ 柔軟性が高い（各ノードが異なる重み付けを設定可能）

**デメリット:**
- ❌ 各ノードで同じ計算を繰り返す（計算の重複）
- ❌ Service Function情報が更新された際に、すべてのノードで再計算が必要
- ❌ 現在の実装では、NameLSAの更新タイミングとルーティング計算のタイミングがずれる可能性がある

### 提案方式1: Service Function提供側でコスト計算（集中計算方式）

**概要:**
- node1とnode2が自分自身のFunctionCostを計算
- FunctionCostをPrefixInfoのコストとして含めてアドバタイズ
- 他のノード（node3, node4）は、そのコストをそのまま使用

**実装方法:**
1. node1とnode2で、Service Function情報が更新された際にFunctionCostを計算
2. `PrefixInfo`のコストとしてFunctionCostを含める
3. NameLSAに含めてアドバタイズ

**メリット:**
- ✅ 計算の重複がなくなる
- ✅ Service Function提供側がコストを決定できる
- ✅ コスト更新のタイミングが明確（NameLSAの更新と同時）

**デメリット:**
- ❌ 各ノードが独自の重み付けパラメータを使用できない
- ❌ Service Function提供側の負荷が増加
- ❌ 柔軟性が低い（すべてのノードが同じコストを使用）

### 提案方式2: ハイブリッド方式

**概要:**
- Service Function提供側（node1, node2）がFunctionCostを計算
- FunctionCostをNameLSAのService Function情報に含める（現在の実装に追加）
- 各ノード（node3, node4）は、そのFunctionCostを使用するか、独自に計算するかを選択可能

**実装方法:**
1. `ServiceFunctionInfo`構造体に`functionCost`フィールドを追加
2. node1とnode2で、Service Function情報が更新された際にFunctionCostを計算し、`ServiceFunctionInfo`に含める
3. 各ノードは、`ServiceFunctionInfo.functionCost`を使用するか、独自に計算するかを選択可能

**メリット:**
- ✅ 計算の重複を減らせる（オプション）
- ✅ 柔軟性を維持できる（各ノードが独自の重み付けを使用可能）
- ✅ 後方互換性を維持できる

**デメリット:**
- ❌ 実装が複雑になる
- ❌ `ServiceFunctionInfo`構造体の変更が必要

### 提案方式3: 現在の方式の改善（推奨）

**概要:**
- 現在の分散計算方式を維持
- NameLSAの更新タイミングとルーティング計算のタイミングを同期
- デバッグログを追加して、問題を特定しやすくする

**改善点:**
1. **NameLSA更新時のルーティング計算スケジュール**
   - Service Function情報が更新された際に、ルーティング計算をスケジュール
   - 現在の実装では、NAMEタイプのLSA更新時にルーティング計算がスケジュールされているが、Service Function情報の更新タイミングとずれる可能性がある

2. **デバッグログの追加**
   - `adjustNexthopCosts`で、NameLSAの`m_serviceFunctionInfo`マップの内容を出力
   - `getServiceFunctionInfo`メソッドの動作を確認

3. **キャッシュの活用**
   - 計算結果をキャッシュし、Service Function情報が更新された際にのみ再計算

**メリット:**
- ✅ 現在の実装を大きく変更する必要がない
- ✅ 各ノードが独自の重み付けを使用可能
- ✅ 柔軟性が高い

**デメリット:**
- ❌ 計算の重複は残る
- ❌ 実装の改善が必要

### 推奨事項

**現在の方式（分散計算方式）を維持し、改善することを推奨します。**

**理由:**
1. **柔軟性:** 各ノードが独自の重み付けパラメータを使用できるため、異なるポリシーを適用可能
2. **拡張性:** 将来的に、各ノードが異なる重み付けを使用する必要が生じた場合に対応可能
3. **実装の簡潔性:** 現在の実装を大きく変更する必要がない

**改善すべき点:**
1. NameLSAの更新タイミングとルーティング計算のタイミングを同期
2. デバッグログの追加
3. 計算結果のキャッシュ（オプション）

### 代替案: PrefixInfoのコストとしてFunctionCostを含める方式

もし、すべてのノードが同じ重み付けを使用する場合、**提案方式1（集中計算方式）**も有効です。

**実装方法:**
1. node1とnode2で、Service Function情報が更新された際にFunctionCostを計算
2. `PrefixInfo`のコストとしてFunctionCostを含める
3. `nlsrc advertise /relay`コマンドで、コストを指定してアドバタイズ

**注意点:**
- 現在のNLSRでは、`PrefixInfo`のコストは手動で設定する必要がある
- Service Function情報が更新された際に、自動的にコストを更新する仕組みが必要

---

## まとめ

1. **FunctionCostはそれぞれ別々に計算されている** ✅
   - node1とnode2のService Function情報に基づいて、別々のFunctionCostが計算される

2. **/relayへのコスト決定は、アドバタイズ時に各ノードで行われる** ✅
   - 各ノードがNameLSAからService Function情報を取得し、独自にFunctionCostを計算

3. **現在の方式を維持し、改善することを推奨** ✅
   - 柔軟性と拡張性を維持しつつ、実装を改善する

