# examples

手で動かして試すためのサンプル入力を置く。

```sh
mise run run -- examples/hello.src
```

テストの入力は**ここではなく** `tests/cases/` に置く。こちらは期待値を持たない、触って確かめ
るためだけの置き場。どちらに置くか迷ったら `tests/cases/`（そちらは壊れたときに気付ける）。

対象言語の文法が決まったら [../docs/spec/language.md](../docs/spec/language.md) を見ながら増や
していく。
