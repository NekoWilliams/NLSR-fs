# 追加確認項目と重要性

## 確認日時
2025-11-16

---

## 確認項目の分類

### 🔴 **重要度: 高** - 機能の動作に直接影響

#### 1. `onLsdbModified`シグナルの発火確認

**確認内容**:
- `lsdb.cpp`の`installLsa()`で`onLsdbModified`が呼ばれているか
- NameLSA更新時に`LsdbUpdate::UPDATED`が発火しているか

**確認方法**:
- デバッグログを追加したバージョンで再ビルド・再デプロイ
- NameLSA更新後に`onLsdbModified called`のログを確認

**重要性**: ⭐⭐⭐⭐⭐ (最重要)
- `onLsdbModified`が発火しないと、ルーティング計算がスケジュールされない
- これが機能の根本的な問題である可能性が高い

**期待される結果**:
```
DEBUG: [nlsr.route.RoutingTable] onLsdbModified called: type=2, updateType=1, origin=/ndn/jp/%C1.Router/node1
DEBUG: [nlsr.route.RoutingTable] Schedule calculation set to true for LSA type: 2
DEBUG: [nlsr.route.RoutingTable] Calling scheduleRoutingTableCalculation() for LSA type: 2
```

---

#### 2. `NameLsa::update()`の動作確認

**確認内容**:
- `NameLsa::update()`がService Function情報の変更を検出しているか
- `update()`が`true`を返しているか（`false`の場合、`onLsdbModified`が呼ばれない）

**確認方法**:
- `name-lsa.cpp`の`update()`関数にデバッグログを追加
- Service Function情報の変更時に`update()`が`true`を返すか確認

**重要性**: ⭐⭐⭐⭐⭐ (最重要)
- `update()`が`false`を返すと、`lsdb.cpp`の294行目で`onLsdbModified`が呼ばれない
- Service Function情報の変更が検出されていない可能性がある

**期待される結果**:
```
DEBUG: [nlsr.lsa.NameLsa] update() called, checking Service Function info changes
DEBUG: [nlsr.lsa.NameLsa] Service Function info changed, returning true
```

**確認すべきコード箇所**:
- `src/lsa/name-lsa.cpp`の`update()`関数（約200-280行目付近）

---

#### 3. ルーティング計算のスケジュール確認

**確認内容**:
- `scheduleRoutingTableCalculation()`が呼ばれているか
- ルーティング計算が実際にスケジュールされているか

**確認方法**:
- デバッグログを追加したバージョンで確認
- `Scheduling routing table calculation`のログを確認

**重要性**: ⭐⭐⭐⭐ (高)
- スケジュールされていても、実行されない可能性がある
- スケジュール間隔（通常15秒）を確認

**期待される結果**:
```
DEBUG: [nlsr.route.RoutingTable] scheduleRoutingTableCalculation() called, m_isRouteCalculationScheduled=false
DEBUG: [nlsr.route.RoutingTable] Scheduling routing table calculation in 15 seconds
```

---

### 🟡 **重要度: 中** - 機能の完全性に影響

#### 4. Service Function情報のTLVエンコード/デコード確認

**確認内容**:
- Service Function情報が正しくTLVエンコードされているか
- 他ノードで正しくデコードされているか

**確認方法**:
- node1のNameLSAをTLVダンプ
- node3で受信したNameLSAのTLVダンプ
- Service Function情報が含まれているか確認

**重要性**: ⭐⭐⭐ (中)
- エンコード/デコードが正しくないと、Service Function情報が失われる
- ただし、現在はNameLSA更新自体は動作している

**確認すべきコード箇所**:
- `src/lsa/name-lsa.cpp`の`wireEncode()`と`wireDecode()`

---

#### 5. `calculateCombinedCost()`の呼び出し確認

**確認内容**:
- ルーティング計算時に`calculateCombinedCost()`が呼ばれているか
- Service Function情報が取得できているか

**確認方法**:
- `routing-calculator-link-state.cpp`にデバッグログを追加
- ルーティング計算時にService Function情報が使用されているか確認

**重要性**: ⭐⭐⭐ (中)
- ルーティング計算が実行されても、Service Function情報が使用されない可能性がある
- ただし、まずルーティング計算が実行される必要がある

**期待される結果**:
```
DEBUG: [nlsr.route.RoutingCalculatorLinkState] Getting Service Function info for /relay
DEBUG: [nlsr.route.RoutingCalculatorLinkState] Service Function info: processingTime=0.009269, load=0, usageCount=0
DEBUG: [nlsr.route.RoutingCalculatorLinkState] calculateCombinedCost called: linkCost=50, functionCost=0.003708
```

---

#### 6. ルーティングテーブルのコスト値の変化確認

**確認内容**:
- Service Function情報の変更がルーティングテーブルのコスト値に反映されているか
- コスト値が実際に変化しているか

**確認方法**:
- NameLSA更新前後のルーティングテーブルを比較
- コスト値の変化を確認

**重要性**: ⭐⭐⭐ (中)
- 最終的な動作確認として重要
- ただし、まずルーティング計算が実行される必要がある

