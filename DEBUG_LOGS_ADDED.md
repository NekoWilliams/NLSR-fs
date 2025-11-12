# デバッグログ追加のまとめ

## 追加したデバッグログ

### 1. `startLogMonitoring()`関数
- 関数呼び出し時のログ（interval, logPath）
- 初期ハッシュ計算のログ
- 各監視チェック時のログ
- ログファイル変更検出時のログ
- エラーハンドリングのログ

### 2. `updateNameLsaWithStats()`関数
- 関数呼び出し時のログ
- 統計情報取得のログ
- ServiceFunctionInfo変換のログ
- NameLSA検索のログ
- NameLSA更新のログ

### 3. `parseSidecarLog()`関数
- 関数呼び出し時のログ（logPath）
- ファイルオープン成功のログ
- パース結果のログ（エントリ数、キー数、キー名）

## 期待されるログ出力

### 起動時
```
SidecarStatsHandler initialized with log path: /var/log/sidecar/service.log
startLogMonitoring called with interval: 5000ms, logPath: /var/log/sidecar/service.log
Initial log file hash calculated: <hash>... (size: <size> bytes)
Started log file monitoring with interval: 5000ms, logPath: /var/log/sidecar/service.log
```

### 監視チェック時（5秒ごと）
```
Log monitoring check triggered
Current log file hash: <hash>... (size: <size> bytes), last hash: <hash>...
Log file unchanged, skipping update
```

### ログファイル変更時
```
Log monitoring check triggered
Current log file hash: <new_hash>... (size: <size> bytes), last hash: <old_hash>...
Log file changed, updating NameLSA (old hash: <old_hash>..., new hash: <new_hash>...)
updateNameLsaWithStats called
Getting latest stats from log file...
parseSidecarLog called for: /var/log/sidecar/service.log
Log file opened successfully
Parsed <n> log entries from /var/log/sidecar/service.log
First entry has <n> keys
  Key: service_call_in_time = <value>
  Key: service_call_out_time = <value>
  ...
Retrieved <n> stat entries
Converting stats to ServiceFunctionInfo...
ServiceFunctionInfo: processingTime=<value>, load=<value>, usageCount=<value>
Looking for NameLSA for router: <router_name>
Service function prefix: /relay
Service Function info set in NameLSA
Rebuilding and installing NameLSA...
Updated NameLSA with Service Function info: prefix=/relay, processingTime=<value>, load=<value>, usageCount=<value>
```

## 次のステップ

1. **新しいビルドをデプロイ**
   - デバッグログを含む新しいビルドを作成
   - Kubernetes Podを再デプロイ

2. **検証スクリプトを実行**
   ```bash
   cd /home/katsutoshi/nlsr-sample-k8s
   ./verify_with_debug.sh
   ```

3. **ログを確認**
   - `startLogMonitoring called` が表示されるか
   - `Log monitoring check triggered` が5秒ごとに表示されるか
   - ログファイル変更時に `Log file changed` が表示されるか
   - `updateNameLsaWithStats called` が表示されるか

4. **問題の特定**
   - ログが表示されない場合: 監視機能が起動していない
   - `Cannot open log file` が表示される場合: ファイルパスの問題
   - `No valid statistics available` が表示される場合: パースの問題
   - `Own NameLSA not found` が表示される場合: NameLSAの検索の問題

## トラブルシューティング

### ログが表示されない場合
- ログレベルがDEBUGに設定されているか確認
- NLSRが新しいビルドで起動しているか確認
- ログが別の場所に出力されていないか確認

### 監視チェックが実行されない場合
- `startLogMonitoring` が呼ばれているか確認
- スケジューラーが正しく動作しているか確認
- エラーが発生していないか確認

### NameLSAが更新されない場合
- `updateNameLsaWithStats` が呼ばれているか確認
- 統計情報が正しく抽出されているか確認
- NameLSAの検索が成功しているか確認

