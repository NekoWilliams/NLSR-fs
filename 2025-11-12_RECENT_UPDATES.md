# NLSR-fs 直近の更新内容

## 更新日時
2025-11-12

---

## 更新内容の概要

直近のNLSR-fsの更新では、以下の2つの主要な変更が実施されました：

1. **デバッグログの追加** - 監視機能の動作を確認するための詳細なログを追加
2. **監視機能の致命的なバグの修正** - `startLogMonitoring()`の実装を修正し、監視チェックが正しく実行されるように変更

---

## 1. デバッグログの追加

### 目的
監視機能が実際に動作しているか、どのステップで問題が発生しているかを特定するため、詳細なデバッグログを追加しました。

### 追加されたログ

#### `startLogMonitoring()`関数
- 関数呼び出し時のログ（interval, logPath）
- 初期ハッシュ計算のログ
- 各監視チェック時のログ
- ログファイル変更検出時のログ
- エラーハンドリングのログ

**追加されたログメッセージ**:
```cpp
NLSR_LOG_INFO("startLogMonitoring called with interval: " << intervalMs << "ms, logPath: " << m_logPath);
NLSR_LOG_DEBUG("Initial log file hash calculated: " << m_lastLogHash.substr(0, 16) << "... (size: " << content.size() << " bytes)");
NLSR_LOG_DEBUG("Log monitoring check triggered");
NLSR_LOG_DEBUG("Current log file hash: " << currentHash.substr(0, 16) << "... (size: " << content.size() << " bytes), last hash: " << lastHashPreview << "...");
NLSR_LOG_INFO("Log file changed, updating NameLSA (old hash: " << oldHashPreview << "..., new hash: " << currentHash.substr(0, 16) << "...)");
NLSR_LOG_DEBUG("Log file unchanged, skipping update");
```

#### `updateNameLsaWithStats()`関数
- 関数呼び出し時のログ
- 統計情報取得のログ
- ServiceFunctionInfo変換のログ
- NameLSA検索のログ
- NameLSA更新のログ

**追加されたログメッセージ**:
```cpp
NLSR_LOG_DEBUG("updateNameLsaWithStats called");
NLSR_LOG_DEBUG("Getting latest stats from log file...");
NLSR_LOG_DEBUG("Retrieved " << stats.size() << " stat entries");
NLSR_LOG_DEBUG("Converting stats to ServiceFunctionInfo...");
NLSR_LOG_DEBUG("ServiceFunctionInfo: processingTime=" << sfInfo.processingTime << ", load=" << sfInfo.load << ", usageCount=" << sfInfo.usageCount);
NLSR_LOG_DEBUG("Looking for NameLSA for router: " << routerPrefix);
NLSR_LOG_DEBUG("Service function prefix: " << servicePrefix);
NLSR_LOG_DEBUG("Service Function info set in NameLSA");
NLSR_LOG_DEBUG("Rebuilding and installing NameLSA...");
```

#### `parseSidecarLog()`関数
- 関数呼び出し時のログ（logPath）
- ファイルオープン成功のログ
- パース結果のログ（エントリ数、キー数、キー名）

**追加されたログメッセージ**:
```cpp
NLSR_LOG_DEBUG("parseSidecarLog called for: " << m_logPath);
NLSR_LOG_DEBUG("Log file opened successfully");
NLSR_LOG_DEBUG("Parsed " << logEntries.size() << " log entries from " << m_logPath);
NLSR_LOG_DEBUG("First entry has " << logEntries[0].size() << " keys");
NLSR_LOG_DEBUG("  Key: " << key << " = " << value.substr(0, 50));
```

### 効果
これらのデバッグログにより、以下の情報が確認できるようになりました：
- 監視機能が起動しているか
- 監視チェックが実行されているか
- ログファイルの読み取りが成功しているか
- ハッシュ計算が正しく動作しているか
- ログファイルの変更検出が動作しているか
- NameLSA更新の各ステップが実行されているか

---

## 2. 監視機能の致命的なバグの修正

### 問題の詳細

**発見された問題**:
`startLogMonitoring()`の実装に致命的なバグがあり、監視チェックの関数（`monitorFunc`）が実行されていませんでした。

**問題の症状**:
- `Started log file monitoring with interval: 5000ms` が5秒ごとに繰り返し出力される
- `Log monitoring check triggered` などのデバッグログが出力されない
- ログファイルの変更検出が動作しない
- NameLSAが更新されない

