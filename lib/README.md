# lib/ — プロジェクト横断の共有ライブラリ

ここに置いたライブラリは全 env から自動的に見える（PlatformIO の `lib_dir` 既定）。

```
lib/
└── MyHelper/
    ├── MyHelper.h
    └── MyHelper.cpp
```

`#include <MyHelper.h>` で参照できる。特定プロジェクトでしか使わないコードは
`projects/<name>/` 側に置くこと。
