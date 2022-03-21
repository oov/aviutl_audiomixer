# AudioMixer.auf

AudioMixer.auf は拡張編集上でイコライザーやコンプレッサーを使えるようになる AviUtl プラグインです。

AudioMixer.auf の動作には AviUtl version 1.00 以降と拡張編集 version 0.92 以降が必要です。  
また、CPU が SSE2 に対応している必要があります。

## 注意事項

AudioMixer.auf は無保証で提供されます。  
AudioMixer.auf を使用したこと及び使用しなかったことによるいかなる損害について、開発者は責任を負いません。

これに同意できない場合、あなたはAudioMixer.auf を使用することができません。

## ダウンロード

https://github.com/oov/aviutl_audiomixer/releases

## インストール／アンインストール

AudioMixer.auf を **exedit.auf と同じ場所** に置いてください。

そして AviUtl を起動後、メインメニューから `［設定］→［フィルタ順序の設定］→［オーディオフィルタ順序の設定］` を開いて、

- `拡張編集(音声)` より上に `チャンネルストリップ - Aux`
- `拡張編集(音声)` より下に `チャンネルストリップ` と `チャンネルストリップ - マスター`

のように順序が設定されていれば設定完了です。

この順番が間違っていると全ての処理が上手くいかなくなるので、必ず正しい順序に設定してください。

アンインストールは AudioMixer.auf を削除すれば完了です。

## 使い方

### チャンネルストリップ

拡張編集タイムラインの右クリックメニューから
`［フィルタオブジェクトの追加］→［チャンネルストリップ］` を選ぶと、
タイムライン上に `チャンネルストリップ` オブジェクトが配置できます。

配置したオブジェクトには `ID` というパラメーターがあり、これを `-1` 以外にすることで
チャンネルストリップより上のレイヤーに置いた全ての音声データをチャンネルストリップが管理するようになります。

チャンネルストリップには異なる `ID` を設定することで、同じフレーム上で複数同時に使うことができます。
2つ目以降のチャンネルストリップは、上のチャンネルストリップより下にある音声データを管理するようになります。

現在使用可能なパラメーターには以下のものがあります。

- ID
- 入力音量
- 遅延
- EQ LoFreq
- EQ LoGain
- EQ HiFreq
- EQ HiGain
- C Thresh
- C Ratio
- C Attack
- C Release
- Aux ID
- Aux Send
- 出力音量
- 左右

パラメーター名のうち、接頭辞 EQ が付くものはイコライザー、C が付くものはコンプレッサー用のパラメーターです。  
Aux はセンドエフェクトのためのパラメーターで、これは後述します。

### チャンネルストリップ - Aux

拡張編集タイムラインの右クリックメニューから
`［フィルタオブジェクトの追加］→［チャンネルストリップ - Aux］` を選ぶと、
タイムライン上に `チャンネルストリップ - Aux` オブジェクトが配置できます。

これにもチャンネルストリップと同じように `ID` パラメーターがあり、
これを `-1` 以外に設定することで Aux を有効化することができます。

ただし、Aux はこれ単体ではまだ機能しません。
チャンネルストリップの `Aux ID` には `チャンネルストリップ - Aux` で設定した `ID` を指定し、
更に `Aux Send` で送る音量を指定することで使えるようになります。

現在使用可能なパラメーターには以下のものがあります。

- ID
- R PreDly
- R LPF
- R Diffuse
- R Decay
- R Damping
- R Excursion
- R Wet

パラメーター名のうち、接頭辞 R が付くものはリバーブ用のパラメーターです。  
現在はリバーブのみ実装されています。

#### 制限事項

`音声波形表示` のような音声データにアクセスする必要がある一部の機能のうち、音声ファイルを指定するのではなく、
すべての音声を対象とするような使い方と Aux は相性が悪く、エフェクトの掛かり方がおかしくなります。

これはバグではなく仕様です。
ただし、この場合でもパラアウトは正しく動作するはずです。

### チャンネルストリップ - マスター

マスターはプラグイン内部で自動的に使用される音声フィルターであり、タイムラインへの挿入はできません。  
複数のチャンネルストリップや Aux は、このマスターによって一本の音声データに合成されます。

また、マスターにはリミッターがあり、音が割れるほど音量が大きい場合でもなるべく割れないように対処します。

ただし、ひとつのチャンネルストリップに複数の音声を流したり、
チャンネルストリップではなく音声ファイルオブジェクト側で音量を大きくしていたり、
チャンネルストリップを使用していない場合は音が割れることがあります。  
（これらのケースではプラグインが音声を受け取った時点で既に音が割れているため対処できません）

### チャンネルストリップ - パラアウト

AviUtl のメインメニューから `［ファイル］→［エクスポート］→［チャンネルストリップ - パラアウト］` を選ぶと、
個々のチャンネルストリップの音声を別々の音声ファイルに保存できる「パラアウト」機能を使うことができます。

選択しているフレームの区間を一括で書き出せるので、音声の編集を DAW で行う場合などに有用です。

## ビルドについて

[MSYS2](https://www.msys2.org/) + MINGW32 上で開発し、リリース用ファイルは GitHub Actions にて生成しています。  
ビルド方法や必要になるパッケージなどは [GitHub Actions の設定ファイル](https://github.com/oov/aviutl_audiomixer/blob/main/.github/workflows/releaser.yml) を参照してください。

## Credits

AudioMixer.auf is made possible by the following open source softwares.

### Acutest

https://github.com/mity/acutest

The MIT License (MIT)

Copyright © 2013-2019 Martin Mitáš

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the “Software”),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

### AviUtl Plugin SDK

http://spring-fragrance.mints.ne.jp/aviutl/

The MIT License

Copyright (c) 1999-2012 Kenkun

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

### hashmap.c

https://github.com/tidwall/hashmap.c

NOTICE: This program used a modified version of hashmap.c.  
        https://github.com/oov/hashmap.c/tree/simplify

The MIT License (MIT)

Copyright (c) 2020 Joshua J Baker

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### mda VST plug-ins

http://mda.smartelectronix.com/
https://sourceforge.net/projects/mda-vst/

Copyright (c) 2008 Paul Kellett

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### RBJ Audio EQ Cookbook Filters

http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt  
http://www.musicdsp.org/showone.php?id=128  
http://www.musicdsp.org/showone.php?id=225

### TinyCThread

https://github.com/tinycthread/tinycthread

NOTICE: This program used a modified version of TinyCThread.  
        https://github.com/oov/tinycthread

Copyright (c) 2012 Marcus Geelnard
              2013-2016 Evan Nemerson

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.

### UXFDReverb

https://github.com/khoin/UXFDReverb

In jurisdictions that recognize copyright laws, this software is to
be released into the public domain.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
THE AUTHOR(S) SHALL NOT BE LIABLE FOR ANYTHING, ARISING FROM, OR IN
CONNECTION WITH THE SOFTWARE OR THE DISTRIBUTION OF THE SOFTWARE.

## 更新履歴

CHANGELOG を参照してください。

https://github.com/oov/aviutl_audiomixer/blob/main/CHANGELOG.md