**問題の原因**:
監視チェックの関数内で、`startLogMonitoring()`を再帰的に呼んでいたため、監視チェックの関数自体が実行されず、代わりに`startLogMonitoring()`が再度呼ばれていました。

### 修正前のコード

```cpp
std::function<void()> monitorFunc = [this, &scheduler, intervalMs]() {
  NLSR_LOG_DEBUG("Log monitoring check triggered");
  try {
    // 監視チェックの処理
    // ...
    
    // 間違い: startLogMonitoring()を再帰的に呼んでいる
    scheduler.schedule(ndn::time::milliseconds(intervalMs),
                      [this, &scheduler, intervalMs]() { startLogMonitoring(scheduler, intervalMs); });
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error in log monitoring: " + std::string(e.what()));
    // 間違い: startLogMonitoring()を再帰的に呼んでいる
    scheduler.schedule(ndn::time::milliseconds(intervalMs),
                      [this, &scheduler, intervalMs]() { startLogMonitoring(scheduler, intervalMs); });
  }
};

scheduler.schedule(ndn::time::milliseconds(intervalMs), monitorFunc);
```

**問題点**:
1. `startLogMonitoring()`が呼ばれるたびに、初期ハッシュ計算が実行される
2. 新しい`monitorFunc`が作成されてスケジュールされる
3. 監視チェックの関数（`monitorFunc`）が実行されない
4. ログファイルの変更検出が動作しない

### 修正後のコード

```cpp
// Schedule periodic monitoring
// Use shared_ptr to allow recursive scheduling of the same function
auto monitorFunc = std::make_shared<std::function<void()>>();
*monitorFunc = [this, &scheduler, intervalMs, monitorFunc]() {
  NLSR_LOG_DEBUG("Log monitoring check triggered");
  try {
    // Read current log file
    std::ifstream logFile(m_logPath);
    if (!logFile.is_open()) {
      NLSR_LOG_DEBUG("Cannot open log file for monitoring: " + m_logPath);
      // Schedule next check using the same function
      scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
      return;
    }
    
    std::string content((std::istreambuf_iterator<char>(logFile)),
                        std::istreambuf_iterator<char>());
    std::hash<std::string> hasher;
    std::string currentHash = std::to_string(hasher(content));
    
    std::string lastHashPreview = m_lastLogHash.empty() ? "(empty)" : m_lastLogHash.substr(0, std::min(16UL, m_lastLogHash.size()));
    NLSR_LOG_DEBUG("Current log file hash: " << currentHash.substr(0, std::min(16UL, currentHash.size())) << "... (size: " << content.size() << " bytes), last hash: " << lastHashPreview << "...");
    
    // Check if log file has changed
    if (m_lastLogHash.empty() || currentHash != m_lastLogHash) {
      std::string oldHashPreview = m_lastLogHash.empty() ? "(empty)" : m_lastLogHash.substr(0, std::min(16UL, m_lastLogHash.size()));
      NLSR_LOG_INFO("Log file changed, updating NameLSA (old hash: " << oldHashPreview << "..., new hash: " << currentHash.substr(0, std::min(16UL, currentHash.size())) << "...)");
      m_lastLogHash = currentHash;
      updateNameLsaWithStats();
    } else {
      NLSR_LOG_DEBUG("Log file unchanged, skipping update");
    }
    
    // Schedule next check using the same function
    scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error in log monitoring: " + std::string(e.what()));
    // Continue monitoring even on error
    scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
  }
};

scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
```

**修正のポイント**:
1. `std::function<void()>`を`std::shared_ptr<std::function<void()>>`に変更
2. ラムダ式内で`monitorFunc`をキャプチャして、同じ関数を再スケジュール
3. `startLogMonitoring()`を再帰的に呼ぶのではなく、`monitorFunc`自体を再スケジュール

### 技術的な詳細

#### なぜ`std::shared_ptr`が必要なのか

C++のラムダ式では、自分自身を再帰的に呼ぶことが難しい問題があります。`std::function`を直接キャプチャしようとすると、循環参照の問題が発生します。

**解決策**:
`std::shared_ptr<std::function<void()>>`を使用することで、ラムダ式内で自分自身をキャプチャし、再スケジュールできるようにしました。

#### 実装のポイント

