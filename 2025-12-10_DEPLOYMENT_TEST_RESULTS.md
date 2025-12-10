# 新規デプロイ後のテスト結果レポート

## テスト概要

hostPath設定を修正（`ndn-node1.yaml`と`ndn-node2.yaml`で`/mnt/sidecar-logs-node1`と`/mnt/sidecar-logs-node2`に分離）した後の動作確認テストを実施しました。

## テスト結果

### 1. Podの状態確認 ✓

すべてのPodが正常に起動しています：

```
NAME                         READY   STATUS    RESTARTS   AGE
ndn-node1-6fd6b456c5-4m7qp   1/1     Running   0          2m17s
ndn-node2-8b84d8587-kxcbj    1/1     Running   0          2m17s
ndn-node3-df67b7fd4-5jz96    1/1     Running   0          2m16s
ndn-node4-5f69f8c89-82vzg    1/1     Running   0          2m16s
ndn-service-pod1             2/2     Running   0          69s
ndn-service-pod2             2/2     Running   0          69s
```

### 2. hostPath設定の確認 ✓

**修正前の問題:**
- `ndn-node1.yaml`: `/mnt/sidecar-logs`
- `ndn-node2.yaml`: `/mnt/sidecar-logs`
- 両方が同じhostPathを使用していた

**修正後:**
- `ndn-node1.yaml`: `/mnt/sidecar-logs-node1` ✓
- `ndn-node2.yaml`: `/mnt/sidecar-logs-node2` ✓
- `relayPod.yaml`: `/mnt/sidecar-logs-node1` ✓
- `relayPod2.yaml`: `/mnt/sidecar-logs-node2` ✓

**確認結果:**
- node1のsidecarログファイル: `/var/log/sidecar/service.log` (0バイト、新規作成)
- node2のsidecarログファイル: `/var/log/sidecar/service.log` (更新されている)
- それぞれのノードが独自のログファイルを使用していることを確認

### 3. /sample2.txtの提供 ✓

- node1で`/sample2.txt`を正常に提供
- アドバタイズ成功
- Dataパケットの公開成功

### 4. /relay/sample2.txtへの通信テスト ✓

**結果:**
- `/relay/sample2.txt`へのリクエストが成功
- データの取得に成功（"Hello, world from node1"）
- 20回のリクエストすべてが成功

### 5. sidecarログの確認

**node1のsidecarログ:**
- ログファイルは存在するが、内容が空（0バイト）
- トラフィックがnode1にルーティングされていない可能性

**node2のsidecarログ:**
- ログが正常に更新されている
- 最新のProcessingTime: 1.821ミリ秒（以前は4.243ミリ秒）
- タイムスタンプ: 2025-12-10 17:02:51.255231

### 6. ProcessingTimeの比較

**node1:**
- sidecarログが空のため、ProcessingTimeを取得できず

**node2:**
- ProcessingTime: 1.821ミリ秒（0.001821秒）
- 以前の値（4.243ミリ秒）から改善

### 7. ルーティング設定の確認

**node3のFIB（/relay関連）:**
```
/relay nexthops={faceid=262 (cost=25), faceid=264 (cost=50)}
```

- faceid=262: cost=25（node2へのパス）
- faceid=264: cost=50（node1へのパス）
- 最小コストのパス（cost=25）が選択されているため、すべてのトラフィックがnode2にルーティングされている

## 問題点と分析

### 1. node1のsidecarログが空

**原因:**
- NLSRが最小コストのパスを選択しているため、すべてのトラフィックがnode2（cost=25）にルーティングされている
- node1へのパス（cost=50）は選択されていない

**影響:**
- node1のsidecarログが更新されない
- node1のProcessingTimeが取得できない
- トラフィック分散が機能していない

### 2. トラフィック分散の確認

**現状:**
- すべてのトラフィックがnode2にルーティングされている
- これは、NLSRが最小コストのパスを選択する正常な動作

**期待される動作:**
- FunctionCostが適用されれば、node1とnode2のコストが調整され、トラフィックが分散される可能性がある
- しかし、現在はFunctionCostが適用されていない可能性がある

## 成功した項目

1. ✓ hostPath設定の分離が正常に機能している
2. ✓ `/relay/sample2.txt`への通信が成功している
3. ✓ node2のsidecarログが正常に更新されている
4. ✓ node2のProcessingTimeが取得できている（1.821ミリ秒）

## 確認が必要な項目

1. **node1のsidecarログが空**
   - トラフィックがnode1にルーティングされていない
   - FunctionCostが適用されているか確認が必要

2. **トラフィック分散**
   - 現在、すべてのトラフィックがnode2にルーティングされている
   - FunctionCostが適用されれば、トラフィックが分散される可能性がある

3. **FunctionCostの適用**
   - NameLSAにService Function情報が含まれているか確認
   - wireEncode/wireDecodeが正常に動作しているか確認
   - NamePrefixTableでFunctionCostが計算されているか確認

## 次のステップ

1. **FunctionCostの適用状況を確認**
   - NameLSAにService Function情報が含まれているか確認
   - wireEncode/wireDecodeのデバッグログを確認
   - NamePrefixTableのadjustNexthopCostsが呼び出されているか確認

2. **トラフィック分散の確認**
   - FunctionCostが適用されれば、node1とnode2のコストが調整され、トラフィックが分散される可能性がある
   - 必要に応じて、トラフィック分散の設定を確認

3. **node1のsidecarログの確認**
   - トラフィックがnode1にルーティングされるようにする
   - または、node1から直接トラフィックを生成して、sidecarログが更新されるか確認

## 結論

hostPath設定の分離は正常に機能しており、node1とnode2がそれぞれ独自のログファイルを使用していることを確認しました。しかし、現在のルーティング設定では、すべてのトラフィックがnode2にルーティングされているため、node1のsidecarログが更新されていません。

FunctionCostが適用されれば、トラフィックが分散され、両方のノードのsidecarログが更新される可能性があります。FunctionCostの適用状況を確認する必要があります。

