# デバッグログ分析レポート

## テスト実行日時
2025年12月10日

## テスト結果のまとめ

### 確認できた事実

1. **node1とnode2ではService Function情報が更新されている**
   ```
   node1: processingTime=0.00307512
   node2: processingTime=0.00307512
   ```

2. **しかし、node3のNameLSAではService Function情報マップのサイズが0**
   ```
   NameLSA Service Function info map size: 0
   m_serviceFunctionInfo map size: 0
   Service Function info NOT found for /relay in map
   ```

3. **FunctionCostが計算されていない**
   ```
   functionCost=0
   adjustedCostはリンクコストのみ（25, 50）
   ```

## 問題の原因

### 根本原因

**node1とnode2ではService Function情報がNameLSAに設定されているが、node3が受信したNameLSAにはService Function情報が含まれていない。**

### 考えられる原因

1. **NameLSAのwireEncode処理でService Function情報がエンコードされていない**
   - `wireEncode`メソッドでService Function情報が正しくエンコードされていない可能性

2. **NameLSAのwireDecode処理でService Function情報がデコードされていない**
   - `wireDecode`メソッドでService Function情報が正しくデコードされていない可能性

3. **NameLSAの伝播時にService Function情報が失われている**
   - NameLSAが他のノードに伝播する際に、Service Function情報が失われている可能性

## デバッグログの詳細

### node3のadjustNexthopCostsログ

```
1765382803.545427 DEBUG: [nlsr.route.NamePrefixTable] adjustNexthopCosts called: nameToCheck=/relay, destRouterName=/ndn/jp/%C1.Router/node1
1765382803.545429 DEBUG: [nlsr.route.NamePrefixTable] NameLSA found for /ndn/jp/%C1.Router/node1
1765382803.545430 DEBUG: [nlsr.route.NamePrefixTable] NameLSA Service Function info map size: 0
1765382803.545433 DEBUG: [nlsr.lsa.NameLsa] getServiceFunctionInfo called: name=/relay
1765382803.545434 DEBUG: [nlsr.lsa.NameLsa] m_serviceFunctionInfo map size: 0
1765382803.545435 DEBUG: [nlsr.lsa.NameLsa] Service Function info NOT found for /relay in map
1765382803.545436 DEBUG: [nlsr.route.NamePrefixTable] ServiceFunctionInfo for /relay: processingTime=0, load=0, usageCount=0
1765382803.545439 DEBUG: [nlsr.route.NamePrefixTable] No Service Function info in NameLSA for /relay (all values are zero)
```

### node3のgetServiceFunctionInfoログ

```
1765382803.546175 DEBUG: [nlsr.lsa.NameLsa] getServiceFunctionInfo called: name=/relay
1765382803.546176 DEBUG: [nlsr.lsa.NameLsa] m_serviceFunctionInfo map size: 0
1765382803.546178 DEBUG: [nlsr.lsa.NameLsa] Service Function info NOT found for /relay in map
```

### node1とnode2のService Function情報更新ログ

```
node1: 1765382996.229372  INFO: [nlsr.SidecarStatsHandler] Updated NameLSA with Service Function info: prefix=/relay, processingTime=0.00307512, load=0, usageCount=0
node2: 1765382996.594665  INFO: [nlsr.SidecarStatsHandler] Updated NameLSA with Service Function info: prefix=/relay, processingTime=0.00307512, load=0, usageCount=0
```

## 次のステップ

### 優先度1: NameLSAのwireEncode/wireDecode処理の確認

**確認事項:**
1. `wireEncode`メソッドでService Function情報が正しくエンコードされているか
2. `wireDecode`メソッドでService Function情報が正しくデコードされているか
3. NameLSAが他のノードに伝播する際に、Service Function情報が含まれているか

**実装方法:**
1. `wireEncode`メソッドにデバッグログを追加
2. `wireDecode`メソッドにデバッグログを追加
3. NameLSAの伝播時のログを確認

### 優先度2: NameLSAの伝播確認

**確認事項:**
1. node1とnode2が送信するNameLSAにService Function情報が含まれているか
2. node3が受信するNameLSAにService Function情報が含まれているか
3. NameLSAの伝播時にService Function情報が失われているか

**実装方法:**
1. node1とnode2のNameLSA送信時のログを確認
2. node3のNameLSA受信時のログを確認
3. `wireDecode`メソッドのデバッグログを確認

## 結論

**問題の原因は、NameLSAのwireEncode/wireDecode処理でService Function情報が失われていることです。**

次のステップとして、`wireEncode`と`wireDecode`メソッドにデバッグログを追加し、Service Function情報が正しくエンコード/デコードされているか確認する必要があります。

