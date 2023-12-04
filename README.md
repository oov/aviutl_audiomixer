AudioMixer.auf
==============

AudioMixer.auf は拡張編集上でイコライザーやコンプレッサーを使えるようになる AviUtl プラグインです。

AudioMixer.auf の動作には AviUtl version 1.00 以降と拡張編集 version 0.92 以降が必要です。  
また、CPU が SSE2 に対応している必要があります。

注意事項
-------

AudioMixer.auf は無保証で提供されます。  
AudioMixer.auf を使用したこと及び使用しなかったことによるいかなる損害について、開発者は責任を負いません。

これに同意できない場合、あなたはAudioMixer.auf を使用することができません。

ダウンロード
------------

https://github.com/oov/aviutl_audiomixer/releases

インストール／アンインストール
------------------------------

AudioMixer.auf を **exedit.auf と同じ場所** に置いてください。

アンインストールは AudioMixer.auf を削除すれば完了です。

使い方
------

このプラグインを導入すると、`チャンネルストリップ` や `チャンネルストリップ - Aux` というオブジェクトが使えるようになります。

![チャンネルストリップの配置サンプル](images/timeline.png)

これらのオブジェクトには `ID` というパラメーターがあり、これを `0` 以上に設定することで有効化できます。

チャンネルストリップは、それよりも上に置かれた音声オブジェクトが処理対象になります。  
上記スクリーンショットでは `ID` `0` の `チャンネルストリップ` は `BGM` を管理し、`ID` `1` の `チャンネルストリップ` は `ボイス1` と `ボイス2` を管理しています。

上記スクリーンショットでは `ID` `0` の `チャンネルストリップ - Aux` も配置していますが、これだけではまだ機能しません。  
この状態で `チャンネルストリップ` 側で `Aux ID` を `0` に設定し、更に `Aux Send` で送る音量を設定することで初めて機能します。

`管轄外` にある音声は、チャンネルストリップでは管理されていません。

### チャンネルストリップ

拡張編集タイムラインの右クリックメニューから
`［フィルタオブジェクトの追加］→［チャンネルストリップ］` を選ぶと、
タイムライン上に `チャンネルストリップ` オブジェクトが配置できます。

![チャンネルストリップのスクリーンショット](images/cs.png)

接頭辞 EQ が付くものはイコライザー、C が付くものはコンプレッサー用のパラメーター、Aux はセンドエフェクトのためのパラメーターです。

プロパティ名|説明
---|---
`ID`|`チャンネルストリップ` を識別する番号を指定します。<br>`チャンネルストリップ` を使用する時は、同じ時間軸で重複しない `0` 以上の `ID` を指定する必要があります。<br>つまり縦に `ID: 0` が２つあるのはまずいですが、横に `ID: 0` が２つあるのは問題ありません。
`入力音量`|エフェクターに通す前の音量を指定します。
`遅延`|音声を遅らせる時間をミリ秒でします。
`EQ LoFreq`|イコライザーで音量調整したい周波数帯を指定します。
`EQ LoGain`|`EQ LoFreq` で指定した周波数帯の音量を調整します。
`EQ HiFreq`|イコライザーで音量調整したい周波数帯を指定します。
`EQ HiGain`|`EQ HiFreq` で指定した周波数帯の音量を調整します。
`C Thresh`|コンプレッサーのスレッショルド（どの程度の大きさを超えたらコンプレッサーで圧縮するか）を指定します。
`C Ratio`|コンプレッサーのレシオ（どの程度圧縮するか）を指定します。
`C Attack`|コンプレッサーのアタック（どのぐらいの速さで音量が圧縮されるか）を指定します。
`C Release`|コンプレッサーのリリース（どのぐらいの速さで音量が戻るか）を指定します。
`Aux ID`|送り先の `チャンネルストリップ - Aux` の ID を指定します。
`Aux Send`|`チャンネルストリップ - Aux`に送る音の大きさを指定します。
`出力音量`|エフェクターに通した後の音量を指定します。
`左右`|左右の定位を指定します。

### チャンネルストリップ - Aux

拡張編集タイムラインの右クリックメニューから
`［フィルタオブジェクトの追加］→［チャンネルストリップ - Aux］` を選ぶと、
タイムライン上に `チャンネルストリップ - Aux` オブジェクトが配置できます。

![チャンネルストリップ - Aux のスクリーンショット](images/csaux.png)

接頭辞 R が付くものはリバーブ用のパラメーターです。

