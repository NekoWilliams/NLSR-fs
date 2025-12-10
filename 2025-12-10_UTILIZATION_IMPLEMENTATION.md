# 時間ベースのファンクション利用率によるFunctionCost計算の実装

## 実装概要

ProcessingTimeを、1リクエストあたりの処理時間から、**時間ベースのファンクション利用率（0.0 ~ 1.0）**に変更しました。

## 実装内容

### 1. ConfParameterクラスの拡張

**ファイル:** `src/conf-parameter.hpp`

**追加内容:**
- `m_utilizationWindowSeconds`メンバ変数（デフォルト: 1秒）
- `setUtilizationWindowSeconds(uint32_t windowSeconds)`メソッド
- `getUtilizationWindowSeconds()`メソッド

### 2. 設定ファイル処理の拡張

**ファイル:** `src/conf-file-processor.cpp`

**追加内容:**
- `utilization-window-seconds`パラメータの読み込み
- 値が0の場合は警告を出してデフォルト値（1秒）を使用

### 3. SidecarStatsHandlerの拡張

**ファイル:** `src/publisher/sidecar-stats-handler.hpp`, `src/publisher/sidecar-stats-handler.cpp`

**追加メソッド:**
- `parseSidecarLogWithTimeWindow(uint32_t windowSeconds)`: 時間窓内のログエントリを取得
- `calculateUtilization(...)`: 利用率を計算

**変更メソッド:**
- `convertStatsToServiceFunctionInfo()`: シグネチャを変更し、利用率を計算するように変更

### 4. 設定ファイルの更新

**ファイル:** `nlsr-node1.conf`, `nlsr-node2.conf`

**追加内容:**
```conf
utilization-window-seconds  1  ; default: 1 second
```

## 利用率の計算方法

### 計算式

```
利用率 = 時間窓内の総処理時間 / 時間窓の長さ
```

**例:**
- 時間窓: 1秒
- 時間窓内の総処理時間: 0.8秒
- 利用率 = 0.8 / 1.0 = 0.8 (80%)

### 実装の詳細

1. **時間窓の設定**
   - 設定ファイルから`utilization-window-seconds`を読み込む
   - デフォルト: 1秒

2. **ログエントリの収集**
   - `parseSidecarLogWithTimeWindow()`で時間窓内のエントリを取得
   - 各エントリの`service_call_in_time`または`sidecar_in_time`を確認
   - 時間窓内のエントリのみを収集

3. **処理時間の計算**
   - 各エントリの`service_call_out_time - service_call_in_time`を計算
   - または`sidecar_out_time - sidecar_in_time`を計算
   - すべてのエントリの処理時間を合計

4. **利用率の計算**
   - 総処理時間を時間窓の長さで割る
   - 結果を[0.0, 1.0]の範囲にクランプ

## FunctionCostへの影響

### 変更前

```cpp
FunctionCost = processingTime * processingWeight;
// 例: 0.001222秒 * 10000 = 12.22
```

### 変更後

```cpp
FunctionCost = utilization * processingWeight;
// 例: 0.8（利用率80%） * 10000 = 8000
```

**影響:**
- 利用率が高いFunctionには高いコストが設定される
- トラフィックが分散される

## 期待される効果

1. **負荷分散の改善**
   - 利用率が高いFunctionには高いコストが設定される
   - トラフィックが分散される

2. **リアルタイム性**
   - 時間窓内の最新のデータを使用
   - 負荷の変動を反映

3. **リクエスト数の考慮**
   - リクエスト数が多い場合、利用率も高くなる
   - 負荷の高さを正確に反映

## 次のステップ

1. **ビルドとデプロイ**
   - コードをビルド
   - デプロイして動作確認

2. **テスト**
   - 利用率が正しく計算されているか確認
   - FunctionCostが正しく適用されているか確認
   - トラフィック分散が機能しているか確認

3. **調整**
   - 時間窓のサイズを調整（必要に応じて）
   - processing-weightを調整（必要に応じて）

