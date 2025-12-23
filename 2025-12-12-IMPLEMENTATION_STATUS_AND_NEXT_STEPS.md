2025-12-12

# 実装状況と次のステップ

## 実装済みの内容

### 1. サービスファンクション識別フラグの実装

#### 1.1 PrefixInfoへのisServiceFunctionフラグ追加

**ファイル**: `src/name-prefix-list.hpp`, `src/name-prefix-list.cpp`

- `PrefixInfo`クラスに`bool m_isServiceFunction`メンバを追加
- `isServiceFunction()`, `setIsServiceFunction()`メソッドを追加
- コンストラクタに`isServiceFunction`パラメータを追加

**目的**: NameLSA内のPrefixInfoにサービスファンクション識別情報を埋め込む

#### 1.2 TLVエンコーディング/デコーディングの実装

**ファイル**: `src/tlv-nlsr.hpp`, `src/name-prefix-list.cpp`

- 新しいTLVタイプ`IsServiceFunction = 156`を追加
- `PrefixInfo::wireEncode()`で`isServiceFunction`フラグをエンコード（trueの場合のみ）
- `PrefixInfo::wireDecode()`で`isServiceFunction`フラグをデコード（後方互換性のためデフォルトはfalse）

**目的**: NameLSAを交換する際に、サービスファンクション識別情報を伝播する

#### 1.3 NameLSA構築時のフラグ設定

**ファイル**: `src/lsdb.cpp`

- `buildAndInstallOwnNameLsa()`で、設定ファイルの`function-prefix`に基づいて`isServiceFunction`フラグを設定
- 各`PrefixInfo`に対して`setIsServiceFunction(true)`を呼び出し

**目的**: サービスファンクション関連のプレフィックスをNameLSAに明示的にマークする

### 2. adjustNexthopCostsの修正

#### 2.1 サービスファンクション関連のNextHopのみにFunctionCostを適用

**ファイル**: `src/route/name-prefix-table.cpp`

- `adjustNexthopCosts()`の冒頭で、`nameToCheck`がサービスファンクション関連のプレフィックスかどうかを判定
- サービスファンクション関連でない場合は、元の`NexthopList`をそのまま返す

**目的**: サービスファンクションに関連のないNextHopにはFunctionCostを適用しない

#### 2.2 同じFaceUriでも異なるdestRouterNameのNextHopを区別

**ファイル**: `src/route/name-prefix-table.cpp`

- `std::map<std::pair<ndn::FaceUri, ndn::Name>, std::pair<NextHop, double>>`を使用
- キーとして`(FaceUri, destRouterName)`のペアを使用
- これにより、同じFaceUriでも異なるdestRouterNameのNextHopを区別できる

**目的**: 同じFaceUriでも異なるdestRouterNameのNextHopが両方とも保持され、それぞれが更新された際に正しく反映される

#### 2.3 FunctionCostの計算ロジック

**ファイル**: `src/route/name-prefix-table.cpp`

- 各`RoutingTablePoolEntry`に対して個別にFunctionCostを計算
- `destRouterName`のNameLSAから`ServiceFunctionInfo`を取得
- スタイル（古い）データの判定（`timeSinceLastUpdate > threshold`）
- `functionCost = processingTime * processingWeight + load * loadWeight + usageCount * usageWeight`

**目的**: 各NextHopに対して正確なFunctionCostを計算し、動的にコストを更新する

### 3. 設定ファイルの修正

**ファイル**: `src/conf-file-processor.cpp`

- デフォルトの`/relay`プレフィックスの追加を削除
- 設定ファイルに`function-prefix`が明示的に指定されている場合のみ、サービスファンクションとして認識

**目的**: ハードコードされたプレフィックスを削除し、設定ファイルベースの柔軟な設定を実現

### 4. テストと検証

#### 4.1 サービスファンクションフラグのテスト

- TLVエンコーディング/デコーディングの動作確認
- NameLSAへのフラグ設定の確認
- 異なるノード間でのフラグ伝播の確認

#### 4.2 高トラフィックテスト

- 5分間の負荷テストを実施（約1秒に3回のリクエスト）
- 358リクエストを生成
- コスト推移の監視（10秒間隔、31回測定）

## 現在の問題点

### 1. FunctionCostが動的に更新されない

**現象**:
- コストが一定値を維持（変化なし）
- 全パスでコストが固定値のまま

**根本原因**:
1. **ServiceFunctionInfoがスタイル（古い）と判定されている**
   - `timeSinceLastUpdate > threshold (120秒)` → `functionCost=0`
   - ログファイルが更新されないため、`lastUpdateTime`が古いまま

2. **ログファイルが更新されていない**
   - サイドカーログのハッシュが変わっていない
   - `updateNameLsaWithStats()`が呼び出されない
   - ServiceFunctionInfoが更新されない

3. **トラフィックが実際に到達していない可能性**
   - `ndncatchunks`がタイムアウト（exit code 124）している
   - 実際のデータ転送が成功していない可能性

### 2. トラフィック分散が機能していない

**現象**:
- node2にのみトラフィックが集中（分散度: 0.0%）
- node1にはトラフィックが到達していない

**原因**:
- コストが一定値を維持しているため、常に同じパスが選択される
- FunctionCostが更新されないため、負荷分散が機能しない

## 次のステップ

### 優先度1: 即座に対応すべき項目

#### 1.1 ndncatchunksのタイムアウト問題の解決

**問題**: `ndncatchunks`がタイムアウト（exit code 124）している

**対応策**:
1. タイムアウト時間を延長する（現在: 1秒 → 5秒以上）
2. データ転送の成功を確認する（エラーログの確認）
3. ネットワーク接続を確認する（FIBエントリの確認）

