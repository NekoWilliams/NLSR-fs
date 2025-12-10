# ProcessingTimeの影響を10~50%にするための選択肢

## 現在のコスト計算方法

### PathCost構造体
```cpp
struct PathCost {
  double linkCost;       // リンクコスト
  double functionCost;   // ファンクションコスト
  double totalCost;      // 総合コスト = linkCost + functionCost
};
```

### calculateCombinedCost関数
```cpp
double functionCost = sfInfo.processingTime * processingWeight +
                     sfInfo.load * loadWeight +
                     (sfInfo.usageCount / 100.0) * usageWeight;

return PathCost(linkCost, functionCost);
```

### 現在の状況
- linkCost: 50 (node1), 25 (node2)
- processingTime: 0.00166512秒
- processing-weight: 1000
- functionCost: 0.00166512 × 1000 = 1.66512
- totalCost: 50.0013 (node1), 25.0007 (node2)
- **影響: 約3.3% (1.66512 / 50 = 3.3%)**

### 目標
- 影響を10~50%にする
- node1の場合: functionCost = 5 ~ 25 (linkCost=50に対して)
- node2の場合: functionCost = 2.5 ~ 12.5 (linkCost=25に対して)

---

## 選択肢1: processing-weightをさらに大きくする

### 方法
`processing-weight`を3000~15000に設定する

### 計算
- 現在: functionCost = 0.00166512 × 1000 = 1.66512
- 目標（10%影響）: functionCost = 5
  - 必要なweight: 5 / 0.00166512 = **約3000**
- 目標（50%影響）: functionCost = 25
  - 必要なweight: 25 / 0.00166512 = **約15000**

### 設定例
```conf
service-function
{
  processing-weight  3000  ; 10%影響の場合
  ; または
  processing-weight  15000 ; 50%影響の場合
  load-weight       0.0
  usage-weight      0.0
}
```

### メリット
- ✅ 実装が簡単（設定ファイルの変更のみ）
- ✅ 既存のコードを変更する必要がない
- ✅ 他の計算ロジックに影響しない

### デメリット
- ⚠️ weightの値が非常に大きくなる（3000~15000）
- ⚠️ processingTimeが変化した場合、影響が大きくなりすぎる可能性

---

## 選択肢2: totalCostの計算方法を変更（重み付き加算）

### 方法
`totalCost`の計算を、linkCostとfunctionCostの重み付き加算に変更する

### 現在の実装
```cpp
totalCost = linkCost + functionCost;
```

### 変更案
```cpp
// 設定ファイルに新しいパラメータを追加
double linkWeight = 1.0;      // デフォルト
double functionWeight = 10.0;  // 新しいパラメータ

totalCost = linkCost * linkWeight + functionCost * functionWeight;
```

### 計算例
- linkCost = 50, functionCost = 1.66512
- linkWeight = 1.0, functionWeight = 10.0
- totalCost = 50 × 1.0 + 1.66512 × 10.0 = 50 + 16.6512 = 66.6512
- 影響: 16.6512 / 50 = 33.3%

### メリット
- ✅ linkCostとfunctionCostのバランスを調整できる
- ✅ より柔軟なコスト計算が可能

### デメリット
- ⚠️ コードの変更が必要
- ⚠️ 設定ファイルに新しいパラメータを追加する必要がある
- ⚠️ 既存のルーティングアルゴリズムに影響する可能性

---

## 選択肢3: functionCostをlinkCostに対する割合で計算

### 方法
`functionCost`を、linkCostに対する割合として計算する

### 現在の実装
```cpp
double functionCost = sfInfo.processingTime * processingWeight;
```

### 変更案
```cpp
// processingWeightを割合として扱う（0.1 = 10%, 0.5 = 50%）
double functionCost = linkCost * (sfInfo.processingTime * processingWeight);
// または
double functionCost = linkCost * processingWeight;  // processingWeightを直接割合として使用
```

### 計算例
- linkCost = 50
- processingWeight = 0.1 (10%影響)
- functionCost = 50 × 0.1 = 5
- totalCost = 50 + 5 = 55
- 影響: 5 / 50 = 10%

### メリット
- ✅ 影響を直接的に制御できる（10% = 0.1, 50% = 0.5）
- ✅ 設定が直感的

### デメリット
- ⚠️ コードの変更が必要
- ⚠️ processingTimeの値が無視される（または別の方法で考慮する必要がある）

---

## 選択肢4: functionCostをlinkCostに対する相対値で計算（推奨）

### 方法
`functionCost`を、linkCostに対する相対値として計算し、processingTimeをスケーリングファクターとして使用

### 変更案
```cpp
// processingWeightをlinkCostに対する割合のスケーリングファクターとして使用
// 例: processingWeight = 1000, processingTime = 0.00166512
//     functionCost = linkCost × (processingTime × processingWeight / baseProcessingTime)
//     baseProcessingTime = 0.001 (基準値)

double baseProcessingTime = 0.001;  // 設定可能
double relativeProcessingTime = sfInfo.processingTime / baseProcessingTime;
double functionCost = linkCost * (relativeProcessingTime * processingWeight / 100.0);
```

### 計算例
- linkCost = 50
- processingTime = 0.00166512
- baseProcessingTime = 0.001
- processingWeight = 10.0 (10%影響の場合)
- relativeProcessingTime = 0.00166512 / 0.001 = 1.66512
- functionCost = 50 × (1.66512 × 10.0 / 100.0) = 50 × 0.166512 = 8.3256
- totalCost = 50 + 8.3256 = 58.3256
- 影響: 8.3256 / 50 = 16.7%

### メリット
- ✅ processingTimeの値も考慮される
- ✅ linkCostに対する影響を直接制御できる
- ✅ より柔軟な設定が可能

### デメリット
- ⚠️ コードの変更が必要
- ⚠️ 設定がやや複雑

---

## 推奨される選択肢

### 短期的な解決策: 選択肢1
- **processing-weightを3000~15000に設定**
- 実装が簡単で、すぐに効果を確認できる

### 長期的な解決策: 選択肢4
- **functionCostをlinkCostに対する相対値で計算**
- より柔軟で、将来の拡張にも対応できる

---

## 実装の優先順位

1. **選択肢1（推奨）**: 設定ファイルの変更のみで実現可能
2. **選択肢4**: より柔軟な実装が必要な場合
3. **選択肢2**: より複雑なコスト計算が必要な場合
4. **選択肢3**: processingTimeを無視しても良い場合

