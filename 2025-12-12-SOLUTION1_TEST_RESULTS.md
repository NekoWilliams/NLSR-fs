2025-12-12

# 案1実装後のテスト結果

## テスト実施日時
2025年12月12日

## 実装内容
案1: `adjustNexthopCosts`のシグネチャを変更し、`NamePrefixTableEntry`への参照を追加

### 変更点
1. `adjustNexthopCosts`のシグネチャ変更: `destRouterName`引数を削除し、`NamePrefixTableEntry`への参照を追加
2. 実装変更: 各`RoutingTablePoolEntry`に対して個別にFunctionCostを計算
3. 呼び出し元の修正: 4箇所（`updateFromLsdb`, `addEntry`×2, `removeEntry`）

## テスト結果

### 1. トラフィック生成 ✓
- `ndnputchunks`と`ndncatchunks`を使用してトラフィックを生成
- node2のsidecarログにサービス呼び出しが記録されている

### 2. FIBエントリの確認
**node3のFIBエントリ:**
```
/relay nexthops={faceid=262 (cost=25000), faceid=264 (cost=50000)}
```

**分析:**
- コストが25000と50000（25.0と50.0を1000倍）のまま
- FunctionCostが適用されていない可能性がある

### 3. ログ確認
- node3のログから`adjustNexthopCosts`の呼び出しログを確認できなかった
- `Processing RoutingTablePoolEntry`や`FunctionCost calculated`のログが確認できなかった

## 問題の可能性

### 1. adjustNexthopCostsが呼ばれていない
- ルーティング計算が実行されていない可能性
- NameLSA更新が検知されていない可能性

### 2. Service Function情報が取得できていない
- node3がnode1/node2のNameLSAからService Function情報を取得できていない
- NameLSAの伝播に時間がかかっている

### 3. FunctionCostが0になっている
- 利用率が計算されていない
- Service Function情報がNameLSAに含まれていない

## 次のステップ

### 1. ログレベルの確認
- NLSRのログレベルがDEBUGになっているか確認
- `adjustNexthopCosts`の呼び出しを確認

### 2. NameLSAの確認
- node1とnode2のNameLSAにService Function情報が含まれているか確認
- node3が正しくNameLSAを受信しているか確認

### 3. ルーティング計算のタイミング確認
- NameLSA更新時にルーティング計算がスケジュールされているか確認
- `updateFromLsdb`が正しく呼ばれているか確認

### 4. 実装の再確認
- `adjustNexthopCosts`の実装が正しいか確認
- 各`RoutingTablePoolEntry`に対してFunctionCostが計算されているか確認

## 期待される動作

### 正常な動作の場合
1. node1とnode2のsidecarログから利用率が計算される
2. NameLSAにService Function情報が含まれる
3. node3がNameLSAを受信し、`adjustNexthopCosts`が呼ばれる
4. 各`RoutingTablePoolEntry`に対して個別にFunctionCostが計算される
5. FIBエントリのコストにFunctionCostが加算される

### 例: 利用率2.6%（0.0260789）の場合
- FunctionCost = 0.0260789 × 100 = 2.60789
- リンクコスト25の場合: 最終コスト = 25 + 2.60789 = 27.60789 → 27608（整数化）
- リンクコスト50の場合: 最終コスト = 50 + 2.60789 = 52.60789 → 52608（整数化）

## 現在の状態
- FIBエントリのコスト: 25000と50000（FunctionCostが適用されていない）
- ログ確認: `adjustNexthopCosts`の呼び出しログが確認できなかった
- トラフィック: node2に到達しているが、node1には到達していない

