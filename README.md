# TS-switching-detector<br>
streamlinkでAbemaの生放送を録画してできてしまった扱いづらいファイルを番組内容が切り替わるタイミングで切り分けて出力します。<br>
(検知できるもの)本編->広告、広告->本編、第一話->第二話、アニメA->アニメB<br>

## 必要なもの<br>
* このソフト<br>
* お好きなファイル結合ソフト<br>
   (コマンドが使える方はそちらも可)<br>
   (ex)(windows->copy linux->cat)<br>

## 使い方<br>
1.  録画したtsファイルをドラッグアンドドロップ<br>

2.  同じフォルダに<br>
    "TS_Video_01-01.ts", "TS_Video_01-02.ts", "TS_Video_02-01.ts", "TS_Video_02-02.ts"......<br>
    という風にファイルが生成される。<br>
    
3.  CMや録画の前後にくっついた要らない部分を消す。<br>

4.  ファイル結合ソフト(またはコマンド)で結合する。<br>
    ただし、前半の番号は混ざらないようにすること。<br>
    ||example|
    |---|---|
    |ok|Video_01-01.ts, Video_01-03.ts, Video_01-04.ts|
    |ok|Video_02-01.ts, Video_02-03.ts, Video_02-05.ts|
    |bad|Video_**01**-01.ts, Video_**01**-02.ts, Video_**02**-01.ts|
        
5.  あとは煮るなり焼くなりどうぞ。<br>
