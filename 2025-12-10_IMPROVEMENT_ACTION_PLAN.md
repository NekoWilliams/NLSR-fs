# 改善アクションプラン

## 現在の問題

1. **NameLSAにService Function情報が含まれているが、`adjustNexthopCosts`で取得した情報がすべて0**
   - node3のログ: "ServiceFunctionInfo for /relay: processingTime=0, load=0, usageCount=0"
   - しかし、node1とnode2のログでは、Service Function情報が更新されている

2. **NameLSAの更新タイミングとルーティング計算のタイミングがずれている可能性**
   - Service Function情報が更新された後、ルーティング計算が再実行されていない可能性

## 改善ステップ

### ステップ1: デバッグログの追加

**目的:** NameLSAの状態を詳細に確認し、問題の原因を特定する

**実装箇所:**
1. `src/route/name-prefix-table.cpp`の`adjustNexthopCosts`関数
   - NameLSAの`m_serviceFunctionInfo`マップの内容を出力
   - `getServiceFunctionInfo`メソッドの動作を確認

2. `src/lsa/name-lsa.cpp`の`getServiceFunctionInfo`メソッド
   - マップの検索結果を詳細に出力

3. `src/lsa/name-lsa.cpp`の`wireDecode`メソッド
   - Service Function情報のデコード結果を確認

**追加するログ:**
```cpp
// adjustNexthopCosts関数内
NLSR_LOG_DEBUG("NameLSA m_serviceFunctionInfo map size: " << nameLsa->getServiceFunctionInfoMapSize());
for (const auto& [name, info] : nameLsa->getAllServiceFunctionInfo()) {
  NLSR_LOG_DEBUG("  Service Function: " << name << ", processingTime=" << info.processingTime);
}
```

### ステップ2: NameLSA更新時のルーティング計算スケジュール確認

**目的:** Service Function情報が更新された際に、ルーティング計算が確実に実行されるようにする

**確認箇所:**
1. `src/publisher/sidecar-stats-handler.cpp`の`updateServiceFunctionInfo`関数
   - NameLSAが更新された後、ルーティング計算がスケジュールされているか確認

2. `src/route/routing-table.cpp`の`scheduleRoutingTableCalculation`関数
   - NAMEタイプのLSA更新時にルーティング計算がスケジュールされているか確認

**実装内容:**
- Service Function情報が更新された際に、ルーティング計算をスケジュール
- または、NameLSAの`update`メソッドで、Service Function情報が変更された場合にルーティング計算をスケジュール

### ステップ3: NameLSAの`getServiceFunctionInfo`メソッドの改善

**目的:** NameLSAからService Function情報を正しく取得できるようにする

**確認事項:**
1. `m_serviceFunctionInfo`マップに正しく情報が格納されているか
2. `getServiceFunctionInfo`メソッドが正しく動作しているか
3. NameLSAの`wireDecode`でService Function情報が正しくデコードされているか

**改善案:**
- `getServiceFunctionInfo`メソッドにデバッグログを追加
- マップの内容を確認するためのヘルパーメソッドを追加

### ステップ4: 動作確認とテスト

**目的:** 改善が正しく動作することを確認する

**確認項目:**
1. Service Function情報がNameLSAに正しく含まれているか
2. 他のノードでNameLSAからService Function情報が正しく取得できるか
3. FunctionCostが正しく計算されているか
4. FIBコストが正しく更新されているか

## 実装の優先順位

### 優先度1: デバッグログの追加（問題の原因特定）
- **所要時間:** 30分
- **影響範囲:** ログ出力のみ（動作への影響なし）
- **効果:** 問題の原因を特定できる

### 優先度2: NameLSA更新時のルーティング計算スケジュール
- **所要時間:** 1時間
- **影響範囲:** ルーティング計算のタイミング
- **効果:** Service Function情報が更新された際に、確実にルーティング計算が実行される

### 優先度3: NameLSAの`getServiceFunctionInfo`メソッドの改善
- **所要時間:** 1時間
- **影響範囲:** Service Function情報の取得
- **効果:** Service Function情報が正しく取得できるようになる

### 優先度4: 動作確認とテスト
- **所要時間:** 1時間
- **影響範囲:** テストのみ
- **効果:** 改善が正しく動作することを確認

## 次のアクション

1. **デバッグログの追加**
   - `src/route/name-prefix-table.cpp`の`adjustNexthopCosts`関数にログを追加
   - `src/lsa/name-lsa.cpp`の`getServiceFunctionInfo`メソッドにログを追加
   - NameLSAの`m_serviceFunctionInfo`マップの内容を確認するためのヘルパーメソッドを追加

2. **ビルドとデプロイ**
   - 変更をビルド
   - デプロイして動作確認

3. **ログの確認**
   - デバッグログを確認し、問題の原因を特定

4. **必要に応じて追加の修正**
   - 問題の原因に応じて、追加の修正を実施

## 期待される結果

1. **デバッグログから問題の原因が特定できる**
   - NameLSAの`m_serviceFunctionInfo`マップの内容が確認できる
   - `getServiceFunctionInfo`メソッドの動作が確認できる

2. **Service Function情報が正しく取得できる**
   - `adjustNexthopCosts`でService Function情報が正しく取得できる
   - FunctionCostが正しく計算される

3. **FIBコストが正しく更新される**
   - `/relay`プレフィックスのFIBコストが、FunctionCostを含めた値に更新される

