# ProcessingTimeの説明

## ProcessingTimeとは

ProcessingTimeは、**Service Function（`/relay`など）が1つのリクエストを処理するのにかかった時間**を表す数値です。単位は**秒**です。

## ProcessingTimeの計算方法

ProcessingTimeは、sidecarログファイル（`/var/log/sidecar/service.log`）から読み取られ、以下の優先順位で計算されます：

### 優先度1: service_callの時間差

```cpp
// service_call_in_time と service_call_out_time の差分
ProcessingTime = service_call_out_time - service_call_in_time
```

**意味:**
- `service_call_in_time`: Service Functionがリクエストを受け取った時刻
- `service_call_out_time`: Service Functionがリクエストを処理して応答を返した時刻
- **ProcessingTime**: Service Functionの処理時間（レイテンシ）

### 優先度2: sidecarの時間差

```cpp
// sidecar_in_time と sidecar_out_time の差分
ProcessingTime = sidecar_out_time - sidecar_in_time
```

**意味:**
- `sidecar_in_time`: Sidecarがリクエストを受け取った時刻
- `sidecar_out_time`: Sidecarがリクエストを処理して応答を返した時刻
- **ProcessingTime**: Sidecarの処理時間

### 優先度3: 直接値

```cpp
// processing_timeフィールドから直接取得
ProcessingTime = processing_time
```

## 実際の例

### sidecarログの内容

```json
{
    "service_call": {
        "call_name": "/relay",
        "in_time": "2025-12-10 17:05:37.565398",
        "out_time": "2025-12-10 17:05:37.566620",
        "port_num": 6363,
        "in_datasize": 24,
        "out_datasize": 24
    },
    "sidecar": {
        "in_time": "2025-12-10 17:05:42.048614",
        "out_time": "2025-12-10 17:05:42.051306",
        "name": "ndn-sidecar",
        "protocol": "ndn"
    }
}
```

### ProcessingTimeの計算

**優先度1（service_call）を使用:**
```
in_time:  2025-12-10 17:05:37.565398
out_time: 2025-12-10 17:05:37.566620
ProcessingTime = 0.001222秒 (1.222ミリ秒)
```

**優先度2（sidecar）を使用（service_callがない場合）:**
```
in_time:  2025-12-10 17:05:42.048614
out_time: 2025-12-10 17:05:42.051306
ProcessingTime = 0.002692秒 (2.692ミリ秒)
```

## ProcessingTimeの意味

### 1. Service Functionの処理性能

ProcessingTimeは、Service Functionが1つのリクエストを処理するのにかかった時間を表します。この値が小さいほど、Service Functionの処理が高速であることを意味します。

### 2. FunctionCostへの影響

ProcessingTimeは、FunctionCostの計算に使用されます：

```cpp
FunctionCost = processingTime * processingWeight + 
               load * loadWeight + 
               (usageCount / 100.0) * usageWeight
```

**現在の設定:**
- `processing-weight`: 10000
- `load-weight`: 0.0
- `usage-weight`: 0.0

**計算例:**
- ProcessingTime = 0.001222秒（1.222ミリ秒）
- FunctionCost = 0.001222 * 10000 = 12.22

### 3. ルーティング決定への影響

FunctionCostは、ルーティングコストに追加されます：

```cpp
adjustedCost = originalCost + nexthopCost + functionCost
```

**例:**
- 元のコスト: 25（node2へのリンクコスト）
- FunctionCost: 12.22
- 調整後のコスト: 25 + 12.22 = 37.22

## ProcessingTimeの特徴

### 1. リアルタイム性

ProcessingTimeは、sidecarログファイルから最新の値を読み取ります。ログファイルが更新されるたびに、ProcessingTimeも更新されます。

### 2. 1リクエストあたりの時間

ProcessingTimeは、**1つのリクエストを処理するのにかかった時間**を表します。単位時間当たりの処理時間ではありません。

### 3. 動的な値

ProcessingTimeは、Service Functionの負荷や処理内容によって変化します。負荷が高い場合、ProcessingTimeは大きくなります。

## ProcessingTimeの取得元

### 1. sidecarログファイル

ProcessingTimeは、sidecarログファイル（`/var/log/sidecar/service.log`）から読み取られます。このファイルは、sidecarコンテナによって更新されます。

### 2. ログファイルの監視

NLSR-fsは、sidecarログファイルを定期的に監視し、変更を検出すると、ProcessingTimeを更新します。

### 3. NameLSAへの反映

更新されたProcessingTimeは、NameLSAに含まれ、ネットワーク全体に広告されます。

## ProcessingTimeの使用例

### 1. FunctionCostの計算

```cpp
// name-prefix-table.cpp
ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
functionCost = sfInfo.processingTime * processingWeight;
```

### 2. ルーティングコストの調整

```cpp
// name-prefix-table.cpp
double adjustedCost = originalCost + nexthopCost + functionCost;
```

### 3. トラフィック分散

ProcessingTimeが大きいService Functionは、FunctionCostが大きくなり、ルーティングコストが高くなります。これにより、トラフィックが分散されます。

## まとめ

ProcessingTimeは、**Service Functionが1つのリクエストを処理するのにかかった時間（秒単位）**を表す数値です。この値は、sidecarログファイルから読み取られ、FunctionCostの計算に使用されます。ProcessingTimeが小さいほど、Service Functionの処理が高速であり、FunctionCostも小さくなります。

