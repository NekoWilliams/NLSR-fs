2025-12-12

# 問題点検証テスト結果

## 検証実施日時
2025年12月12日

## 検証項目

### 1. トラフィック生成の確認 ✓
- `ndnputchunks`と`ndncatchunks`を使用してトラフィックを生成
- トラフィックは正常に動作している（node3から/relayへの通信成功）

### 2. Sidecarログの確認
**node1のsidecarログ:**
- ファイルサイズ: 0バイト（ログなし）

**node2のsidecarログ:**
- 2件のサービス呼び出しが記録されている
- 最新の呼び出し: 2025-12-12 06:52:22

**分析:**
- node1にはトラフィックが到達していない
- node2には一部のトラフィックが到達している
- すべてのトラフィックがnode2にルーティングされている可能性

### 3. FIBエントリの確認
**node3のFIBエントリ:**
```
/relay nexthops={faceid=262 (cost=25000), faceid=264 (cost=50000)}
```

**分析:**
- コストが25000と50000（25.0と50.0を1000倍）のまま
- FunctionCostが適用されていない
- リンクコストのみでルーティングが決定されている

### 4. NameLSAの確認
**問題:**
- `nlsrc status`コマンドでNameLSAの詳細を取得できなかった
- Service Function情報がNameLSAに含まれているかを確認できなかった

### 5. ログの確認
**問題:**
- `adjustNexthopCosts`の呼び出しログが確認できなかった
- `Processing RoutingTablePoolEntry`のログが確認できなかった
- `FunctionCost calculated`のログが確認できなかった

**可能性:**
1. ログレベルがDEBUGになっていない
2. `adjustNexthopCosts`が呼ばれていない
3. ログが別の場所に出力されている

## 問題の分析

### 問題1: FunctionCostが適用されていない

**症状:**
- FIBエントリのコストが25000と50000のまま（リンクコストのみ）
- FunctionCostが加算されていない

**原因の可能性:**
1. `adjustNexthopCosts`が呼ばれていない
   - ルーティング計算が実行されていない
   - NameLSA更新が検知されていない
2. Service Function情報がNameLSAに含まれていない
   - node1/node2のNameLSAにService Function情報が設定されていない
   - NameLSAの伝播に時間がかかっている
3. `adjustNexthopCosts`内でService Function情報が取得できていない
   - NameLSAからService Function情報を取得できていない
   - 利用率が0のためFunctionCostが0になっている

### 問題2: ログが確認できない

**症状:**
- `adjustNexthopCosts`の呼び出しログが確認できない
- `Processing RoutingTablePoolEntry`のログが確認できない

**原因の可能性:**
1. ログレベルがDEBUGになっていない
   - 設定ファイルでログレベルが設定されていない
   - デフォルトのログレベルがINFO以下
2. ログが別の場所に出力されている
   - 標準出力ではなくファイルに出力されている
   - ログファイルの場所が異なる

### 問題3: トラフィック分散が機能していない

**症状:**
- node1にはトラフィックが到達していない
- node2には一部のトラフィックが到達している

**原因:**
- FunctionCostが適用されていないため、リンクコストのみでルーティングが決定されている
- node2へのリンクコスト（25）がnode1へのリンクコスト（50）より低いため、すべてのトラフィックがnode2にルーティングされている

## 次の検証ステップ

### 1. ログレベルの確認
- NLSRの設定ファイルでログレベルを確認
- 必要に応じてログレベルをDEBUGに設定

### 2. NameLSAの内容確認
- node1/node2のNameLSAにService Function情報が含まれているか確認
- NameLSAのwireEncode/wireDecodeを確認

### 3. adjustNexthopCostsの呼び出し確認
- `addEntry`や`updateFromLsdb`が呼ばれているか確認
- `adjustNexthopCosts`の呼び出し元を確認

### 4. ルーティング計算のタイミング確認
- NameLSA更新時にルーティング計算がスケジュールされているか確認
- `updateFromLsdb`が正しく呼ばれているか確認

### 5. Service Function情報の伝播確認
- node1/node2のNameLSAにService Function情報が設定されているか確認
- node3が正しくNameLSAを受信しているか確認

## 実装の確認事項

### adjustNexthopCostsの実装
- 各`RoutingTablePoolEntry`に対して個別にFunctionCostを計算している ✓
- `destRouterName`から正しくNameLSAを取得している ✓
- Service Function情報からFunctionCostを計算している ✓

### 呼び出し元の実装
- `addEntry`で`adjustNexthopCosts`を呼んでいる ✓
- `updateFromLsdb`で`adjustNexthopCosts`を呼んでいる ✓
- `removeEntry`で`adjustNexthopCosts`を呼んでいる ✓

## 推奨される次のアクション

1. **ログレベルの設定**
   - NLSRの設定ファイルでログレベルをDEBUGに設定
   - ログの出力先を確認

2. **NameLSAの詳細確認**
   - node1/node2のNameLSAにService Function情報が含まれているか確認
   - NameLSAのwireEncode/wireDecodeを確認

3. **手動でのルーティング計算トリガー**
   - NameLSAを手動で更新してルーティング計算をトリガー
   - `adjustNexthopCosts`が呼ばれるか確認

4. **デバッグログの追加**
   - `addEntry`や`updateFromLsdb`の呼び出し時にログを追加
   - `adjustNexthopCosts`の呼び出しを確実に確認できるようにする