**実装**:
```bash
# タイムアウト時間を延長
timeout 5 ndncatchunks /relay/content1_1.txt

# エラーログを確認
kubectl exec -n default deployment/ndn-node3 -- bash -c "ndncatchunks /relay/content1_1.txt 2>&1"
```

#### 1.2 サイドカーログの更新メカニズムの確認

**問題**: サイドカーログが更新されていない

**対応策**:
1. ログファイルのパスを確認する
2. サイドカーのログ書き込みを確認する
3. ログファイルの変更検出メカニズムを確認する

**実装**:
```bash
# ログファイルのパスを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "ls -la /var/log/sidecar/"

# ログファイルの変更を監視
kubectl exec -n default deployment/ndn-node1 -- bash -c "tail -f /var/log/sidecar/service.log"

# サイドカーのログを確認
kubectl logs -n default deployment/ndn-node1 -c sidecar
```

#### 1.3 スタイルデータの閾値を調整

**問題**: `threshold`が120秒で、ログファイルの更新頻度が低い場合にFunctionCostが0になる

**対応策**:
- `threshold`を`utilization-window-seconds * 3`（180秒）に変更
- または、`utilization-window-seconds * 4`（240秒）に変更

**実装**:
```cpp
// src/route/name-prefix-table.cpp
// thresholdを調整
double threshold = m_confParam.getUtilizationWindowSeconds() * 3; // 180秒
```

### 優先度2: 中期的な対応項目

#### 2.1 強制的な更新メカニズムの実装

**問題**: ログファイルが更新されなくても、定期的にServiceFunctionInfoを更新する必要がある

**対応策**:
- 一定時間ごと（例: 60秒）に強制的にログファイルを再読み込みする
- ハッシュが変わっていなくても、一定時間経過後に更新する

**実装**:
```cpp
// src/sidecar-stats-handler.cpp
// 強制的な更新メカニズムを追加
void SidecarStatsHandler::startLogMonitoring() {
    // 現在の実装に加えて
    // 最後の更新時刻を記録
    // 60秒経過したら強制的に更新
}
```

#### 2.2 ログファイルの変更検出の改善

**問題**: ハッシュだけでなく、ファイルサイズやmtimeも確認する必要がある

**対応策**:
- ファイルサイズの変更も検出する
- ファイルのmtime（変更時刻）をチェックする
- ハッシュだけでなく、複数の指標を使用する

**実装**:
```cpp
// src/sidecar-stats-handler.cpp
// 複数の指標を使用
bool SidecarStatsHandler::isLogFileChanged() {
    // ハッシュ、ファイルサイズ、mtimeを確認
    // いずれかが変更されていれば更新
}
```

### 優先度3: 長期的な対応項目

#### 3.1 トラフィック生成の強化

**問題**: より多くのトラフィックを生成して、ログファイルを定期的に更新する必要がある

**対応策**:
- リクエスト間隔を短くする（0.1秒間隔など）
- 複数のプロセスから同時にリクエストを送信する
- より長い期間のテストを実施する

**実装**:
```bash
# より多くのトラフィックを生成
for i in {1..50}; do
    kubectl exec -n default deployment/ndn-node3 -- bash -c "timeout 5 ndncatchunks /relay/content1_$i.txt > /dev/null 2>&1" &
done
```

#### 3.2 FunctionCostの動的更新の検証

**問題**: FunctionCostが実際に動的に更新されているかを検証する必要がある

**対応策**:
- コストの変化を詳細に監視する
- ログからFunctionCostの計算過程を確認する
- トラフィック分散の効果を測定する

**実装**:
```bash
# コストの変化を詳細に監視
watch -n 1 "kubectl exec -n default deployment/ndn-node3 -- bash -c 'nfdc route list | grep /relay'"

# FunctionCostの計算過程を確認
kubectl exec -n default deployment/ndn-node3 -- bash -c "cat /proc/\$(pgrep nlsr)/fd/1 | grep -E 'functionCost|adjustNexthopCosts'"
```

## 実装の確認方法

### 1. サービスファンクションフラグの確認

```bash
# NameLSAからフラグを確認
kubectl exec -n default deployment/ndn-node1 -- bash -c "nlsrc lsdb | grep -A 10 'Name LSA'"
```

### 2. FunctionCostの計算確認

```bash
# adjustNexthopCostsのログを確認
kubectl exec -n default deployment/ndn-node3 -- bash -c "cat /proc/\$(pgrep nlsr)/fd/1 | grep adjustNexthopCosts"
```

### 3. コストの推移確認

```bash
# FIBエントリのコストを確認
kubectl exec -n default deployment/ndn-node3 -- bash -c "nfdc route list | grep /relay"
```

## まとめ

### 実装済みの機能

1. ✅ サービスファンクション識別フラグ（PrefixInfoにisServiceFunctionフラグ）
2. ✅ TLVエンコーディング/デコーディング
3. ✅ adjustNexthopCostsの修正（サービスファンクション関連のNextHopのみにFunctionCostを適用）
4. ✅ 同じFaceUriでも異なるdestRouterNameのNextHopを区別
5. ✅ 設定ファイルベースの柔軟な設定

### 現在の問題

1. ❌ FunctionCostが動的に更新されない
2. ❌ トラフィック分散が機能していない
3. ❌ サイドカーログが更新されていない

### 次のステップ

1. **優先度1**: ndncatchunksのタイムアウト問題の解決、サイドカーログの更新メカニズムの確認、スタイルデータの閾値を調整
2. **優先度2**: 強制的な更新メカニズムの実装、ログファイルの変更検出の改善
3. **優先度3**: トラフィック生成の強化、FunctionCostの動的更新の検証

