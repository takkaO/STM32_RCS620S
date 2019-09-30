# RC-S620S on STM32

## Description
FeLiCa（Suica）のIDmを取得し，残高を表示するプログラムです．  
[RCS620S制御ライブラリ（Arduino）](http://blog.felicalauncher.com/?p=2677)をSTM32に移植し，残高を取得するためのコードを追加しました．      

This program acquires the IDm of FeLiCa (Suica) and displays the balance.  
Ported [RCS620S control library (Arduino)](http://blog.felicalauncher.com/?p=2677) to STM32 and added code to get balance.  

## Pin assign
|Pin|Peripheral|
|-|-|
|PD8|USART3_TX (PC, For debug)|
|PA2|USART2_TX (RCS620S_RX)|
|PA3|USART2_RX (RCS620S_TX)|
|3.3V|RCS620S VCC|
|GND|RCS620S_GND|