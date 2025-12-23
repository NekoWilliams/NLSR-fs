2025-12-12

# relayPod-ICSM 分析レポート

## ① ProcessingTimeの計算ロジック

### 計算フロー

**1. ログエントリの収集**

`parseSidecarLogWithTimeWindow()`関数（`sidecar-stats-handler.cpp`）:
- サイドカーログファイル（`/var/log/sidecar/service.log`）を読み込み
- 最新のエントリのタイムスタンプを基準に、時間窓（`utilization-window-seconds`）内のエントリを抽出
- 各エントリから`service_call_in_time`と`service_call_out_time`（または`sidecar_in_time`と`sidecar_out_time`）を抽出

**2. 各エントリの処理時間を計算**

`calculateUtilization()`関数（`sidecar-stats-handler.cpp:604-703`）:
```cpp
for (const auto& entry : entries) {
  double processingTime = 0.0;
  
  // Priority: 1) service_call times, 2) sidecar times
  if (entry.find("service_call_in_time") != entry.end() && 
      entry.find("service_call_out_time") != entry.end()) {
    double inTime = parseTimestamp(entry.at("service_call_in_time"));
    double outTime = parseTimestamp(entry.at("service_call_out_time"));
    if (inTime > 0.0 && outTime > 0.0) {
      processingTime = outTime - inTime;  // 各エントリの処理時間
    }
  } else if (entry.find("sidecar_in_time") != entry.end() && 
             entry.find("sidecar_out_time") != entry.end()) {
    double inTime = parseTimestamp(entry.at("sidecar_in_time"));
    double outTime = parseTimestamp(entry.at("sidecar_out_time"));
    if (inTime > 0.0 && outTime > 0.0) {
      processingTime = outTime - inTime;  // 各エントリの処理時間
    }
  }
  
  if (processingTime > 0.0) {
    totalProcessingTime += processingTime;  // 合計処理時間に加算
    validEntries++;
  }
}
```

**3. Utilization（利用率）を計算**

```cpp
// Calculate utilization: total processing time / time window
double utilization = totalProcessingTime / static_cast<double>(windowSeconds);

// Clamp to [0.0, 1.0] (utilization cannot exceed 100%)
if (utilization > 1.0) {
  utilization = 1.0;
}
```

**4. ProcessingTimeとして保存**

`convertStatsToServiceFunctionInfo()`関数（`sidecar-stats-handler.cpp:705-829`）:
```cpp
info.processingTime = calculateUtilization(entries, windowSeconds);
```

**注意**: `info.processingTime`は実際には**利用率（utilization）**を表す（0.0 ~ 1.0）

### 計算式

```
ProcessingTime (実際は利用率) = totalProcessingTime / windowSeconds

where:
  totalProcessingTime = Σ(各エントリの処理時間)
  各エントリの処理時間 = service_call_out_time - service_call_in_time
                        (または sidecar_out_time - sidecar_in_time)
  windowSeconds = utilization-window-seconds (設定ファイルから取得、デフォルト: 60秒)
```

### ログエントリの形式

サイドカーログ（`sidecar.py:171-187`）:
```json
{
  "sfc_time": "2025-12-23 13:34:03.261357",
  "service_call": {
    "call_name": "/relay",
    "in_time": "2025-12-23 14:18:31.118615",
    "out_time": "2025-12-23 14:18:31.428371",
    "port_num": 6363,
    "in_datasize": 13,
    "out_datasize": 13
  },
  "sidecar": {
    "in_time": "2025-12-23 14:18:31.443559",
    "out_time": "2025-12-23 14:18:31.446665",
    "name": "ndn-sidecar",
    "protocol": "ndn"
  },
  "host_name": "192.168.49.2"
}
```

**タイムスタンプの優先順位**:
1. `service_call_in_time` / `service_call_out_time`（優先）
2. `sidecar_in_time` / `sidecar_out_time`（フォールバック）