1. **`std::make_shared<std::function<void()>>()`で関数オブジェクトを作成**
   - 関数オブジェクトを`shared_ptr`で管理することで、ラムダ式内で安全にキャプチャできる

2. **ラムダ式内で`monitorFunc`をキャプチャ**
   - `[this, &scheduler, intervalMs, monitorFunc]()`で`monitorFunc`をキャプチャ
   - `monitorFunc`は`shared_ptr`なので、コピーしても同じオブジェクトを参照する

3. **スケジューラーに`*monitorFunc`を渡す**
   - `scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc)`で同じ関数を再スケジュール
   - これにより、監視チェックの関数が5秒ごとに正しく実行される

### 修正前後の動作の違い

#### 修正前の動作
1. `startLogMonitoring()`が呼ばれる
2. 初期ハッシュ計算が実行される
3. `monitorFunc`が作成され、5秒後にスケジュールされる
4. 5秒後、`monitorFunc`が実行される代わりに、`startLogMonitoring()`が再帰的に呼ばれる
5. 初期ハッシュ計算が再度実行される
6. 新しい`monitorFunc`が作成され、5秒後にスケジュールされる
7. **監視チェックが実行されない**

#### 修正後の動作
1. `startLogMonitoring()`が呼ばれる
2. 初期ハッシュ計算が実行される
3. `monitorFunc`が作成され、5秒後にスケジュールされる
4. 5秒後、`monitorFunc`が実行される
5. ログファイルを読み取り、ハッシュを計算
6. 変更を検出した場合、`updateNameLsaWithStats()`を呼ぶ
7. 同じ`monitorFunc`を5秒後に再スケジュール
8. **5秒ごとに繰り返し**

### 期待される効果

修正後、以下の動作が期待されます：

- ✅ 監視チェックの関数が5秒ごとに正しく実行される
- ✅ `Log monitoring check triggered`のログが出力される
- ✅ ログファイルの変更検出が動作する
- ✅ NameLSAが更新される
- ✅ 統計情報が正しく抽出される
- ✅ Service Function情報がNameLSAに設定される

---

## 修正されたファイル

### `/home/katsutoshi/NLSR-fs/src/publisher/sidecar-stats-handler.cpp`

**修正箇所**:
- `startLogMonitoring()`関数（585-629行目）
- `updateNameLsaWithStats()`関数（486-545行目）
- `parseSidecarLog()`関数（194-324行目）

**変更内容**:
1. デバッグログの追加（各関数に詳細なログを追加）
2. `startLogMonitoring()`の実装を修正（`std::shared_ptr`を使用して再帰的なスケジューリングを実現）

---

## 検証方法

修正後、以下のログが出力されるはずです：

### 起動時
```
INFO: [nlsr.SidecarStatsHandler] startLogMonitoring called with interval: 5000ms, logPath: /var/log/sidecar/service.log
DEBUG: [nlsr.SidecarStatsHandler] Initial log file hash calculated: <hash>... (size: <size> bytes)
INFO: [nlsr.SidecarStatsHandler] Started log file monitoring with interval: 5000ms, logPath: /var/log/sidecar/service.log
```

### 監視チェック時（5秒ごと）
```
DEBUG: [nlsr.SidecarStatsHandler] Log monitoring check triggered
DEBUG: [nlsr.SidecarStatsHandler] Current log file hash: <hash>... (size: <size> bytes), last hash: <hash>...
DEBUG: [nlsr.SidecarStatsHandler] Log file unchanged, skipping update
```

### ログファイル変更時
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

---

## まとめ

### 実施した更新
1. ✅ **デバッグログの追加** - 監視機能の動作を確認するための詳細なログを追加
2. ✅ **監視機能の致命的なバグの修正** - `startLogMonitoring()`の実装を修正し、監視チェックが正しく実行されるように変更

### 技術的な改善
- `std::shared_ptr`を使用した再帰的なスケジューリングの実現
- 詳細なデバッグログによる問題の特定が容易に

### 期待される効果
- 監視機能が正しく動作する
- ログファイルの変更検出が動作する
- NameLSAが更新される
- Service Function情報が正しく反映される

---

## 参考資料

- `FIX_SUMMARY_COMPLETE.md`: 修正内容の完全なまとめ
- `FIX_APPLIED.md`: 修正実施内容
- `LOG_ANALYSIS_SUMMARY.md`: ログ分析結果の詳細
- `PROBLEMS_AND_SOLUTIONS.md`: 問題と解決策の詳細

