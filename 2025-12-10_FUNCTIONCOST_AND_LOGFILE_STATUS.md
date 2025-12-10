# FunctionCost関連の変更内容とログファイル関連のトラブルシューティング - 現状と次のアクション

## 1. FunctionCost関連の変更内容

### 1.1 実装の概要

FunctionCostは、Service Function（`/relay`など）の統計情報（ProcessingTime、Load、UsageCount）に基づいて計算されるコストで、ルーティング決定に影響を与えます。

### 1.2 主要な変更ファイル

#### 1.2.1 `src/route/name-prefix-table.cpp`

**変更内容:**
- `adjustNexthopCosts`関数を修正し、`/relay`プレフィックスのコスト計算時にFunctionCostを適用
- NameLSAから直接Service Function情報を取得（`m_confParam.isServiceFunctionPrefix()`への依存を削除）

**実装のポイント:**
```cpp
NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, 
                                    const ndn::Name& nameToCheck, 
                                    const ndn::Name& destRouterName)
{
  // NameLSAに含まれるService Function情報を信頼できるソースとして使用
  auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
  if (nameLsa) {
    ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
    
    // Service Function情報が存在する場合、FunctionCostを計算
    if (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0) {
      functionCost = sfInfo.processingTime * processingWeight +
                     sfInfo.load * loadWeight +
                     (sfInfo.usageCount / 100.0) * usageWeight;
    }
  }
  
  // FunctionCostをコストに追加
  double adjustedCost = originalCost + nexthopCost + functionCost;
}
```

**変更の理由:**
- 設定ファイルの`function-prefix`リストに依存しない設計
- Service Function情報を持つノードのみが情報を提供する設計
- ルーティングノードはNameLSAから情報を取得する設計

#### 1.2.2 `src/lsa/name-lsa.cpp`

**変更内容:**
- `wireEncode`と`wireDecode`にデバッグログを追加
- Service Function情報のエンコード/デコード処理を改善

**実装のポイント:**
- Service Function情報をTLV形式でエンコード/デコード
- ProcessingTime、Load、UsageCountを個別にエンコード/デコード
- デバッグログでエンコード/デコードの過程を追跡可能

#### 1.2.3 `src/conf-file-processor.cpp`

**変更内容:**
- `service-function`セクションで複数の`function-prefix`を指定可能に
- 重み（weight）の範囲制限を削除（任意の非負の値を受け入れる）

**実装のポイント:**
```cpp
// 複数のfunction-prefixを読み込む
for (const auto& item : section) {
  if (item.first == "function-prefix") {
    std::string functionPrefixStr = item.second.get_value<std::string>();
    m_confParam.addServiceFunctionPrefix(ndn::Name(functionPrefixStr));
  }
}
```

#### 1.2.4 `src/conf-parameter.hpp`

**変更内容:**
- 単一の`m_serviceFunctionPrefix`から`std::set<ndn::Name> m_serviceFunctionPrefixes`に変更
- 複数のService Functionプレフィックスを管理可能に

### 1.3 FunctionCostの計算方法

**計算式:**
```
FunctionCost = processingTime * processingWeight + 
               load * loadWeight + 
               (usageCount / 100.0) * usageWeight
```

**現在の設定:**
- `processing-weight`: 10000
- `load-weight`: 0.0
- `usage-weight`: 0.0

**影響:**
- ProcessingTimeが主な要因
- ProcessingTimeが1ミリ秒（0.001秒）の場合、FunctionCost = 0.001 * 10000 = 10
- リンクコスト（25, 50）に対して、FunctionCost（10）が追加される

## 2. ログファイル関連のトラブルシューティング

### 2.1 問題の発見

**問題:**
- node1とnode2のsidecarログが同じ内容になっていた
- ProcessingTimeが同じ値（0.003075秒）になっていた

**原因:**
- `ndn-node1.yaml`と`ndn-node2.yaml`の両方で同じ`/mnt/sidecar-logs`を使用していた
- 両方のノードが同じログファイルを参照していた

### 2.2 修正内容

**修正前:**
```yaml
# ndn-node1.yaml
volumes:
  - name: sidecar-logs
    hostPath:
      path: /mnt/sidecar-logs

# ndn-node2.yaml
volumes:
  - name: sidecar-logs
    hostPath:
      path: /mnt/sidecar-logs
```

**修正後:**
```yaml
# ndn-node1.yaml
volumes:
  - name: sidecar-logs
    hostPath:
      path: /mnt/sidecar-logs-node1

# ndn-node2.yaml
volumes:
  - name: sidecar-logs
    hostPath:
      path: /mnt/sidecar-logs-node2
```

### 2.3 修正後の確認

**確認結果:**
- node1のsidecarログファイル: `/var/log/sidecar/service.log` (0バイト、新規作成)
- node2のsidecarログファイル: `/var/log/sidecar/service.log` (17KB、更新されている)
- それぞれのノードが独自のログファイルを使用していることを確認

## 3. 現状の課題

### 3.1 FunctionCostが適用されていない可能性

**現状:**
- node3のFIB: `/relay nexthops={faceid=262 (cost=25), faceid=264 (cost=50)}`
- 最小コストのパス（cost=25、node2）が選択されている
- FunctionCostが適用されていない可能性がある

**考えられる原因:**
1. NameLSAにService Function情報が含まれていない
2. wireEncode/wireDecodeが正常に動作していない
3. adjustNexthopCostsが呼び出されていない
4. FunctionCostが計算されているが、影響が小さい

### 3.2 node1のsidecarログが空

**現状:**
- node1のsidecarログファイルが空（0バイト）
- トラフィックがnode1にルーティングされていない

**考えられる原因:**
1. FunctionCostが適用されていないため、最小コストのパス（node2）が選択されている
2. コンテンツ提供元の位置による影響（node4で提供しても、node2が選択される）

