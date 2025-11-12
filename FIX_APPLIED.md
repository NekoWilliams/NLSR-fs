# 修正実施内容

## 修正日時
2025-11-12

## 修正内容

### 1. `startLogMonitoring()`の実装を修正 ✅

**問題**:
- 監視チェックの関数内で再度`startLogMonitoring()`を再帰的に呼んでいた
- これにより、監視チェックの関数（`monitorFunc`）が実行されず、ログファイルの変更検出が動作しなかった

**修正前**:
```cpp
std::function<void()> monitorFunc = [this, &scheduler, intervalMs]() {
  // 監視チェックの処理
  // ...
  
  // 間違い: startLogMonitoring()を再帰的に呼んでいる
  scheduler.schedule(ndn::time::milliseconds(intervalMs),
                    [this, &scheduler, intervalMs]() { startLogMonitoring(scheduler, intervalMs); });
};
```

**修正後**:
```cpp
// Use shared_ptr to allow recursive scheduling of the same function
auto monitorFunc = std::make_shared<std::function<void()>>();
*monitorFunc = [this, &scheduler, intervalMs, monitorFunc]() {
  // 監視チェックの処理
  // ...
  
  // 正しい: 同じ関数を再スケジュール
  scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
};
```

**変更点**:
- `std::function<void()>`を`std::shared_ptr<std::function<void()>>`に変更
- ラムダ式内で`monitorFunc`をキャプチャして、同じ関数を再スケジュールするように変更
- `startLogMonitoring()`を再帰的に呼ぶのではなく、`monitorFunc`自体を再スケジュール

**期待される効果**:
- 監視チェックの関数が5秒ごとに正しく実行される
- `Log monitoring check triggered`のログが出力される
- ログファイルの変更検出が動作する
- NameLSAが更新される

---

## 修正されたファイル

### `/home/katsutoshi/NLSR-fs/src/publisher/sidecar-stats-handler.cpp`
- `startLogMonitoring()`関数（585-629行目）を修正

---

## 次のステップ

### 1. ビルドとデプロイ
```bash
cd /home/katsutoshi/NLSR-fs
# ビルド
./waf configure
./waf build

# デプロイ（Dockerイメージの再ビルドとKubernetes Podの再デプロイ）
```

### 2. 検証
修正後、以下のログが出力されるはずです：

**起動時**:
```
INFO: [nlsr.SidecarStatsHandler] startLogMonitoring called with interval: 5000ms, logPath: /var/log/sidecar/service.log
DEBUG: [nlsr.SidecarStatsHandler] Initial log file hash calculated: <hash>... (size: <size> bytes)
INFO: [nlsr.SidecarStatsHandler] Started log file monitoring with interval: 5000ms, logPath: /var/log/sidecar/service.log
```

**監視チェック時（5秒ごと）**:
```
DEBUG: [nlsr.SidecarStatsHandler] Log monitoring check triggered
DEBUG: [nlsr.SidecarStatsHandler] Current log file hash: <hash>... (size: <size> bytes), last hash: <hash>...
DEBUG: [nlsr.SidecarStatsHandler] Log file unchanged, skipping update
```

**ログファイル変更時**:
```
DEBUG: [nlsr.SidecarStatsHandler] Log monitoring check triggered
DEBUG: [nlsr.SidecarStatsHandler] Current log file hash: <new_hash>... (size: <size> bytes), last hash: <old_hash>...
INFO: [nlsr.SidecarStatsHandler] Log file changed, updating NameLSA (old hash: <old_hash>..., new hash: <new_hash>...)
DEBUG: [nlsr.SidecarStatsHandler] updateNameLsaWithStats called
DEBUG: [nlsr.SidecarStatsHandler] Getting latest stats from log file...
DEBUG: [nlsr.SidecarStatsHandler] parseSidecarLog called for: /var/log/sidecar/service.log
DEBUG: [nlsr.SidecarStatsHandler] Log file opened successfully
DEBUG: [nlsr.SidecarStatsHandler] Parsed <n> log entries from /var/log/sidecar/service.log
DEBUG: [nlsr.SidecarStatsHandler] Retrieved <n> stat entries
DEBUG: [nlsr.SidecarStatsHandler] Converting stats to ServiceFunctionInfo...
DEBUG: [nlsr.SidecarStatsHandler] ServiceFunctionInfo: processingTime=<value>, load=<value>, usageCount=<value>
DEBUG: [nlsr.SidecarStatsHandler] Looking for NameLSA for router: <router_name>
DEBUG: [nlsr.SidecarStatsHandler] Service function prefix: /relay
DEBUG: [nlsr.SidecarStatsHandler] Service Function info set in NameLSA
DEBUG: [nlsr.SidecarStatsHandler] Rebuilding and installing NameLSA...
INFO: [nlsr.SidecarStatsHandler] Updated NameLSA with Service Function info: prefix=/relay, processingTime=<value>, load=<value>, usageCount=<value>
```

### 3. 確認項目
- [ ] `Log monitoring check triggered`が5秒ごとに出力される
- [ ] ログファイルの変更検出が動作する
- [ ] NameLSAのシーケンス番号が更新される
- [ ] 統計情報が正しく抽出される

---

## 注意事項

### node2とnode3について
- node2とnode3の設定ファイルには`sidecar-log-path`が設定されているが、`ndn-node2.yaml`と`ndn-node3.yaml`には`sidecar-logs`ボリュームマウントが設定されていない
- これらのノードでService Function routingを使用する場合は、ボリュームマウントを追加する必要がある
- 使用しない場合は、設定ファイルから`sidecar-log-path`を削除するか、ログファイルのパスを設定しないようにする

---

## 技術的な詳細

### 修正の理由
C++のラムダ式では、自分自身を再帰的に呼ぶことが難しい。`std::function`を直接キャプチャしようとすると、循環参照の問題が発生する。

解決策として、`std::shared_ptr<std::function<void()>>`を使用することで、ラムダ式内で自分自身をキャプチャし、再スケジュールできるようにした。

### 実装のポイント
1. `std::make_shared<std::function<void()>>()`で関数オブジェクトを作成
2. ラムダ式内で`monitorFunc`をキャプチャ
3. スケジューラーに`*monitorFunc`を渡すことで、同じ関数を再スケジュール

これにより、監視チェックの関数が5秒ごとに正しく実行され、ログファイルの変更検出が動作するようになる。