---

### 🟢 **重要度: 低** - 最適化・改善に影響

#### 7. 複数回のNameLSA更新での動作確認

**確認内容**:
- 複数回のNameLSA更新でルーティング計算が正しく再実行されるか
- 連続した更新での動作確認

**重要性**: ⭐⭐ (低)
- 基本的な動作が確認できてから実施
- エッジケースの確認

---

#### 8. 他ノードのNameLSA更新時の動作確認

**確認内容**:
- node2のNameLSA更新時にもルーティング計算が再実行されるか
- 複数ノードの更新での動作確認

**重要性**: ⭐⭐ (低)
- 基本的な動作が確認できてから実施
- マルチノード環境での動作確認

---

## 推奨される確認順序

### フェーズ1: 根本原因の特定（最重要）

1. **`onLsdbModified`シグナルの発火確認** ⭐⭐⭐⭐⭐
   - デバッグログ追加版で再ビルド・再デプロイ
   - NameLSA更新後にログを確認

2. **`NameLsa::update()`の動作確認** ⭐⭐⭐⭐⭐
   - `update()`関数にデバッグログを追加
   - Service Function情報の変更検出を確認

### フェーズ2: 動作確認（高）

3. **ルーティング計算のスケジュール確認** ⭐⭐⭐⭐
   - `scheduleRoutingTableCalculation()`の呼び出しを確認

4. **`calculateCombinedCost()`の呼び出し確認** ⭐⭐⭐
   - ルーティング計算時にService Function情報が使用されているか確認

### フェーズ3: 最終確認（中）

5. **ルーティングテーブルのコスト値の変化確認** ⭐⭐⭐
   - 実際のコスト値の変化を確認

6. **Service Function情報のTLVエンコード/デコード確認** ⭐⭐⭐
   - ネットワーク経由での情報伝達を確認

---

## 現在の状況分析

### 確認済み ✅

1. NameLSA更新の検出（node3でnode1のNameLSA更新を検出）
2. Service Function情報の設定（node1で設定されている）
3. NameLSAの同期（node3でnode1のNameLSAを受信）

### 未確認 ❌

1. `onLsdbModified`シグナルの発火（ログに表示されていない）
2. `NameLsa::update()`の動作（Service Function情報の変更検出）
3. ルーティング計算の再実行（NameLSA更新後に実行されていない）

### 推測される問題

**最も可能性が高い問題**:
- `NameLsa::update()`がService Function情報の変更を検出していない
- 結果として、`update()`が`false`を返し、`onLsdbModified`が呼ばれない
- ルーティング計算がスケジュールされない

**確認すべきコード**:
```cpp
// src/lsa/name-lsa.cpp の update() 関数
// Service Function情報の変更を検出しているか確認
```

---

## 次のアクション

### 即座に実施すべき確認

1. **`NameLsa::update()`の実装を確認**
   - Service Function情報の変更を検出しているか
   - `m_serviceFunctionInfo`の比較を行っているか

2. **デバッグログを追加**
   - `name-lsa.cpp`の`update()`関数にログを追加
   - `lsdb.cpp`の`installLsa()`にログを追加（既に追加済み）

3. **再ビルド・再デプロイ**
   - デバッグログ追加版で再ビルド
   - 再デプロイして検証

---

## 確認方法の詳細

### 確認1: `onLsdbModified`シグナルの発火

**手順**:
1. デバッグログ追加版で再ビルド・再デプロイ
2. node1のログファイルに新しいエントリを追加
3. node3のログで以下を確認:
   ```
   DEBUG: [nlsr.route.RoutingTable] onLsdbModified called: type=2, updateType=1, origin=/ndn/jp/%C1.Router/node1
   ```

**成功条件**:
- `onLsdbModified called`のログが表示される
- `type=2`（NAMEタイプ）である
- `updateType=1`（UPDATED）である

---

### 確認2: `NameLsa::update()`の動作

**手順**:
1. `name-lsa.cpp`の`update()`関数にデバッグログを追加
2. 再ビルド・再デプロイ
3. node1のログファイルに新しいエントリを追加
4. node3のログで以下を確認:
   ```
   DEBUG: [nlsr.lsa.NameLsa] update() called
   DEBUG: [nlsr.lsa.NameLsa] Service Function info changed, returning true
   ```

**成功条件**:
- `update()`が呼ばれる
- Service Function情報の変更が検出される
- `true`が返される

---

## まとめ

### 最重要確認項目

1. **`onLsdbModified`シグナルの発火確認** ⭐⭐⭐⭐⭐
2. **`NameLsa::update()`の動作確認** ⭐⭐⭐⭐⭐

この2つが確認できれば、根本原因が特定できます。

### 確認の優先順位

1. **フェーズ1**: 根本原因の特定（`onLsdbModified`と`update()`）
2. **フェーズ2**: 動作確認（ルーティング計算のスケジュール）
3. **フェーズ3**: 最終確認（コスト値の変化）

現在は**フェーズ1**の確認が最優先です。

