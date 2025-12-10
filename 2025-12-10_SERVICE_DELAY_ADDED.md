# /relayサービスにランダム待機を追加

## 変更の目的

見た目のファンクション利用率を上げるために、`/relay`サービスに0~0.5秒までランダムで待機するプログラムを追加しました。

## 変更内容

### ファイル: `service/src/service.py`

**変更前:**
```python
import json
import socket
import time
import os

# RELAY FUNCTION
def function(data: bytes) -> bytes:
    return data
```

**変更後:**
```python
import json
import socket
import time
import os
import random  # 追加

# RELAY FUNCTION
def function(data: bytes) -> bytes:
    # Add random delay (0~0.5 seconds) to increase apparent utilization
    delay = random.uniform(0.0, 0.5)
    time.sleep(delay)
    return data
```

## 効果

### 1. 処理時間の増加

**変更前:**
- 処理時間: ほぼ0秒（データをそのまま返すだけ）

**変更後:**
- 処理時間: 0~0.5秒のランダム値
- 平均処理時間: 約0.25秒

### 2. 利用率への影響

**例:**
- 時間窓: 1秒
- リクエスト数: 10回
- 各リクエストの処理時間: 0.1秒、0.2秒、0.3秒、...（ランダム）
- 総処理時間: 約2.5秒（10回 × 平均0.25秒）
- 利用率: 2.5 / 1.0 = 2.5 → 1.0（クランプ後）

**注意:** 利用率は1.0（100%）を超えないようにクランプされます。

### 3. FunctionCostへの影響

**変更前:**
- ProcessingTime（利用率）: ほぼ0.0
- FunctionCost: 0.0 * 10000 = 0

**変更後:**
- ProcessingTime（利用率）: 0.0 ~ 1.0（リクエスト数と処理時間に依存）
- FunctionCost: 0.0 ~ 10000（利用率に応じて変動）

## 期待される効果

1. **利用率の可視化**
   - ランダム待機により、処理時間が可視化される
   - 利用率の計算が正しく動作することを確認できる

2. **負荷分散のテスト**
   - 異なる処理時間を持つFunction間で負荷分散が機能するか確認できる
   - FunctionCostが正しく適用されるか確認できる

3. **トラフィック分散**
   - 利用率が高いFunctionには高いコストが設定される
   - トラフィックが分散される

## 次のステップ

1. **ビルドとデプロイ**
   - サービスイメージを再ビルド
   - Podを再デプロイ

2. **テスト**
   - ランダム待機が機能しているか確認
   - 利用率が正しく計算されているか確認
   - FunctionCostが正しく適用されているか確認

3. **調整**
   - 待機時間の範囲を調整（必要に応じて）
   - 時間窓のサイズを調整（必要に応じて）

