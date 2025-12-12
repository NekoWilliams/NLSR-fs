2025-12-12

# 時間窓と古い情報の処理に関する修正

## 修正内容

### 1. 時間窓の設定方法の変更

**変更前:**
- 現在時刻を基準に、`現在時刻 - windowSeconds`から`現在時刻`までのエントリを取得
- エントリのタイムスタンプが現在時刻より前にある場合、時間窓外になってしまう

**変更後:**
- エントリのタイムスタンプを基準に、最新のエントリのタイムスタンプを`now`として設定
- `now - windowSeconds`から`now`までのエントリを取得
- これにより、エントリのタイムスタンプとファンクション利用率計算に使われる時刻が同じになる

**実装:**
- `parseSidecarLogWithTimeWindow`を2パスで実装：
  1. 最初のパス：全てのエントリを読み込み、最新のエントリのタイムスタンプを見つける
  2. 2番目のパス：最新のエントリのタイムスタンプを基準に時間窓内のエントリを取得

### 2. 時間窓の長さを60秒に変更

**設定ファイル:**
- `nlsr-node1.conf`: `utilization-window-seconds  60`
- `nlsr-node2.conf`: `utilization-window-seconds  60`

### 3. 古い情報の処理

**問題:**
- ファンクション利用状況の更新が一定時間以上なかった場合、古い情報が使われ続ける可能性がある

**解決策:**
- `convertStatsToServiceFunctionInfo`で、最新のエントリのタイムスタンプを`lastUpdateTime`に設定
- 最新のエントリが`windowSeconds * 2`より古い場合、`processingTime=0`を設定
- `adjustNexthopCosts`で、`lastUpdateTime`から`windowSeconds * 2`以上経過している場合、`functionCost=0`を設定

**実装:**
1. **`convertStatsToServiceFunctionInfo`**:
   - 最新のエントリのタイムスタンプを`lastUpdateTime`に設定
   - 最新のエントリが`windowSeconds * 2`より古い場合、`processingTime=0`を設定

2. **`adjustNexthopCosts`**:
   - `lastUpdateTime`から`windowSeconds * 2`以上経過している場合、`functionCost=0`を設定
   - これにより、古い情報が使われ続けることを防ぐ

## 期待される効果

### 1. エントリのタイムスタンプ基準の時間窓
- エントリのタイムスタンプとファンクション利用率計算に使われる時刻が同じになる
- 現在時刻（もしくは現在時刻に最も近い時刻）の実質的なファンクション利用率をルーティングに使用できる

### 2. 60秒の時間窓
- より長い時間窓により、より多くのエントリが含まれる
- より正確な利用率計算が可能

### 3. 古い情報の処理
- 一定時間以上更新がない場合、FunctionCostが0になる
- 古い情報が使われ続けることを防ぐ

## 実装の詳細

### parseSidecarLogWithTimeWindowの変更

**変更前:**
```cpp
// Calculate time window start (current time - windowSeconds)
auto now = std::chrono::system_clock::now();
auto windowStart = now - std::chrono::seconds(windowSeconds);

// Filter entries by time window
if (entryTime >= windowStart && entryTime <= now) {
  logEntries.push_back(entry);
}
```

**変更後:**
```cpp
// First pass: Read all entries and find the latest timestamp
std::vector<std::pair<std::map<std::string, std::string>, std::chrono::system_clock::time_point>> allEntries;
auto latestTimestamp = std::chrono::system_clock::time_point::min();

// Read all lines and parse timestamps
while (std::getline(logFile, line)) {
  // Parse entry and timestamp
  auto entryTime = parseTimestampToTimePoint(timeStr);
  allEntries.push_back({entry, entryTime});
  if (entryTime > latestTimestamp) {
    latestTimestamp = entryTime;
  }
}

// Second pass: Filter entries based on time window relative to latest timestamp
auto windowStart = latestTimestamp - std::chrono::seconds(windowSeconds);
for (const auto& [entry, entryTime] : allEntries) {
  if (entryTime >= windowStart && entryTime <= latestTimestamp) {
    logEntries.push_back(entry);
  }
}
```

### convertStatsToServiceFunctionInfoの変更

**追加:**
```cpp
// Find the latest entry timestamp to set lastUpdateTime
auto latestTimestamp = std::chrono::system_clock::time_point::min();
for (const auto& entry : entries) {
  // Parse timestamp and find latest
  if (entryTime > latestTimestamp) {
    latestTimestamp = entryTime;
  }
}

// Set lastUpdateTime to the latest entry timestamp
info.lastUpdateTime = latestTimestamp;

// Check if the latest entry is too old
auto now = std::chrono::system_clock::now();
auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(now - latestTimestamp).count();
uint32_t staleThreshold = windowSeconds * 2;

if (timeSinceLastUpdate > static_cast<int64_t>(staleThreshold)) {
  info.processingTime = 0.0;
  return info;
}
```

### adjustNexthopCostsの変更

**追加:**
```cpp
// Check if Service Function info is stale
bool isStale = false;
if (sfInfo.lastUpdateTime != ndn::time::system_clock::time_point::min()) {
  auto now = ndn::time::system_clock::now();
  auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(now - sfInfo.lastUpdateTime).count();
  uint32_t staleThreshold = m_confParam.getUtilizationWindowSeconds() * 2;
  
  if (timeSinceLastUpdate > static_cast<int64_t>(staleThreshold)) {
    isStale = true;
  }
}

// Calculate function cost if not stale
if (!isStale && (sfInfo.processingTime > 0.0 || ...)) {
  // Calculate functionCost
} else if (isStale) {
  functionCost = 0.0;
}
```

## 次のステップ

1. ビルドとデプロイ
2. テスト実行
3. ログ確認：
   - エントリのタイムスタンプ基準で時間窓が設定されているか
   - 60秒の時間窓でエントリが取得できているか
   - 古い情報が正しく処理されているか