## ② /relayがパケットにどのような処理を行い、何を返すか

### パケット処理フロー

**1. Interestパケット受信**

`on_interest()`関数（`sidecar.py:77-191`）:
- Interestパケットを受信
- 名前をパース（セグメント番号、バージョン番号を抽出）
- `LOG_SIDECAR_IN_TIME`を記録

**2. キャッシュチェック**

```python
holding_content = {'name': '', 'content': b'', 'time': 0.0}
if not q.empty():
    holding_content = q.get()
if Name.to_str(name_nosegver) == holding_content['name'] and time.time() - holding_content['time'] <= FRESHNESS_PERIOD:
    processed_content = holding_content['content']  # キャッシュから返送
    q.put(holding_content)
```

- キュー（`Queue(maxsize=1)`）にキャッシュされたコンテンツがあるか確認
- 名前が一致し、`FRESHNESS_PERIOD`（5秒）以内であれば、キャッシュから返送

**3. コンテンツ取得（キャッシュミスの場合）**

```python
# ndncatchunksでコンテンツを取得
with open(os.path.join(TMP_PATH, name_message), 'wb') as f:
    sp.run(['ndncatchunks', Name.to_str(trimmed_name), '-qf'], stdout=f)
with open(os.path.join(TMP_PATH, name_message), 'r') as f:
    os.environ['IN_DATASIZE'] = str(len(f.read()))
```

- `ndncatchunks`を使用して、元のコンテンツ名（`trimmed_name`）からコンテンツを取得
- 取得したコンテンツを一時ファイルに保存
- 入力データサイズを記録

**4. サービス関数呼び出し**

```python
# service function
t0_service = time.time()
os.environ['LOG_SERVICE_CALL_IN_TIME'] = str(datetime.datetime.now())
processed_content = call_service(name_message)  # サービス関数を呼び出し
dt_service = t0_service - time.time()  # 注意: この計算は負の値になる（バグ？）
os.environ['LOG_SERVICE_CALL_OUT_TIME'] = str(datetime.datetime.now())
```

- `call_service()`関数を呼び出し（`send_msg.py`からインポート）
  - ファイル名（`name_message`）をTCPソケット経由でサービスに送信
  - サービスから返されたファイル名のファイルを読み込む
- サービス関数の処理時間を計測（ただし、`dt_service`の計算にバグがある可能性）
- `LOG_SERVICE_CALL_IN_TIME`と`LOG_SERVICE_CALL_OUT_TIME`を記録

**5. キャッシュへの保存**

```python
holding_content = {
    'name': Name.to_str(name_nosegver),
    'content': processed_content,
    'time': time.time()
}
q.put(holding_content)
```

- 処理済みコンテンツをキューに保存（次回のリクエストで使用）

**6. Dataパケットの返送**

```python
seg_cnt = (len(processed_content) + SEGMENT_SIZE - 1) // SEGMENT_SIZE
timestamp = int(holding_content['time'] * 1000)

if '/32=metadata' in Name.to_str(name) and seg_no < 1:
    # version discovery process
    metaname = name_nosegver + Name.normalize('/32=metadata') + ...
    app.put_data(metaname, Name.to_bytes(metadata), freshness_period=10)
elif seg_no < seg_cnt:
    send_name = name_nosegver + [Component.from_version(timestamp)] + [Component.from_segment(seg_no)]
    app.put_data(send_name,
                 processed_content[seg_no * SEGMENT_SIZE:(seg_no+1)*SEGMENT_SIZE],
                 freshness_period=100,
                 final_block_id=Component.from_segment(seg_cnt-1))
```

- コンテンツをセグメントに分割（`SEGMENT_SIZE = 8000`バイト）
- 各セグメントをDataパケットとして返送
- バージョン番号とセグメント番号を含む名前で返送

**7. ログ記録**

