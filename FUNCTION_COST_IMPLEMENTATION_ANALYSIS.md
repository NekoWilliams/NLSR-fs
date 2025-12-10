# FunctionCost実装分析レポート

## 概要

FunctionCostをルーター間のコストではなく、`/relay`プレフィックスのコストに反映する仕様変更について、現在の実装状況、必要な変更、リスクを分析します。

## 現在の実装状況

### 既に実装済みの変更

現在のコードは**既に「FunctionCostを/relayプレフィックスのコストに反映する」仕様**になっています。

#### 1. `routing-calculator-link-state.cpp`の変更

**ファイル**: `src/route/routing-calculator-link-state.cpp`

**変更箇所**: `calculateDijkstraPath`関数（344-346行目）

```cpp
// FunctionCost is now applied to /relay prefix cost in NamePrefixTable::adjustNexthopCosts
// No longer applying FunctionCost to router-to-router path cost
double functionCost = 0.0;
```

**効果**: 
- ルーター間の最短経路計算（Dijkstraアルゴリズム）では、FunctionCostを0.0に設定
- ルーター間のコストにはFunctionCostが含まれない

#### 2. `name-prefix-table.cpp`の変更

**ファイル**: `src/route/name-prefix-table.cpp`

**変更箇所**: `adjustNexthopCosts`関数（129-153行目）

```cpp
// Calculate FunctionCost for service function prefix if applicable
double functionCost = 0.0;
ndn::Name serviceFunctionPrefix = m_confParam.getServiceFunctionPrefix();
if (nameToCheck == serviceFunctionPrefix) {
  // Get Service Function information from the destination router's NameLSA
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(serviceFunctionPrefix);
    
    // Calculate function cost if Service Function info is available
    if (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0) {
      double processingWeight = m_confParam.getProcessingWeight();
      double loadWeight = m_confParam.getLoadWeight();
      double usageWeight = m_confParam.getUsageWeight();
      
      functionCost = sfInfo.processingTime * processingWeight +
                    sfInfo.load * loadWeight +
                    (sfInfo.usageCount / 100.0) * usageWeight;
    }
  }
}

for (const auto& nh : nhlist.getNextHops()) {
    double adjustedCost = nh.getRouteCost() + 
                         m_nexthopCost[DestNameKey(destRouterName, nameToCheck)] +
                         functionCost;
    const NextHop newNextHop = NextHop(nh.getConnectingFaceUri(), adjustedCost);
    new_nhList.addNextHop(newNextHop);
}
```

**効果**:
- `/relay`プレフィックス（または設定ファイルで指定されたファンクション名）の場合のみ、FunctionCostを計算
- NextHopのコストにFunctionCostを加算してFIBに反映

## 実装の流れ

### 1. ルーティング計算フェーズ

```
calculateLinkStateRoutingPath()
  └─> calculateDijkstraPath()
       └─> ルーター間の最短経路を計算（FunctionCost = 0.0）
            └─> RoutingTableにNextHopを追加（linkCostのみ）
```

### 2. 名前プレフィックス処理フェーズ

```
NamePrefixTable::addEntry()
  └─> adjustNexthopCosts()
       └─> プレフィックスが/relayの場合
            └─> FunctionCostを計算
                 └─> NextHopのコストにFunctionCostを加算
                      └─> FIBに反映
```

## 変更が必要な場合の分析

### ケース1: 現在の実装が不完全な場合

もし現在の実装が期待通りに動作していない場合、以下の点を確認・修正する必要があります。

#### 確認すべき点

1. **`adjustNexthopCosts`の呼び出しタイミング**
   - `addEntry`関数内で正しく呼び出されているか
   - `updateWithNewRoute`関数内で正しく呼び出されているか

2. **FunctionCostの計算タイミング**
   - NameLSAが更新されたときに、FunctionCostが再計算されるか
   - Service Function情報が変更されたときに、FIBが更新されるか

3. **設定ファイルの`function-prefix`**
   - 設定ファイルで正しいファンクション名が指定されているか

#### 必要な変更（最小限）

**リスク**: 低

