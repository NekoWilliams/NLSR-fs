2025-12-12

# 優先度1: 即座に対応すべき項目の実装

## 実装内容

### 1. スタイルデータの閾値を調整 ✅

**変更内容**:
- `src/route/name-prefix-table.cpp`の257行目を修正
- `staleThreshold`を`utilization-window-seconds * 2`から`utilization-window-seconds * 3`に変更

**変更前**:
```cpp
uint32_t staleThreshold = m_confParam.getUtilizationWindowSeconds() * 2;  // 2x window duration
```

**変更後**:
```cpp
uint32_t staleThreshold = m_confParam.getUtilizationWindowSeconds() * 3;  // 3x window duration (180 seconds if window is 60s)
```

**効果**:
- `utilization-window-seconds`が60秒の場合、閾値は180秒（3分）になる
- ログファイルの更新頻度が低い場合でも、FunctionCostが計算される可能性が高くなる

### 2. ndncatchunksのタイムアウト問題の解決

#### 2.1 タイムアウト時間を延長（1秒 → 5秒以上）

**問題点**:
- テストスクリプトで`timeout 1`を使用しているため、ndncatchunksが正常にデータを取得できない

**対応策**:
- テストスクリプトで`timeout 5`または`ndncatchunks -t 5`を使用する
- または、ndncatchunksのタイムアウトオプションを直接指定する

**推奨される修正**:
```bash
# 修正前
timeout 1 ndncatchunks /relay/content1.txt

# 修正後（オプション1: Linuxのtimeoutコマンドを使用）
timeout 5 ndncatchunks /relay/content1.txt

# 修正後（オプション2: ndncatchunksのタイムアウトオプションを使用）
ndncatchunks -t 5 /relay/content1.txt
```

#### 2.2 データ転送の成功を確認

**確認方法**:
1. ndncatchunksの終了コードを確認
2. 取得したデータのサイズを確認
3. エラーログを確認

**実装例**:
```bash
# データ転送の成功を確認するスクリプト
if ndncatchunks -t 5 /relay/content1.txt > /tmp/content1.txt 2>&1; then
    if [ -s /tmp/content1.txt ]; then
        echo "データ転送成功: $(wc -c < /tmp/content1.txt) bytes"
    else
        echo "データ転送失敗: ファイルが空"
    fi
else
    echo "データ転送失敗: 終了コード $?"
fi
```

### 3. サイドカーログの更新メカニズムの確認

#### 3.1 ログファイルのパスを確認

**現在の設定**:
- `sidecar-stats-handler.hpp`: デフォルトパスは`/var/log/sidecar/service.log`
- `sidecar.py`: 環境変数`SIDECAR_LOG_PATH`またはデフォルトで`/var/log/sidecar/service.log`

**確認方法**:
```bash
# ログファイルのパスを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "echo \$SIDECAR_LOG_PATH"
kubectl exec -n default deployment/ndn-node1 -- bash -c "ls -la /var/log/sidecar/"

# ログファイルの存在確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "test -f /var/log/sidecar/service.log && echo '存在' || echo '不存在'"
```

#### 3.2 サイドカーのログ書き込みを確認

**sidecar.pyのログ書き込み処理** (188-190行目):
```python
with open(LOG_PATH, 'a') as f:
    f.write(log_json)
    f.write('\n')
```

**確認方法**:
1. ログファイルの変更時刻を確認
2. ログファイルのサイズを確認
3. ログファイルの内容を確認

**実装例**:
```bash
# ログファイルの変更を監視
kubectl exec -n default deployment/ndn-node1 -- bash -c "
    while true; do
        stat /var/log/sidecar/service.log 2>&1 | grep Modify
        sleep 1
    done
"

# ログファイルの内容を確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "tail -f /var/log/sidecar/service.log"

# ログファイルのサイズを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "wc -l /var/log/sidecar/service.log"
```

#### 3.3 sidecar-stats-handlerのログ監視メカニズム

**ログ監視のフロー**:
1. `startLogMonitoring()`が呼び出される
2. 5秒間隔でログファイルをチェック
3. ハッシュを計算して、前回のハッシュと比較
4. ハッシュが変わっている場合、`updateNameLsaWithStats()`を呼び出し
5. ServiceFunctionInfoを更新してNameLSAを再構築

**確認方法**:
```bash
# sidecar-stats-handlerのログを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "cat /proc/\$(pgrep nlsr)/fd/1 | grep -E 'Log file|hash|updateNameLsaWithStats'"
```

## 実装手順

### ステップ1: スタイルデータの閾値を調整 ✅

1. `src/route/name-prefix-table.cpp`を修正（完了）
2. コンパイルしてデプロイ

### ステップ2: ndncatchunksのタイムアウト問題を解決

1. テストスクリプトを修正
   - `timeout 1`を`timeout 5`に変更
   - または、`ndncatchunks -t 5`を使用

2. データ転送の成功を確認するスクリプトを作成

### ステップ3: サイドカーログの更新メカニズムを確認

1. ログファイルのパスを確認
2. サイドカーのログ書き込みを確認
3. sidecar-stats-handlerのログ監視を確認

## テスト方法

### テスト1: スタイルデータの閾値調整の確認

```bash
# ログから閾値を確認
kubectl exec -n default deployment/ndn-node3 -- bash -c "cat /proc/\$(pgrep nlsr)/fd/1 | grep -E 'threshold|stale' | tail -10"
```

### テスト2: ndncatchunksのタイムアウト問題の確認

```bash
# タイムアウトを5秒に設定してテスト
kubectl exec -n default deployment/ndn-node3 -- bash -c "timeout 5 ndncatchunks /relay/content1_1.txt 2>&1"

# データ転送の成功を確認
kubectl exec -n default deployment/ndn-node3 -- bash -c "timeout 5 ndncatchunks /relay/content1_1.txt > /tmp/test.txt 2>&1 && echo '成功: $(wc -c < /tmp/test.txt) bytes' || echo '失敗: 終了コード $?'"
```

### テスト3: サイドカーログの更新メカニズムの確認

```bash
# ログファイルのパスを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "ls -la /var/log/sidecar/"

# ログファイルの変更を監視
kubectl exec -n default deployment/ndn-node1 -- bash -c "stat /var/log/sidecar/service.log"

# サイドカーのログ書き込みを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "tail -5 /var/log/sidecar/service.log"

# sidecar-stats-handlerのログ監視を確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "cat /proc/\$(pgrep nlsr)/fd/1 | grep -E 'Log file changed|hash' | tail -10"
```

## 次のステップ

1. **コンパイルとデプロイ**: スタイルデータの閾値調整を反映
2. **テストスクリプトの修正**: ndncatchunksのタイムアウトを延長
3. **検証**: サイドカーログの更新メカニズムを確認
4. **再テスト**: 修正後の動作を確認

