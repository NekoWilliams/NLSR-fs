# 利用率のコストが加算されない理由の検証結果

## 問題の概要

ProcessingTimeのときは正しく計算・加算できていたのに、利用率のコストは加算できない理由を検証しました。

## 検証結果

### 1. 値の違い（問題ではない）

**ProcessingTimeの場合:**
- 値の範囲: 数ミリ秒~数十ミリ秒（例: 0.005秒、0.05秒）
- `processing-weight = 10000`のとき:
  - FunctionCost = 0.005 * 10000 = 50
  - FunctionCost = 0.05 * 10000 = 500

**利用率の場合:**
- 値の範囲: 0.0 ~ 1.0（例: 0.0260789 = 2.6%）
- `processing-weight = 100`のとき:
  - FunctionCost = 0.0260789 * 100 = 2.6

**結論:** 値の違い自体は問題ではありません。コードでは`if (sfInfo.processingTime > 0.0 || ...)`という条件があるため、0.0260789 > 0.0はtrueになります。

### 2. タイミングの問題（根本原因）

**ログ分析結果:**

1. **初期ルーティング計算時（1765389229）:**
   - `adjustNexthopCosts`が呼ばれた
   - この時点では、node2のNameLSAにService Function情報がまだ含まれていない（map size: 0）
   - FunctionCost = 0として計算された

2. **NameLSA更新時（1765389377, 1765389382）:**
   - node2がNameLSAを更新し、Service Function情報を含めた
   - node3が更新されたNameLSAを受信し、Service Function情報を正しくデコードした
   - `onLsdbModified`が呼ばれ、ルーティング計算がスケジュールされた
   - **しかし、その後のルーティング計算実行時に`adjustNexthopCosts`が呼ばれていない**

### 3. 根本原因

**問題の本質:**

ルーティング計算がスケジュールされているが、実際に実行されたときに`adjustNexthopCosts`が呼ばれていない、または、呼ばれたときにまだService Function情報が含まれていない。

**ProcessingTimeのときとの違い:**

ProcessingTimeのときは、おそらく初期ルーティング計算時点で既にService Function情報が含まれていた可能性があります。これは、ProcessingTimeの計算タイミングが異なっていたためです。

**利用率の場合:**

利用率は時間窓（1秒）内の処理時間を計算するため、初期ルーティング計算時点ではまだ値が0の可能性が高く、その後トラフィックが発生してから値が更新されます。しかし、その時点でルーティング計算が再実行されていない、または実行されても`adjustNexthopCosts`が呼ばれていない可能性があります。

### 4. コードレベルの問題

**確認が必要な点:**

1. **ルーティング計算の実行確認**
   - `calculate()`メソッドが呼ばれたときに、`adjustNexthopCosts`が呼ばれているか確認
   - ログを見ると、ルーティング計算がスケジュールされているが、実行時のログが見当たらない

2. **NamePrefixTableの更新確認**
   - `NamePrefixTable::updateFromLsdb()`が呼ばれたときに、`adjustNexthopCosts`が呼ばれているか確認
   - `NamePrefixTable::addEntry()`が呼ばれたときに、`adjustNexthopCosts`が呼ばれているか確認

3. **ルーティング計算のスケジュール**
   - `scheduleRoutingTableCalculation()`が呼ばれたときに、実際にルーティング計算が実行されるまでの遅延を確認
   - ルーティング計算が実行されていない可能性がある

## 解決策

### 1. ルーティング計算の実行確認

ルーティング計算が実際に実行されたときに、`adjustNexthopCosts`が呼ばれているか確認する必要があります。

**確認方法:**
- `calculate()`メソッドにデバッグログを追加
- `calculateLsRoutingTable()`メソッドにデバッグログを追加
- `afterRoutingChange()`メソッドにデバッグログを追加

### 2. NamePrefixTableの更新確認

`NamePrefixTable::updateFromLsdb()`が呼ばれたときに、`adjustNexthopCosts`が呼ばれているか確認する必要があります。

**確認方法:**
- `updateFromLsdb()`メソッドにデバッグログを追加
- `addEntry()`メソッドにデバッグログを追加

### 3. ルーティング計算のスケジュール確認

`scheduleRoutingTableCalculation()`が呼ばれたときに、実際にルーティング計算が実行されるまでの遅延を確認する必要があります。

**確認方法:**
- `scheduleRoutingTableCalculation()`メソッドにデバッグログを追加
- ルーティング計算の実行タイミングを確認

## 期待される動作

### 1. NameLSA更新時

1. node2がNameLSAを更新（Service Function情報を含む）
2. node3が更新されたNameLSAを受信
3. `onLsdbModified`が呼ばれ、ルーティング計算がスケジュールされる
4. **ルーティング計算が実行される**
5. `adjustNexthopCosts`が呼ばれる
6. node2のNameLSAからService Function情報を取得
7. FunctionCostを計算（例: 0.0260789 * 100 = 2.6）
8. 最終コストを計算（例: 25 + 2.6 = 27.6）
9. FIBが更新される

### 2. ルーティングコストの更新

**修正後（利用率2.6%の場合）:**
```
/relay nexthops={faceid=262 (cost=27.6), faceid=264 (cost=50)}
```

**修正後（利用率30%の場合）:**
```
/relay nexthops={faceid=262 (cost=55), faceid=264 (cost=50)}
→ node1の方が低コスト → トラフィックがnode1にルーティング
```

## 結論

**ProcessingTimeと利用率の違いによる問題の根本原因:**

1. **タイミングの問題**: 利用率は時間窓内の処理時間を計算するため、初期ルーティング計算時点ではまだ値が0の可能性が高い
2. **ルーティング計算の実行**: NameLSA更新後にルーティング計算がスケジュールされているが、実際に実行されたときに`adjustNexthopCosts`が呼ばれていない可能性がある
3. **Service Function情報の伝播**: NameLSA更新後にService Function情報が正しく伝播されていない可能性がある

**次のステップ:**

1. ルーティング計算の実行確認
2. NamePrefixTableの更新確認
3. デバッグログの追加
4. 再テスト

