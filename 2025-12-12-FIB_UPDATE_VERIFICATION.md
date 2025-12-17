2025-12-12

# FIB更新がされていない理由の検証結果

## 検証概要

FIBエントリのコストが更新されていない問題について、ログを詳細に分析し、原因を特定しました。

## 検証結果

### 1. FunctionCostの計算は正常に動作している

**ログから確認できたこと:**
```
Adjusting cost for /relay to /ndn/jp/%C1.Router/node1: originalCost=50, nexthopCost=0, functionCost=77.1599, adjustedCost=127.16
Adjusting cost for /relay to /ndn/jp/%C1.Router/node1: originalCost=50, nexthopCost=0, functionCost=91.8005, adjustedCost=141.8
Adjusting cost for /relay to /ndn/jp/%C1.Router/node1: originalCost=50, nexthopCost=0, functionCost=93.7148, adjustedCost=143.715
```

**分析:**
- ✓ `adjustNexthopCosts`は正しくFunctionCostを計算している
- ✓ `adjustedCost`は正しく計算されている（127.16, 141.8, 143.715など）

### 2. Fib::updateは呼ばれている

**ログから確認できたこと:**
```
Fib::update called
Existing FIB Entry
Cost changed for tcp4://10.104.74.134:6363: old=25, new=50
Unregister prefix: /relay Face Uri: tcp4://10.104.74.134:6363
Registering prefix: /relay faceUri: tcp4://10.104.74.134:6363 with cost: 50 (as integer: 50000)
```

**分析:**
- ✓ `Fib::update`は呼ばれている
- ✓ コスト変更は検出されている
- ✓ unregister/registerは実行されている

### 3. 問題点：FIBに登録されるコストが古い

**ログから確認できたこと:**
```
Adjusting cost for /relay to /ndn/jp/%C1.Router/node1: adjustedCost=127.16
Registering prefix: /relay faceUri: tcp4://10.104.74.134:6363 with cost: 25 (as integer: 25000)
```

**分析:**
- ✗ `adjustNexthopCosts`で計算されたコスト（127.16）がFIBに反映されていない
- ✗ FIBに登録されるコストは古い値（25, 50, 75）のまま

## 原因の特定

### 問題の根本原因

`adjustNexthopCosts`で計算された`NexthopList`が`Fib::update`に正しく渡されているが、`Fib::update`内で以下の問題が発生している可能性があります：

1. **`hopsToAdd`の生成タイミング**
   - `Fib::update`は`allHops`から`maxFaces`個のNextHopを`hopsToAdd`に追加
   - しかし、`adjustNexthopCosts`が返す`NexthopList`の順序や内容が、`Fib::update`の期待と一致していない可能性

2. **コスト変更検出のロジック**
   - `Fib::update`は既存のNextHopと新しいNextHopを比較してコスト変更を検出
   - しかし、`adjustNexthopCosts`が返す`NexthopList`のNextHopが、既存のNextHopと正しくマッチングされていない可能性

3. **`generateNhlfromRteList()`の呼び出し**
   - `updateFromLsdb`で`entry->generateNhlfromRteList()`を呼び出している
   - この時点で、まだFunctionCostが適用されていない`NexthopList`が生成されている可能性

### コードフローの問題点

```cpp
// name-prefix-table.cpp:143-146
entry->generateNhlfromRteList();  // ← ここでFunctionCostが適用されていないNexthopListを生成
if (entry->getNexthopList().size() > 0) {
  m_fib.update(entry->getNamePrefix(), 
              adjustNexthopCosts(entry->getNexthopList(), entry->getNamePrefix(), *entry));
}
```

**問題:**
- `generateNhlfromRteList()`は`RoutingTablePoolEntry`から`NexthopList`を生成するが、この時点ではFunctionCostが適用されていない
- `adjustNexthopCosts`は正しく計算しているが、`Fib::update`に渡された後、何らかの理由で正しく処理されていない

## 検証が必要な点

### 1. `adjustNexthopCosts`の戻り値の確認

`adjustNexthopCosts`が返す`NexthopList`の内容をログで確認する必要があります。

### 2. `Fib::update`への引数の確認

`Fib::update`に渡される`NexthopList`の内容をログで確認する必要があります。

### 3. `hopsToAdd`の内容の確認

`Fib::update`内で`hopsToAdd`に追加されるNextHopのコストをログで確認する必要があります。

## 推測される原因

### 最も可能性が高い原因

`adjustNexthopCosts`が返す`NexthopList`のNextHopが、`Fib::update`内で既存のNextHopと正しくマッチングされていない可能性があります。

具体的には：
1. `adjustNexthopCosts`は`rtpe->getNexthopList()`からNextHopを取得してFunctionCostを適用
2. しかし、`Fib::update`は`allHops`から`maxFaces`個のNextHopを選択
3. この選択プロセスで、FunctionCostが適用されたNextHopが除外されている可能性

### 確認すべき点

1. `adjustNexthopCosts`が返す`NexthopList`のサイズと内容
2. `Fib::update`に渡される`allHops`のサイズと内容
3. `hopsToAdd`に追加されるNextHopのコスト

## 次のアクション

### 1. デバッグログの追加

`adjustNexthopCosts`と`Fib::update`の間で、NextHopの内容をログで確認できるようにデバッグログを追加します。

### 2. `Fib::update`の実装確認

`Fib::update`が`adjustNexthopCosts`が返す`NexthopList`を正しく処理しているか確認します。

### 3. `generateNhlfromRteList()`の呼び出しタイミング

`generateNhlfromRteList()`の呼び出しタイミングを調整し、FunctionCostが適用された後に呼び出すようにします。

### 4. テストの実行

修正後、再度テストを実行し、FIBエントリのコストが正しく更新されることを確認します。

