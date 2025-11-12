# NLSR-fs Service Function Routing 動作確認手順

## 実装された機能

1. **ログファイル監視**: SidecarStatsHandlerが`/var/log/sidecar/service.log`を5秒間隔で監視
2. **NameLSA更新**: 統計情報をNameLSAに反映し、ネットワーク内で同期
3. **ルーティング計算**: Service Function情報（processingTime, load, usageCount）を考慮したルーティング

## 確認手順

### ステップ1: ログファイルの存在と内容確認

```bash
# node1のPodに接続
kubectl exec -it ndn-node1-<pod-id> -- bash

# ログファイルの存在確認
ls -la /var/log/sidecar/service.log

# ログファイルの内容確認（JSON形式の統計情報が記録されているか）
cat /var/log/sidecar/service.log
# または
tail -f /var/log/sidecar/service.log
```

**期待される結果**: JSON形式の統計情報が記録されている
```json
{"processing_time": 0.123, "load": 0.5, "usage_count": 10, ...}
```

### ステップ2: NLSRのログで監視開始を確認

```bash
# NLSRのログを確認
kubectl logs ndn-node1-<pod-id> | grep -i "sidecar\|monitoring"

# またはリアルタイムで確認
kubectl logs -f ndn-node1-<pod-id> | grep -i "sidecar\|monitoring"
```

**期待されるログメッセージ**:
- `SidecarStatsHandler initialized with log path: /var/log/sidecar/service.log`
- `Started log file monitoring with interval: 5000ms`
- `Log file changed, updating NameLSA`
- `Updated NameLSA with Service Function info: prefix=/relay, processingTime=..., load=..., usageCount=...`

### ステップ3: NameLSAにService Function情報が含まれているか確認

```bash
# node1のPodに接続
kubectl exec -it ndn-node1-<pod-id> -- bash

# LSDBのNameLSAを確認
nlsrc -k lsdb | grep -A 30 "NAME LSA"

# または詳細表示
nlsrc -k lsdb
```

**期待される結果**: NameLSAにService Function情報が含まれている（TLVエンコードされているため、直接確認は難しいが、シーケンス番号が更新されていることを確認）

### ステップ4: NameLSAのシーケンス番号の更新を確認

```bash
# 初期状態のシーケンス番号を記録
kubectl exec ndn-node1-<pod-id> -- nlsrc -k lsdb | grep "NAME LSA" | head -1

# ログファイルを更新（sidecarが新しい統計情報を書き込む）
# または手動でログファイルに追記
kubectl exec ndn-node1-<pod-id> -- sh -c 'echo "{\"processing_time\": 0.2, \"load\": 0.6, \"usage_count\": 15}" >> /var/log/sidecar/service.log'

# 5秒待機（監視間隔）
sleep 6

# シーケンス番号が更新されているか確認
kubectl exec ndn-node1-<pod-id> -- nlsrc -k lsdb | grep "NAME LSA" | head -1
```

**期待される結果**: シーケンス番号が増加している

### ステップ5: ルーティングテーブルでのService Function情報の反映確認

```bash
# ルーティングテーブルを確認
kubectl exec ndn-node1-<pod-id> -- nlsrc -k routing

# または特定のプレフィックスへのルートを確認
kubectl exec ndn-node1-<pod-id> -- nlsrc -k routing | grep "/relay"
```

**期待される結果**: Service Function情報を考慮したコストでルーティングが計算されている

### ステップ6: 他のノードでのNameLSA同期確認

```bash
# node2でnode1のNameLSAを確認
kubectl exec ndn-node2-<pod-id> -- nlsrc -k lsdb | grep -A 20 "node1"
```

**期待される結果**: node1のNameLSAがnode2に同期され、Service Function情報が含まれている

### ステップ7: 実際のルーティング動作確認

```bash
# node1から/relayプレフィックスへのInterestを送信
kubectl exec ndn-node1-<pod-id> -- ndnpeek -f 1 /relay/test

# または他のノードから
kubectl exec ndn-node2-<pod-id> -- ndnpeek -f 1 /relay/test
```

**期待される結果**: Service Function情報を考慮した最適なパスでルーティングされる

## トラブルシューティング

### ログファイルが存在しない場合

```bash
# ログディレクトリを作成
kubectl exec ndn-node1-<pod-id> -- mkdir -p /var/log/sidecar

# テスト用のログエントリを作成
kubectl exec ndn-node1-<pod-id> -- sh -c 'echo "{\"processing_time\": 0.1, \"load\": 0.3, \"usage_count\": 5}" > /var/log/sidecar/service.log'
```

### NLSRがログファイルを監視していない場合

```bash
# NLSRの設定を確認
kubectl exec ndn-node1-<pod-id> -- cat /etc/nlsr/nlsr.conf | grep -A 5 "sidecar-log-path"

# NLSRを再起動
kubectl delete pod ndn-node1-<pod-id>
# （新しいPodが起動するまで待機）
```

### Service Function情報がNameLSAに反映されない場合

```bash
# NLSRのログでエラーを確認
kubectl logs ndn-node1-<pod-id> | grep -i "error\|warn"

# ログファイルの形式を確認（JSON形式である必要がある）
kubectl exec ndn-node1-<pod-id> -- cat /var/log/sidecar/service.log
```

## 確認チェックリスト

- [ ] ログファイルが存在し、統計情報が記録されている
- [ ] NLSRのログで監視開始メッセージが表示されている
- [ ] ログファイル更新時にNameLSAのシーケンス番号が増加する
- [ ] ルーティングテーブルが更新されている
- [ ] 他のノードでNameLSAが同期されている
- [ ] 実際のルーティングがService Function情報を考慮している

## 詳細なデバッグ方法

### NLSRのデバッグログを有効化

```bash
# NLSRの設定ファイルでログレベルを変更
# /etc/nlsr/nlsr.conf の logging セクションを確認
```

### 手動でNameLSAを更新

```bash
# ログファイルを直接更新して監視をトリガー
kubectl exec ndn-node1-<pod-id> -- sh -c 'echo "{\"processing_time\": 0.15, \"load\": 0.4, \"usage_count\": 8}" >> /var/log/sidecar/service.log'
```

### Service Function情報の直接確認

```bash
# sidecar-stats datasetを直接取得
kubectl exec ndn-node1-<pod-id> -- nlsrc -k status sidecar-stats
```