## 4. 次のアクション

### 4.1 FunctionCostの適用状況を確認

#### 4.1.1 NameLSAの確認

**確認項目:**
- node1とnode2のNameLSAにService Function情報が含まれているか
- wireEncodeのデバッグログでService Function情報がエンコードされているか

**確認方法:**
```bash
# node1のNLSRログからwireEncodeログを確認
kubectl exec <node1-pod> -- tail -1000 /nlsr.log | grep "wireEncode.*Service Function"

# node2のNLSRログからwireEncodeログを確認
kubectl exec <node2-pod> -- tail -1000 /nlsr.log | grep "wireEncode.*Service Function"
```

#### 4.1.2 wireDecodeの確認

**確認項目:**
- node3とnode4のNameLSAにService Function情報がデコードされているか
- wireDecodeのデバッグログでService Function情報がデコードされているか

**確認方法:**
```bash
# node3のNLSRログからwireDecodeログを確認
kubectl exec <node3-pod> -- tail -1000 /nlsr.log | grep "wireDecode.*Service Function"

# node4のNLSRログからwireDecodeログを確認
kubectl exec <node4-pod> -- tail -1000 /nlsr.log | grep "wireDecode.*Service Function"
```

#### 4.1.3 adjustNexthopCostsの確認

**確認項目:**
- adjustNexthopCostsが呼び出されているか
- FunctionCostが計算されているか
- 計算されたFunctionCostがコストに追加されているか

**確認方法:**
```bash
# node3のNLSRログからadjustNexthopCostsログを確認
kubectl exec <node3-pod> -- tail -1000 /nlsr.log | grep "adjustNexthopCosts"

# FunctionCost計算のログを確認
kubectl exec <node3-pod> -- tail -1000 /nlsr.log | grep "FunctionCost calculated"
```

### 4.2 トラフィック分散の確認

#### 4.2.1 トラフィック分散のテスト

**テスト方法:**
1. node4でコンテンツを提供（既に実施済み）
2. node3から複数回リクエストを送信（既に実施済み）
3. node1とnode2のsidecarログを確認
4. トラフィックが分散されているか確認

**期待される結果:**
- FunctionCostが適用されれば、node1とnode2のコストが調整される
- トラフィックが分散され、両方のノードのsidecarログが更新される

#### 4.2.2 トラフィック分散の実現方法

**方法1: FunctionCostの適用**
- FunctionCostが適用されれば、node1とnode2のコストが調整される
- トラフィックが分散される可能性がある

**方法2: 手動でのコスト調整**
- 設定ファイルでリンクコストを調整
- トラフィックが分散されるようにする

**方法3: トラフィック生成の変更**
- node1から直接トラフィックを生成
- node1のsidecarログが更新されるか確認

### 4.3 デバッグログの追加

#### 4.3.1 追加すべきデバッグログ

**NamePrefixTable::adjustNexthopCosts:**
- NameLSAの取得状況
- Service Function情報の取得状況
- FunctionCostの計算結果
- 調整後のコスト

**NameLSA::wireEncode:**
- エンコードするService Function情報の数
- 各情報の詳細（名前、ProcessingTime、Load、UsageCount）

**NameLSA::wireDecode:**
- デコードするService Function情報の数
- 各情報の詳細（名前、ProcessingTime、Load、UsageCount）

### 4.4 設定ファイルの確認

#### 4.4.1 確認項目

**node1とnode2の設定ファイル:**
- `service-function`セクションが存在するか
- `function-prefix`が正しく設定されているか
- `processing-weight`、`load-weight`、`usage-weight`が正しく設定されているか

**node3とnode4の設定ファイル:**
- `service-function`セクションは不要（NameLSAから情報を取得するため）

## 5. 優先順位

### 優先度1: FunctionCostの適用状況を確認

1. NameLSAにService Function情報が含まれているか確認
2. wireEncode/wireDecodeが正常に動作しているか確認
3. adjustNexthopCostsが呼び出されているか確認
4. FunctionCostが計算されているか確認

### 優先度2: トラフィック分散の実現

1. FunctionCostが適用されれば、トラフィックが分散されるか確認
2. 必要に応じて、設定を調整

### 優先度3: デバッグログの追加

1. 追加のデバッグログを追加
2. 問題の原因を特定

## 6. 期待される結果

### 6.1 FunctionCostが適用された場合

**期待される動作:**
- node1とnode2のコストが調整される
- トラフィックが分散される
- 両方のノードのsidecarログが更新される
- それぞれのノードで異なるProcessingTimeが記録される

**期待されるコスト:**
- node2: 25 + FunctionCost（例: 10） = 35
- node1: 50 + FunctionCost（例: 10） = 60
- node2が依然として選択されるが、FunctionCostの影響が確認できる

### 6.2 トラフィック分散が実現した場合

**期待される動作:**
- node1とnode2の両方のsidecarログが更新される
- それぞれのノードで異なるProcessingTimeが記録される
- トラフィックが分散される

## 7. まとめ

### 7.1 実装済みの機能

1. ✓ FunctionCostの計算ロジック
2. ✓ NameLSAへのService Function情報の追加
3. ✓ wireEncode/wireDecodeの実装
4. ✓ adjustNexthopCostsでのFunctionCost適用
5. ✓ hostPath設定の分離

### 7.2 確認が必要な項目

1. FunctionCostが適用されているか
2. NameLSAにService Function情報が含まれているか
3. wireEncode/wireDecodeが正常に動作しているか
4. adjustNexthopCostsが呼び出されているか

### 7.3 次のステップ

1. FunctionCostの適用状況を詳細に確認
2. 必要に応じて、デバッグログを追加
3. トラフィック分散の実現を確認

