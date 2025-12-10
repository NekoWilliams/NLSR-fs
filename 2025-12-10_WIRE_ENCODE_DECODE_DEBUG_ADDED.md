# wireEncode/wireDecodeデバッグログ追加レポート

## 追加日時
2025年12月10日

## 追加したデバッグログ

### 1. wireEncodeメソッド (`src/lsa/name-lsa.cpp`)

**追加したログ:**
- エンコードするService Function情報の数を出力
- 各Service Function情報の内容を出力（名前、processingTime、load、usageCount）
- エンコード後の長さを出力

**ログの例:**
```
wireEncode: Encoding 1 Service Function info entries
wireEncode: Encoding Service Function: /relay, processingTime=0.00307512, load=0, usageCount=0
wireEncode: Encoded Service Function: /relay, length=XX
```

### 2. wireDecodeメソッド (`src/lsa/name-lsa.cpp`)

**追加したログ:**
- デコード開始時にログを出力
- ServiceFunction TLV要素が見つかった際にログを出力
- 各サブ要素（Name、ProcessingTime、Load、UsageCount）のデコード結果を出力
- デコード完了時に、最終的なService Function情報の内容を出力

**ログの例:**
```
wireDecode: Starting decode, will look for Service Function TLV elements
wireDecode: Found ServiceFunction TLV element #1
wireDecode: ServiceFunction TLV has X sub-elements
wireDecode: Decoded Service Function name: /relay
wireDecode: Decoded ProcessingTime: 0.00307512 (value_size=8)
wireDecode: Decoded Load: 0 (value_size=8)
wireDecode: Decoded UsageCount: 0
wireDecode: Stored Service Function info: /relay -> processingTime=0.00307512, load=0, usageCount=0
wireDecode: Completed decode. Service Function info entries: 1
wireDecode: Final Service Function info: /relay -> processingTime=0.00307512, load=0, usageCount=0
```

## 期待される効果

これらのデバッグログにより、以下を確認できます：

1. **wireEncodeでService Function情報が正しくエンコードされているか**
   - エンコードする情報の数と内容を確認
   - エンコード後の長さを確認

2. **wireDecodeでService Function情報が正しくデコードされているか**
   - ServiceFunction TLV要素が見つかっているか
   - 各サブ要素が正しくデコードされているか
   - 最終的なService Function情報が正しく格納されているか

3. **NameLSAの伝播時にService Function情報が失われているか**
   - node1とnode2が送信するNameLSAにService Function情報が含まれているか
   - node3が受信するNameLSAにService Function情報が含まれているか

## 次のステップ

1. **ビルドとデプロイ**
   - 変更をビルドしてデプロイ

2. **ログの確認**
   - wireEncode/wireDecodeのログを確認
   - Service Function情報が正しくエンコード/デコードされているか確認

3. **問題の特定**
   - Service Function情報が失われている箇所を特定
   - 必要に応じて追加の修正を実施

## 追加で確認すべき問題

### node1とnode2のsidecarログが同じ

**問題:**
- node1とnode2のsidecarログが全く同じ内容になっている
- 両方のPodが同じhostPath（`/mnt/sidecar-logs`）を使用している

**原因:**
- `relayPod.yaml`と`relayPod2.yaml`の両方で、同じhostPathを使用している
- これにより、両方のPodが同じファイルに書き込んでいる可能性がある

**解決策:**
- それぞれのPodが独自のsidecarログファイルを持つように、hostPathを分ける
- または、emptyDirを使用して、各Podが独自のログファイルを持つようにする

**確認事項:**
1. それぞれのPodが独自のsidecarログファイルを持っているか
2. sidecarログファイルのパスが正しく設定されているか
3. それぞれのPodが異なるsidecarログを生成しているか

