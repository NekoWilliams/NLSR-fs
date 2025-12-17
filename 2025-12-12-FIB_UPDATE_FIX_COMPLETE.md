2025-12-12

# FIB更新問題の完全修正

## 修正内容

### 問題点

同じ`FaceUri`でも異なる`destRouterName`（node1経由とnode2経由）からのNextHopを区別できず、一方が失われていました。

### 修正前の実装

```cpp
std::map<ndn::FaceUri, std::pair<NextHop, double>> nextHopMap;
```

**問題:**
- `FaceUri`のみをキーにしているため、同じ`FaceUri`でも異なる`destRouterName`のNextHopを区別できない
- 例：node1経由とnode2経由が同じ`FaceUri`を持つ場合、どちらか一方が失われる

### 修正後の実装

```cpp
std::map<std::pair<ndn::FaceUri, ndn::Name>, std::pair<NextHop, double>> nextHopMap;
```

**改善点:**
- `(FaceUri, destRouterName)`のペアをキーとして使用
- 同じ`FaceUri`でも異なる`destRouterName`のNextHopを区別できる
- 例：node1経由とnode2経由が同じ`FaceUri`を持つ場合でも、両方とも保持される

### 動作の確認

#### シナリオ1: 異なるdestRouterNameのNextHopを保持

```
node1経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node1, adjustedCost=100
node2経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node2, adjustedCost=60
```

**結果:**
- 両方とも`nextHopMap`に追加される（キーが異なるため）
- `adjustNexthopCosts`は両方のNextHopを返す
- FIBは両方のNextHopを登録する
- **正しく動作する** ✓

#### シナリオ2: 同じdestRouterNameのNextHopを更新

```
初期状態:
node2経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node2, adjustedCost=60

更新後:
node2経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node2, adjustedCost=120
```

**結果:**
- 同じ`(FaceUri, destRouterName)`のペアが既に存在するため、最新の値で更新される
- node2経由60が120に置き換えられる
- **正しく動作する** ✓

#### シナリオ3: 両方のノードが高いコストになった場合

```
初期状態:
node1経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node1, adjustedCost=75
node2経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node2, adjustedCost=50

両方のノードが高いコストになった場合:
node1経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node1, adjustedCost=150
node2経由: FaceUri=tcp4://10.104.74.134:6363, destRouterName=node2, adjustedCost=125
```

**結果:**
- 両方とも`nextHopMap`に追加される（キーが異なるため）
- `adjustNexthopCosts`は両方のNextHopを返す
- FIBは両方のNextHopを登録する
- **FIBは検知できる** ✓

## 修正の効果

### 1. 同じFaceUriでも異なるdestRouterNameのNextHopを保持

- node1経由とnode2経由のNextHopを両方とも保持できる
- どちらか一方が失われることがない

### 2. 正しいNextHopの更新

- 同じ`(FaceUri, destRouterName)`のペアが既に存在する場合、最新の値で更新される
- 例：node2経由60が120に更新される

### 3. 両方のノードが高いコストになった場合の検知

- FIBは両方のノードのコスト変更を検知できる
- 両方のNextHopが正しく登録される

## デバッグログの改善

以下のデバッグログを追加しました：

- `Added new NextHop: {FaceUri} via {destRouterName} with adjustedCost={cost}`
- `Updating existing NextHop: {FaceUri} via {destRouterName}`
- `Final NextHop added to list: {FaceUri} via {destRouterName} with cost={cost}`

これにより、同じ`FaceUri`でも異なる`destRouterName`のNextHopを区別できるようになりました。

## まとめ

この修正により、以下の問題が解決されました：

1. ✓ 同じ`FaceUri`でも異なる`destRouterName`のNextHopを保持できる
2. ✓ 同じ`destRouterName`からのNextHopが更新された場合、正しく置き換えられる
3. ✓ 両方のノードが高いコストになった場合、FIBは検知できる

