# ログ分析結果のまとめ

## ログの出力先

**確認結果**: NLSRのログは `/nlsr.log` に出力されている

- `startNFDandNLSR.sh`の112行目で、NLSRの標準出力と標準エラー出力が `/nlsr.log` にリダイレクトされている
- プロセスのファイルディスクリプタを確認すると、stdout(1)とstderr(2)が `/nlsr.log` にリダイレクトされている

## 発見された重大な問題

### 🔴 問題1: 監視機能の実装に致命的なバグがある

**症状**:
- `Started log file monitoring with interval: 5000ms` が5秒ごとに繰り返し出力されている
- `Log monitoring check triggered` などのデバッグログが出力されていない
- ログファイルの変更検出が動作していない

**原因**:
`startLogMonitoring()`の実装（617-618行目、623-624行目）で、監視チェックの関数内で再度`startLogMonitoring()`を呼んでいる：

```cpp
// 間違った実装
scheduler.schedule(ndn::time::milliseconds(intervalMs),
                  [this, &scheduler, intervalMs]() { startLogMonitoring(scheduler, intervalMs); });
```

これは間違った実装です。`startLogMonitoring()`が呼ばれるたびに、初期ハッシュ計算が実行され、新しい`monitorFunc`が作成されてスケジュールされます。その結果、監視チェックの関数（`monitorFunc`）が実行されず、ログファイルの変更検出が動作しません。

**修正方法**:
監視チェックの関数自体を再スケジュールするように変更する必要があります。

---

### ⚠️ 問題2: node2とnode3でログファイルが存在しない

**症状**:
- node2とnode3で `Cannot open log file for monitoring: /var/log/sidecar/service.log` が繰り返し出力されている

**原因**:
- node2とnode3には `/var/log/sidecar/service.log` が存在しない
- `ndn-node2.yaml`と`ndn-node3.yaml`に`sidecar-logs`ボリュームマウントが設定されていない可能性

**確認が必要**:
- `ndn-node2.yaml`と`ndn-node3.yaml`の設定を確認
- 必要に応じて、ボリュームマウントを追加

---

## ログの詳細分析

### node1のログ

**正常な出力**:
```
1762920926.973885  INFO: [nlsr.SidecarStatsHandler] Registering sidecar-stats dataset handler
1762920926.973977  INFO: [nlsr.Nlsr] SidecarStatsHandler initialized with log path: /var/log/sidecar/service.log
1762920926.974013  INFO: [nlsr.SidecarStatsHandler] Started log file monitoring with interval: 5000ms
```

**問題のある出力**:
```
1762920936.975154  INFO: [nlsr.SidecarStatsHandler] Started log file monitoring with interval: 5000ms
1762920946.976895  INFO: [nlsr.SidecarStatsHandler] Started log file monitoring with interval: 5000ms
...
```

**観察**:
- 監視機能は起動している
- しかし、`Log monitoring check triggered` などのデバッグログが出力されていない
- これは、監視チェックの関数（`monitorFunc`）が実行されていないことを示している
- ログファイルの変更検出のログも出力されていない

### node2とnode3のログ

```
1762921713.426856 DEBUG: [nlsr.SidecarStatsHandler] Cannot open log file for monitoring: /var/log/sidecar/service.log
1762921718.428207  INFO: [nlsr.SidecarStatsHandler] Started log file monitoring with interval: 5000ms
1762921723.428623 DEBUG: [nlsr.SidecarStatsHandler] Cannot open log file for monitoring: /var/log/sidecar/service.log
...
```

**観察**:
- ログファイルを開けないエラーが繰り返し出力されている
- 監視機能は起動しているが、ログファイルが存在しないため動作していない

---

## 修正が必要な箇所

### 1. `startLogMonitoring()`の実装を修正（最重要）

**現在の実装（間違い）**:
```cpp
std::function<void()> monitorFunc = [this, &scheduler, intervalMs]() {
  // 監視チェックの処理
  // ...
  
  // 間違い: startLogMonitoring()を再帰的に呼んでいる
  scheduler.schedule(ndn::time::milliseconds(intervalMs),
                    [this, &scheduler, intervalMs]() { startLogMonitoring(scheduler, intervalMs); });
};

scheduler.schedule(ndn::time::milliseconds(intervalMs), monitorFunc);
```

**正しい実装**:
監視チェックの関数自体を再スケジュールする必要があります。

```cpp
// 監視チェックの関数を定義（再帰的に呼べるように）
std::function<void()> monitorFunc;
monitorFunc = [this, &scheduler, intervalMs, &monitorFunc]() {
  NLSR_LOG_DEBUG("Log monitoring check triggered");
  try {
    // 監視チェックの処理
    // ...
    
    // 次のチェックをスケジュール（関数自体を再スケジュール）
    scheduler.schedule(ndn::time::milliseconds(intervalMs), monitorFunc);
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error in log monitoring: " + std::string(e.what()));
    // エラー時も継続
    scheduler.schedule(ndn::time::milliseconds(intervalMs), monitorFunc);
  }
};

// 最初のチェックをスケジュール
scheduler.schedule(ndn::time::milliseconds(intervalMs), monitorFunc);
```

### 2. node2とnode3の設定を確認

- `ndn-node2.yaml`と`ndn-node3.yaml`に`sidecar-logs`ボリュームマウントが設定されているか確認
- 必要に応じて、ボリュームマウントを追加

---

## 次のステップ

### 優先度: 最高 🔴

1. **`startLogMonitoring()`の実装を修正**
   - 監視チェックの関数を再スケジュールするように変更
   - 再帰的な`startLogMonitoring()`呼び出しを削除

2. **修正後のビルドとデプロイ**
   - 新しいビルドを作成
   - Kubernetes Podを再デプロイ

3. **検証**
   - ログを確認して、`Log monitoring check triggered`が出力されるか確認
   - ログファイルの変更検出が動作しているか確認
   - NameLSAが更新されるか確認

### 優先度: 高 ⚠️

4. **node2とnode3の設定を確認**
   - ボリュームマウントの設定を確認
   - 必要に応じて修正

---

## まとめ

**ログの出力先**: `/nlsr.log`（`startNFDandNLSR.sh`でリダイレクト）

**発見された問題**:
1. 🔴 **監視機能の実装に致命的なバグ**: `startLogMonitoring()`が再帰的に呼ばれ、監視チェックが実行されない
2. ⚠️ **node2とnode3でログファイルが存在しない**: ボリュームマウントの設定が必要

**修正が必要な箇所**:
1. `src/publisher/sidecar-stats-handler.cpp`の`startLogMonitoring()`関数（617-618行目、623-624行目）
2. `ndn-node2.yaml`と`ndn-node3.yaml`のボリュームマウント設定（必要に応じて）

**期待される結果**:
修正後、以下のログが出力されるはずです：
- `Log monitoring check triggered`（5秒ごと）
- `Current log file hash: ...`
- `Log file changed, updating NameLSA`（ログファイル変更時）
- `updateNameLsaWithStats called`
- `Updated NameLSA with Service Function info`

