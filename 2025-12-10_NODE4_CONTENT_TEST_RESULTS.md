# node4コンテンツ提供テスト結果レポート

## テスト概要

コンテンツ提供元をnode1からnode4に変更し、node3から取得するテストを実施しました。これにより、node1とnode2の両方の`/relay`が利用される可能性を確認しました。

## テスト方法の変更

**変更前:**
- コンテンツ提供: node1
- コンテンツ取得: node3

**変更後:**
- コンテンツ提供: node4
- コンテンツ取得: node3

## テスト結果

### 1. node4で/sample3.txtを提供 ✓

- node4で`/sample3.txt`を正常に提供
- アドバタイズ成功
- Dataパケットの公開成功

### 2. node3から/relay/sample3.txtへの通信テスト ✓

**結果:**
- `/relay/sample3.txt`へのリクエストが成功
- データの取得に成功（"Hello, world from node4"）
- 20回のリクエストすべてが成功

### 3. sidecarログの確認

**node1のsidecarログ:**
- ログファイルは存在するが、内容が空（0バイト）
- トラフィックがnode1にルーティングされていない

**node2のsidecarログ:**
- ログが正常に更新されている（17KB）
- 最新のProcessingTime: 1.222ミリ秒（以前は1.821ミリ秒）
- タイムスタンプ: 2025-12-10 17:05:37.565398

### 4. ProcessingTimeの比較

**node1:**
- sidecarログが空のため、ProcessingTimeを取得できず

**node2:**
- ProcessingTime: 1.222ミリ秒（0.001222秒）
- 以前の値（1.821ミリ秒）から改善

### 5. ルーティング設定の確認

**node3のFIB（/relay関連）:**
```
/relay nexthops={faceid=262 (cost=25), faceid=264 (cost=50)}
```

- faceid=262: cost=25（node2へのパス）
- faceid=264: cost=50（node1へのパス）
- 最小コストのパス（cost=25）が選択されているため、すべてのトラフィックがnode2にルーティングされている

## 問題点と分析

### 1. node1のsidecarログが依然として空

**原因:**
- NLSRが最小コストのパスを選択しているため、すべてのトラフィックがnode2（cost=25）にルーティングされている
- node1へのパス（cost=50）は選択されていない
- コンテンツ提供元をnode4に変更しても、ルーティング結果は変わらなかった

**影響:**
- node1のsidecarログが更新されない
- node1のProcessingTimeが取得できない
- トラフィック分散が機能していない

### 2. FunctionCostの適用状況

**現状:**
- node3のFIBでは、node2へのコスト（25）がnode1へのコスト（50）より低い
- FunctionCostが適用されていない可能性がある
- または、FunctionCostが適用されていても、node2のコストが依然として低いまま

**期待される動作:**
- FunctionCostが適用されれば、node1とnode2のコストが調整され、トラフィックが分散される可能性がある
- しかし、現在はFunctionCostが適用されていない可能性がある

## 成功した項目

1. ✓ node4でコンテンツを正常に提供
2. ✓ `/relay/sample3.txt`への通信が成功している
3. ✓ node2のsidecarログが正常に更新されている
4. ✓ node2のProcessingTimeが取得できている（1.222ミリ秒）

## 確認が必要な項目

1. **node1のsidecarログが依然として空**
   - コンテンツ提供元をnode4に変更しても、トラフィックがnode1にルーティングされていない
   - FunctionCostが適用されているか確認が必要

2. **FunctionCostの適用**
   - NameLSAにService Function情報が含まれているか確認
   - wireEncode/wireDecodeが正常に動作しているか確認
   - NamePrefixTableでFunctionCostが計算されているか確認
   - adjustNexthopCostsが呼び出されているか確認

3. **トラフィック分散**
   - 現在、すべてのトラフィックがnode2にルーティングされている
   - FunctionCostが適用されれば、トラフィックが分散される可能性がある

## 次のステップ

1. **FunctionCostの適用状況を詳細に確認**
   - NameLSAにService Function情報が含まれているか確認
   - wireEncode/wireDecodeのデバッグログを確認
   - NamePrefixTableのadjustNexthopCostsが呼び出されているか確認
   - FunctionCostが計算されているか確認

2. **トラフィック分散の確認**
   - FunctionCostが適用されれば、node1とnode2のコストが調整され、トラフィックが分散される可能性がある
   - 必要に応じて、トラフィック分散の設定を確認

3. **node1のsidecarログの確認**
   - トラフィックがnode1にルーティングされるようにする
   - または、node1から直接トラフィックを生成して、sidecarログが更新されるか確認

## 結論

コンテンツ提供元をnode4に変更しても、node1のsidecarログが依然として空でした。これは、NLSRが最小コストのパス（node2、cost=25）を選択しているため、すべてのトラフィックがnode2にルーティングされているためです。

FunctionCostが適用されれば、node1とnode2のコストが調整され、トラフィックが分散される可能性があります。FunctionCostの適用状況を詳細に確認する必要があります。