プロパティ名|説明
---|---
`ID`|`チャンネルストリップ - Aux` を識別する番号を指定します。<br>`チャンネルストリップ - Aux` を使用する時は、同じ時間軸で重複しない `0` 以上の `ID` を指定する必要があります。<br>つまり縦に `ID: 0` が２つあるのはまずいですが、横に `ID: 0` が２つあるのは問題ありません。
`R PreDly`|リバーブの残響が始まるまでの時間です。
`R LPF`|リバーブの残響に対するローパスフィルターです。
`R Diffuse`|リバーブの残響が拡散していく強さです。
`R Decay`|リバーブの残響が収束していく速さです。<br>値を大きくすると収束しないため、動画を最初から再生した場合と途中から再生した場合で残響の残り方に違いが現れます。
`R Damping`|リバーブの拡散が収束していく速さです。
`R Excursion`|リバーブの残響に少しうねりを加えます。
`R Wet`|リバーブの音量です。

#### 制限事項

`音声波形表示` のような音声データにアクセスする必要がある一部の機能のうち、音声ファイルを指定するのではなく、
すべての音声を対象とするような使い方と Aux は相性が悪く、エフェクトの掛かり方がおかしくなります。

これはバグではなく仕様です。
ただし、この場合でもパラアウトは正しく動作するはずです。

### チャンネルストリップ - パラアウト

AviUtl のメインメニューから `［ファイル］→［エクスポート］→［チャンネルストリップ - パラアウト］` を選ぶと、
個々のチャンネルストリップの音声を別々の音声ファイルに保存できる「パラアウト」機能を使うことができます。

![パラアウトのダイアログ](images/parallel-output.png)

出力先などを設定してエクスポートボタンを押すと、選択しているフレームの区間にあるチャンネルストリップの音声をそれぞれ別々のファイルに書き出されます。

例えば選択しているフレームの区間に `チャンネルストリップ` の `ID` `0` と `ID` `1`、`チャンネルストリップ - Aux` の `ID` `0` がある場合は、以下のファイルが作成されます。

- `1920x1080_60fps_48000Hz.wav` - 管轄外の音声
- `1920x1080_60fps_48000Hz_0.wav` - `チャンネルストリップ` `ID` `0` の音声
- `1920x1080_60fps_48000Hz_1.wav` - `チャンネルストリップ` `ID` `1` の音声
- `1920x1080_60fps_48000Hz_Aux0.wav` - `チャンネルストリップ - Aux` `ID` `0` の音声

`管轄外の音声` とは、チャンネルストリップで管理していないすべての音声を指します。

ビルドについて
--------------

[MSYS2](https://www.msys2.org/) + CLANG32 上で開発し、リリース用ファイルは GitHub Actions にて生成しています。  
ビルド方法や必要になるパッケージなどは [GitHub Actions の設定ファイル](https://github.com/oov/aviutl_audiomixer/blob/main/.github/workflows/releaser.yml) を参照してください。

Credits
-------

AudioMixer.auf is made possible by the following open source softwares.

### [Acutest](https://github.com/mity/acutest)

<details>
<summary>The MIT License</summary>

```
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
```
</details>

### [AviUtl Plugin SDK](http://spring-fragrance.mints.ne.jp/aviutl/)

<details>
<summary>The MIT License</summary>

```
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
```
</details>

### [hashmap.c](https://github.com/tidwall/hashmap.c)

NOTICE: This program used [a modified version of hashmap.c](https://github.com/oov/hashmap.c/tree/simplify).

<details>
<summary>The MIT License</summary>

```
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
```
</details>

### [mda VST plug-ins](https://sourceforge.net/projects/mda-vst/)

<details>
<summary>The MIT License</summary>

```
mda VST plug-ins

Copyright (c) 2008 Paul Kellett

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
</details>

### RBJ Audio EQ Cookbook Filters

<details>
http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt  
http://www.musicdsp.org/showone.php?id=128  
http://www.musicdsp.org/showone.php?id=225
</details>

### [TinyCThread](https://github.com/tinycthread/tinycthread)

NOTICE: This program used [a modified version of TinyCThread](https://github.com/oov/tinycthread).

<details>
<summary>The zlib/libpng License</summary>

```
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
```
</details>

### [UXFDReverb](https://github.com/khoin/UXFDReverb)

<details>
<summary>Public Domain + No-Liability</summary>

```
In jurisdictions that recognize copyright laws, this software is to
be released into the public domain.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
THE AUTHOR(S) SHALL NOT BE LIABLE FOR ANYTHING, ARISING FROM, OR IN
CONNECTION WITH THE SOFTWARE OR THE DISTRIBUTION OF THE SOFTWARE.
```
</details>

## 更新履歴

CHANGELOG を参照してください。

https://github.com/oov/aviutl_audiomixer/blob/main/CHANGELOG.md
