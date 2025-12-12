2025-12-12

# トラフィック生成後のテスト結果

## テスト実施日時
2025-12-12 07:41-07:42頃

## 確認結果

### 1. Sidecarログ ✓
**node1のsidecarログ:**
- エントリ数: 2行
- 最新エントリ:
  ```json
  {"sfc_time": "2025-12-12 07:33:49.181443", "service_call": {"call_name": "/relay", "in_time": "2025-12-12 07:42:05.613725", "out_time": "2025-12-12 07:42:06.012281", ...}, ...}
  ```

**node2のsidecarログ:**
- エントリ数: 2行
- 最新エントリ:
  ```json
  {"sfc_time": "2025-12-12 07:33:49.267609", "service_call": {"call_name": "/relay", "in_time": "2025-12-12 07:41:59.418233", "out_time": "2025-12-12 07:41:59.843593", ...}, ...}
  ```

**分析:**
- ✓ sidecarログにエントリが記録されている
- ✓ `service_call`の`in_time`と`out_time`が記録されている
- ✓ 処理時間が計算可能（node1: 約0.4秒、node2: 約0.4秒）

### 2. updateNameLsaWithStatsの呼び出し ✓
**node1のログ:**
```
1765525275.317694 DEBUG: [nlsr.SidecarStatsHandler] updateNameLsaWithStats called
1765525275.317809 DEBUG: [nlsr.SidecarStatsHandler] Service Function info set in NameLSA
1765525275.317811 DEBUG: [nlsr.SidecarStatsHandler] Rebuilding and installing NameLSA...
```

**node2のログ:**
```
1765525320.641273 DEBUG: [nlsr.SidecarStatsHandler] updateNameLsaWithStats called
1765525320.641458 DEBUG: [nlsr.SidecarStatsHandler] Service Function info set in NameLSA
1765525320.641461 DEBUG: [nlsr.SidecarStatsHandler] Rebuilding and installing NameLSA...
```

**分析:**
- ✓ `updateNameLsaWithStats`が呼ばれている
- ✓ Service Function情報がNameLSAに設定されている
- ✓ NameLSAが再構築・インストールされている

### 3. NameLSAへのService Function情報の設定 ✓
**node1のwireEncodeログ:**
```
1765525275.320757 DEBUG: [nlsr.lsa.NameLsa] wireEncode: Encoding 1 Service Function info entries
1765525275.320758 DEBUG: [nlsr.lsa.NameLsa] wireEncode: Encoding Service Function: /relay, processingTime=0, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
```

**node2のwireEncodeログ:**
```
1765525320.646280 DEBUG: [nlsr.lsa.NameLsa] wireEncode: Encoding 1 Service Function info entries
1765525320.646283 DEBUG: [nlsr.lsa.NameLsa] wireEncode: Encoding Service Function: /relay, processingTime=0, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
```

**分析:**
- ✓ Service Function情報がNameLSAに含まれている
- ✓ `processingWeight=100`が正しく設定されている
- ✗ `processingTime=0`になっている（利用率が計算されていない）

### 4. node3でのNameLSA受信 ✓
**node3のwireDecodeログ:**
```
1765525240.630221 DEBUG: [nlsr.lsa.NameLsa] wireDecode: Decoded Service Function name: /relay
1765525240.630224 DEBUG: [nlsr.lsa.NameLsa] wireDecode: Stored Service Function info: /relay -> processingTime=0, load=0, usageCount=0, processingWeight=100, loadWeight=0, usageWeight=0
1765525240.630231 DEBUG: [nlsr.lsa.NameLsa] wireDecode: Completed decode. Service Function info entries: 1
```

**分析:**
- ✓ node3がService Function情報を受信している
- ✓ `processingWeight=100`が正しく伝播されている
- ✗ `processingTime=0`のまま（利用率が計算されていない）

### 5. adjustNexthopCostsの呼び出し ✓
**node3のログ:**
```
1765525275.322479 DEBUG: [nlsr.route.NamePrefixTable] adjustNexthopCosts called: nameToCheck=/relay
1765525275.322494 DEBUG: [nlsr.route.NamePrefixTable] Processing RoutingTablePoolEntry for destRouterName=/ndn/jp/%C1.Router/node2
1765525275.322629 DEBUG: [nlsr.route.NamePrefixTable] Adjusting cost for /relay to /ndn/jp/%C1.Router/node2: originalCost=25, nexthopCost=0, functionCost=0, adjustedCost=25
```

**分析:**
- ✓ `adjustNexthopCosts`が呼ばれている
- ✓ 各`RoutingTablePoolEntry`に対して個別に処理されている
- ✗ `functionCost=0`になっている（`processingTime=0`のため）

### 6. FIBエントリ ✗
**node3のFIBエントリ:**
```
/relay nexthops={faceid=262 (cost=25000), faceid=264 (cost=50000)}
```

**分析:**
- ✗ コストが25000と50000のまま（FunctionCostが適用されていない）
- 原因: `processingTime=0`のため、`functionCost=0`になっている

## 問題の分析

### 問題: processingTime=0（利用率が計算されていない）

**原因の可能性:**
1. `parseSidecarLogWithTimeWindow`が呼ばれているが、時間窓内のエントリが取得できていない
2. `calculateUtilization`が呼ばれていない、または正しく動作していない
3. 時間窓の設定が不適切（1秒の時間窓に対して、エントリが古すぎる可能性）

**確認すべき点:**
- `parseSidecarLogWithTimeWindow`で取得されたエントリ数
- `calculateUtilization`の呼び出しと計算結果
- 時間窓の開始時刻とエントリのタイムスタンプの関係

### 確認できたこと

1. ✓ **Service Function情報の伝播は正常**
   - sidecarログにエントリが記録されている
   - `updateNameLsaWithStats`が呼ばれている
   - NameLSAにService Function情報が含まれている
   - node3がService Function情報を受信している

2. ✓ **案1の実装は正常に動作**
   - `adjustNexthopCosts`が正しく呼ばれている
   - 各`RoutingTablePoolEntry`に対して個別に処理されている

3. ✗ **利用率計算が動作していない**
   - `processingTime=0`のまま
   - そのため、`functionCost=0`になっている

## 次のステップ

### 1. 利用率計算のデバッグ
- `parseSidecarLogWithTimeWindow`で取得されたエントリ数を確認
- `calculateUtilization`の呼び出しと計算結果を確認
- 時間窓の設定とエントリのタイムスタンプの関係を確認

### 2. デバッグログの追加
- `parseSidecarLogWithTimeWindow`で取得されたエントリ数をログ出力
- `calculateUtilization`の呼び出しと計算結果をログ出力
- 時間窓の開始時刻とエントリのタイムスタンプをログ出力

## 結論

Service Function情報の伝播は正常に動作していますが、利用率計算が動作していないため、`processingTime=0`のままです。そのため、FunctionCostが適用されていません。

利用率計算のデバッグが必要です。