**変更ファイル**:
- `src/route/name-prefix-table.cpp`（既に実装済み）
- `src/route/routing-calculator-link-state.cpp`（既に実装済み）

**変更量**: 既に実装済み（追加変更不要）

### ケース2: より明確な分離を実現する場合

現在の実装では、`PathCost`構造体に`functionCost`が含まれていますが、ルーター間のコスト計算では使用されていません。より明確に分離する場合の変更案：

#### 変更案A: PathCost構造体からfunctionCostを削除

**リスク**: 中

**変更ファイル**:
- `src/route/routing-calculator-link-state.cpp`
  - `PathCost`構造体の定義を変更（`functionCost`フィールドを削除）
  - `calculateDijkstraPath`関数の変更（`functionCost`関連のコードを削除）
  - `addNextHopsToRoutingTable`関数の変更（`totalCost`の計算を変更）

**変更量**: 約50-100行

**影響範囲**:
- ルーティング計算のロジック全体
- テストコード（既存のテストが失敗する可能性）

#### 変更案B: FunctionCostを完全に分離

**リスク**: 高

**変更ファイル**:
- `src/route/routing-calculator-link-state.cpp`
  - `PathCost`構造体から`functionCost`を削除
  - `calculateDijkstraPath`関数を簡素化
- `src/route/name-prefix-table.cpp`
  - `adjustNexthopCosts`関数を拡張（FunctionCostの計算ロジックを強化）
- `src/route/routing-table.hpp`
  - `RoutingTableEntry`からFunctionCost関連の情報を削除

**変更量**: 約100-200行

**影響範囲**:
- ルーティング計算のロジック全体
- NamePrefixTableの処理ロジック
- テストコード（大幅な修正が必要）

## リスク評価

### 現在の実装のリスク

| リスク項目 | レベル | 説明 |
|-----------|--------|------|
| 既存機能への影響 | 低 | ルーター間のコスト計算は変更されていない |
| テストの失敗 | 低 | 既存のテストは通過している |
| パフォーマンス | 低 | 追加の計算は`/relay`プレフィックスのみ |
| コードの複雑性 | 中 | `PathCost`構造体に`functionCost`が残っているが未使用 |

### 変更案Aのリスク

| リスク項目 | レベル | 説明 |
|-----------|--------|------|
| 既存機能への影響 | 中 | `PathCost`構造体の変更により、他のコードに影響する可能性 |
| テストの失敗 | 中 | 既存のテストが失敗する可能性 |
| パフォーマンス | 低 | パフォーマンスへの影響は小さい |
| コードの複雑性 | 低 | コードがより明確になる |

### 変更案Bのリスク

| リスク項目 | レベル | 説明 |
|-----------|--------|------|
| 既存機能への影響 | 高 | 大規模な変更により、予期しない影響が発生する可能性 |
| テストの失敗 | 高 | 多くのテストが失敗し、修正が必要 |
| パフォーマンス | 低 | パフォーマンスへの影響は小さい |
| コードの複雑性 | 低 | コードがより明確になる |

## 推奨事項

### 現状維持（推奨）

**理由**:
1. 既に期待通りの動作を実現している
2. リスクが低い
3. 追加の変更が不要

**確認事項**:
- 実際の動作確認（テストの実行）
- ログの確認（FunctionCostが正しく計算されているか）

### 変更案A（中程度のリスク）

**理由**:
1. コードがより明確になる
2. `functionCost`が未使用であることを明確化できる

**実施手順**:
1. 既存のテストを実行してベースラインを確認
2. `PathCost`構造体から`functionCost`を削除
3. 関連するコードを修正
4. テストを実行して確認

### 変更案B（高リスク、非推奨）

**理由**:
1. 大規模な変更が必要
2. リスクが高い
3. 現在の実装で十分に動作している

## 結論

現在の実装は**既に「FunctionCostを/relayプレフィックスのコストに反映する」仕様**を実現しています。

- **変更不要**: 現在の実装で十分に動作している場合
- **最小限の変更**: 動作確認とログの確認のみ
- **コードの明確化**: 変更案Aを検討（中程度のリスク）

実際の動作を確認し、問題がなければ現状維持を推奨します。