```python
if seg_no == seg_cnt - 1:  # 最後のセグメントの場合
    os.environ['LOG_SIDECAR_OUT_TIME'] = str(datetime.datetime.now())
    log_json = json.dumps({...})
    with open(LOG_PATH, 'a') as f:
        f.write(log_json)
        f.write('\n')
```

- 最後のセグメントが送信された時点でログを記録
- JSON形式でサイドカーログファイルに追記

### サービス関数の処理内容

`send_msg.py`の`call_service()`関数（`send_msg.py:13-45`）:

```python
def call_service(send_message: str) -> bytes:
    # TCPソケットでサービスに接続
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((SERVICE_IP, PORT))
        
        # ファイル名をJSON形式で送信
        send_data = json.dumps({"filename": send_message}).encode()
        s.sendall(send_data)
        s.shutdown(1)
        
        # サービスから返されたJSONを受信
        received_message = b''
        while True:
            chunk = s.recv(BUFFER_SIZE)
            if not chunk:
                break
            received_message += chunk
        received_json = json.loads(received_message.decode())
    
    # サービスから返されたファイル名のファイルを読み込む
    data_path = os.path.join(DATA_VOLUME_PATH, received_json['filename'])
    with open(data_path, 'rb') as f:
        processed_data = f.read()
    
    return processed_data
```

**処理内容**:
1. TCPソケットでサービス（`MY_POD_IP:TCP_MESSAGE_PORT`）に接続
2. ファイル名（`send_message`）をJSON形式（`{"filename": ...}`）で送信
3. サービスから返されたJSON（`{"filename": ...}`）を受信
4. 返されたファイル名のファイルを`SHARE_PATH`から読み込む
5. 読み込んだデータ（バイナリ）を返す

**注意**: この実装では、サービス関数は実際にはファイル名を受け取り、別のファイル名を返すリレー（転送）サービスとして機能している

### 返されるデータ

**返されるDataパケット**:
- **名前**: `/relay/<original_content_name>/v=<timestamp>/seg=<segment_number>`
- **コンテンツ**: サービス関数で処理されたコンテンツ（セグメント化）
- **メタデータ**: 
  - `freshness_period`: 100秒
  - `final_block_id`: 最後のセグメント番号

**処理の種類**:
- **リレー（転送）**: 元のコンテンツ名を受け取り、サービス関数（TCP経由）に処理を依頼
- **サービス関数**: TCPソケット経由でファイル名を受け取り、処理済みファイル名を返す
- **データ返送**: 処理済みファイルを読み込んで、セグメント化してDataパケットとして返送

**実際の処理フロー**:
1. `/relay/content1.txt`というInterestを受信
2. `ndncatchunks`で`/content1.txt`を取得（元のコンテンツ）
3. ファイル名（`content1.txt`）をサービス関数に送信（TCP経由）
4. サービス関数が処理済みファイル名を返す
5. 処理済みファイルを読み込む
6. 処理済みデータをセグメント化してDataパケットとして返送

## まとめ

### ProcessingTimeの計算

1. **ログエントリの収集**: 時間窓内のエントリを抽出
2. **各エントリの処理時間**: `out_time - in_time`を計算
3. **合計処理時間**: すべてのエントリの処理時間を合計
4. **利用率の計算**: `totalProcessingTime / windowSeconds`（0.0 ~ 1.0）
5. **保存**: `ServiceFunctionInfo.processingTime`として保存（実際は利用率）

### /relayのパケット処理

1. **Interest受信**: Interestパケットを受信
2. **キャッシュチェック**: 5秒以内のキャッシュがあれば返送
3. **コンテンツ取得**: `ndncatchunks`で元のコンテンツを取得
4. **サービス関数呼び出し**: `call_service()`で処理
5. **キャッシュ保存**: 処理済みコンテンツをキャッシュ
6. **Data返送**: セグメント化してDataパケットとして返送
7. **ログ記録**: 処理時間とデータサイズをログに記録

