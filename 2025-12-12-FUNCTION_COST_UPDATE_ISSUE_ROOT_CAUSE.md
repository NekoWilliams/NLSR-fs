2025-12-12

# FunctionCost更新が起こらない根本原因

## 問題の概要

5分間の負荷テストで、FunctionCostが一定値を維持し、更新されなかった。

## 根本原因

### 原因1: ServiceFunctionInfoがスタイル（古い）と判定されている

**ログからの証拠:**
```
DEBUG: [nlsr.route.NamePrefixTable] Service Function info is stale (lastUpdateTime: 1765983439, timeSinceLastUpdate: 317s, threshold: 120s)
DEBUG: [nlsr.route.NamePrefixTable] Service Function info is stale for /relay (destRouterName=/ndn/jp/%C1.Router/node2), functionCost=0
```

**分析:**
- `lastUpdateTime`: 1765983439（タイムスタンプ）
- `timeSinceLastUpdate`: 317秒（約5.3分）
- `threshold`: 120秒（2分）
- **判定**: 317秒 > 120秒 → スタイル（古い）と判定される
- **結果**: `functionCost=0`が設定される

### 原因2: ログファイルが更新されていない

**ログからの証拠:**
```
DEBUG: [nlsr.SidecarStatsHandler] Current log file hash: 7154305393529903... (size: 1173 bytes), last hash: 7154305393529903...
DEBUG: [nlsr.SidecarStatsHandler] Log file unchanged, skipping update
```

**分析:**
- ログファイルのハッシュが変わっていない
- そのため、`updateNameLsaWithStats()`が呼び出されない
- ServiceFunctionInfoが更新されない

### 原因3: トラフィックが少ない

**事実:**
- 5分間で3リクエストのみ
- 最新エントリ: 2025-12-17 14:57:17.949908
- 現在時刻: 2025-12-17 15:03:00頃（推定）
- **経過時間**: 約6分

**影響:**
- トラフィックが少ないため、ログファイルが更新されない
- ログファイルが更新されないため、ハッシュが変わらない
- ハッシュが変わらないため、更新がスキップされる

## 処理フローの問題点

### 1. ログファイル監視のフロー

```
1. 5秒間隔でログファイルをチェック
2. ハッシュを計算
3. 前回のハッシュと比較
4. ハッシュが変わっていない → 更新をスキップ
5. ハッシュが変わっている → updateNameLsaWithStats()を呼び出し
```

**問題**: ハッシュが変わらない限り、更新が行われない

### 2. ServiceFunctionInfoの更新フロー

```
1. updateNameLsaWithStats()が呼び出される
2. parseSidecarLogWithTimeWindow()でログを解析
3. convertStatsToServiceFunctionInfo()でServiceFunctionInfoを計算
4. NameLSAを更新
5. LSDBにインストール
```

**問題**: ログファイルが更新されないため、このフローが実行されない

### 3. FunctionCostの計算フロー

```
1. adjustNexthopCosts()が呼び出される
2. ServiceFunctionInfoを取得
3. lastUpdateTimeをチェック
4. timeSinceLastUpdate > threshold (120秒) → functionCost=0
5. timeSinceLastUpdate <= threshold → FunctionCostを計算
```

**問題**: ServiceFunctionInfoが古いため、常に`functionCost=0`になる

## 解決策

### 解決策1: スタイルデータの閾値を調整

**現在の設定:**
- `threshold`: 120秒（2分）
- `utilization-window-seconds`: 60秒

**推奨設定:**
- `threshold`: `utilization-window-seconds * 3` = 180秒（3分）
- または、`utilization-window-seconds * 4` = 240秒（4分）

**理由**: ログファイルの更新頻度が低い場合でも、FunctionCostが計算される

### 解決策2: 強制的な更新メカニズム

**実装:**
- 一定時間ごと（例: 60秒）に強制的にログファイルを再読み込みする
- ハッシュが変わっていなくても、一定時間経過後に更新する

**理由**: ログファイルが更新されなくても、定期的にServiceFunctionInfoを更新する

### 解決策3: トラフィック生成の強化

**実装:**
- より多くのトラフィックを生成する
- リクエスト間隔を短くする（0.1秒間隔など）
- 複数のプロセスから同時にリクエストを送信する

**理由**: ログファイルを定期的に更新し、ハッシュを変更する

### 解決策4: ログファイルの変更検出の改善

**実装:**
- ファイルサイズの変更も検出する
- ファイルのmtime（変更時刻）をチェックする
- ハッシュだけでなく、複数の指標を使用する

**理由**: ログファイルの変更をより確実に検出する

## 推奨される対応

### 即座に対応すべき項目

1. **スタイルデータの閾値を調整**: `threshold`を`utilization-window-seconds * 3`に変更
2. **強制的な更新メカニズムの実装**: 60秒ごとに強制的に更新する

### 長期的な対応

1. **トラフィック生成の強化**: より多くのトラフィックを生成する
2. **ログファイルの変更検出の改善**: 複数の指標を使用する

## 次のステップ

1. `name-prefix-table.cpp`の`threshold`を調整する
2. `sidecar-stats-handler.cpp`に強制的な更新メカニズムを実装する
3. トラフィック生成スクリプトを改善する

