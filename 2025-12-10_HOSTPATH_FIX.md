# hostPath設定の修正レポート

## 問題の概要

`/relay/sample2.txt`にはアクセスできないが、`/sample2.txt`にはアクセスできる問題が発生していました。原因は、`ndn-node1.yaml`と`ndn-node2.yaml`の両方で同じ`/mnt/sidecar-logs`を使用していたことです。

## 発見された問題

### 1. ndn-node1.yamlとndn-node2.yamlのhostPath設定

**問題:**
- `ndn-node1.yaml`: `/mnt/sidecar-logs` (50行目)
- `ndn-node2.yaml`: `/mnt/sidecar-logs` (50行目)
- 両方が同じhostPathを使用していた

**影響:**
- node1とnode2が同じsidecarログファイルを参照していた可能性
- sidecarログが混在し、正しい統計情報が取得できなかった可能性

### 2. relayPod.yamlとrelayPod2.yamlのhostPath設定

**状態:**
- `relayPod.yaml`: `/mnt/sidecar-logs-node1` (63行目) ✓
- `relayPod2.yaml`: `/mnt/sidecar-logs-node2` (63行目) ✓
- 既に正しく分離されていた

## 修正内容

### 1. ndn-node1.yamlの修正

**変更前:**
```yaml
        - name: sidecar-logs
          hostPath:
            path: /mnt/sidecar-logs
            type: DirectoryOrCreate
```

**変更後:**
```yaml
        - name: sidecar-logs
          hostPath:
            path: /mnt/sidecar-logs-node1
            type: DirectoryOrCreate
```

### 2. ndn-node2.yamlの修正

**変更前:**
```yaml
        - name: sidecar-logs
          hostPath:
            path: /mnt/sidecar-logs
            type: DirectoryOrCreate
```

**変更後:**
```yaml
        - name: sidecar-logs
          hostPath:
            path: /mnt/sidecar-logs-node2
            type: DirectoryOrCreate
```

## 修正後の設定

### node1関連
- `ndn-node1.yaml`: `/mnt/sidecar-logs-node1`
- `relayPod.yaml`: `/mnt/sidecar-logs-node1`

### node2関連
- `ndn-node2.yaml`: `/mnt/sidecar-logs-node2`
- `relayPod2.yaml`: `/mnt/sidecar-logs-node2`

### node3, node4
- sidecar-logsの設定なし（正常）

## 次のステップ

1. **Podを再デプロイ**
   ```bash
   kubectl delete -f ndn-node1.yaml
   kubectl delete -f ndn-node2.yaml
   kubectl apply -f ndn-node1.yaml
   kubectl apply -f ndn-node2.yaml
   ```

2. **通信テストを実行**
   - `/relay/sample2.txt`へのアクセスが成功するか確認
   - sidecarログが正しく分離されているか確認

3. **ProcessingTimeの確認**
   - node1とnode2で異なるProcessingTimeが記録されるか確認

## 期待される効果

1. **sidecarログの分離**
   - node1とnode2がそれぞれ独自のsidecarログファイルを使用
   - 混在が解消される

2. **通信の正常化**
   - `/relay/sample2.txt`へのアクセスが成功する
   - sidecarが正しく動作する

3. **ProcessingTimeの正確性**
   - それぞれのノードで異なるProcessingTimeが記録される
   - 正しい統計情報に基づいたルーティングが可能になる

