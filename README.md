# Mercury Chat

[簡體中文](README.zh-CN.md)

Mercury Chat 是給 Mercury HF 數據機使用的 Qt/C++ 聊天程式。它把數據機當成獨立行程啟動，再透過 Mercury 的 VARA 相容 TCP TNC 介面控制連線、信標與聊天資料。

這個程式主要用於短波數位通訊測試與操作，支援：

- 透過 Mercury ARQ 連線進行 UTF-8 文字聊天。
- 顯示對方呼號、連線時間、頻寬、SNR、TX/RX 速率、PTT 與緩衝區狀態。
- 接收與傳送信標，信標清單會顯示最後聽到時間與近期 SNR。
- 訊息佇列：傳輸中又送出新訊息時，會依序排隊。
- 接收端可在完整訊息解完前先預覽已收到的文字。
- 可選的「正在輸入」狀態封包，預設啟用。
- SQLite 記錄信標與聊天記錄；再次連上同一位操作員時會載入歷史對話。
- 可用 hamlib CAT 控制電台、讀寫頻率與跟隨數據機 PTT。

## 系統需求

### Windows

建議使用 GitHub Actions 產生的 Windows 套件：

- `mercury-chat-windows-x64`
- `mercury-chat-windows-arm64`

本儲存庫 GitHub Actions 產生的 Windows 套件會包含聊天程式、Qt 執行時期檔案、Hamlib CAT 工具，以及我們 fork 修改過的 Mercury 數據機，也就是同一個資料夾內應該會有 `mercury-chat.exe` 與 `mercury.exe`，並且 `hamlib/bin` 內會有 `rigctl.exe` 與 `rigctld.exe`。如果你拿到的 Windows 套件沒有內含 `mercury.exe`，就必須另外安裝我們修改過的 Mercury 數據機，並在 `Modem` 分頁把 `Executable` 指到該執行檔；只安裝聊天程式本體會無法正常收發。

Windows ARM64 套件中的聊天程式是 ARM64 版本；目前內含的 Mercury 數據機與 Hamlib CAT 工具是 x64 版本，會透過 Windows 11 ARM64 的 x64 相容層執行。

### Linux Debian 13.5 amd64

GitHub Actions 也會產生：

- `mercury-chat-linux-debian13.5-amd64`

這個 Linux 套件目前只包含 Mercury Chat 主程式，不包含 Mercury 數據機。請另外安裝或編譯 fork 版 Mercury 數據機，並在程式的 `Modem` 分頁設定 `Executable` 路徑。若要在 Linux 套件使用 CAT，請另外安裝 Hamlib 的 `rigctl` / `rigctld` 工具，並讓它們出現在 `PATH`。

Debian 13.5 上執行聊天程式所需的 Qt 執行時期套件：

```sh
sudo apt-get install libqt6core6t64 libqt6gui6 libqt6multimedia6 libqt6network6 libqt6sql6 libqt6sql6-sqlite libqt6widgets6
```

### 音效裝置

一般操作時，電腦需要能把 Mercury 數據機的音訊送到電台，並從電台收回音訊。常見做法如下：

- 實體電台音效卡：選擇電台提供的 USB 音效輸入/輸出裝置。
- 同台電腦回環測試：使用 BlackHole、VB-CABLE、PipeWire/JACK 或其他虛擬音效線。
- macOS 可使用 CoreAudio；Windows 可用 WASAPI 或 DirectSound；Linux 可用 ALSA 或 PulseAudio。

Mercury 數據機內部使用 8000 Hz 音訊；如果音效裝置是 48000 Hz 或其他取樣率，fork 版 Mercury 會做重取樣。

## 取得建置成品

在 GitHub 網頁進入本儲存庫的 `Actions` 頁面，選擇最新成功的工作流程：

- `Windows packages`：下載 Windows x64/ARM64 套件。
- `Linux Debian packages`：下載 Debian 13.5 amd64 套件。

下載後解壓縮。Windows 使用者執行 `mercury-chat.exe`；Linux 使用者執行 `./mercury-chat`。Windows 使用者若發現資料夾內沒有 `mercury.exe`，請另外下載或編譯我們 fork 的 Mercury 數據機，否則 Mercury Chat 只能開啟介面，無法啟動底層數據機完成通訊。Windows 使用者若要使用 CAT，請確認套件內有 `hamlib/bin/rigctld.exe`。

## 第一次啟動

第一次啟動時，建議依照這個順序設定。

1. 開啟 `Modem` 分頁。
2. 確認 `Executable` 指向 Mercury 數據機執行檔。
3. 勾選或取消 `Start on app launch`。勾選時，程式啟動後會自動啟動數據機。
4. 設定 `Broadcast port`。預設為 `8100`。
5. 如果需要額外 Mercury 參數，填入 `Extra args`。
6. 在 `Audio I/O` 設定音效系統、輸入裝置、輸出裝置與 RX channel。
7. 開啟 `Station` 分頁，填入自己的呼號。
8. 確認 `Mercury host` 為 `127.0.0.1`，除非你要連到別台電腦上的 Mercury TNC。
9. 確認 `ARQ base port`。預設為 `8300`。
10. 選擇 ARQ 頻寬：500 Hz、2300 Hz 或 2750 Hz。
11. 按 `Start Modem` 啟動數據機。
12. 按 `Connect TNC` 連上數據機的 TNC 控制埠與資料埠。

TNC 連上後，程式會自動套用本機呼號、頻寬、聊天模式與接收呼叫狀態。必要時也可以按 `Initialize` 手動重新套用站台設定。

## Modem 分頁

### Mercury Modem

- `Executable`：Mercury 數據機執行檔路徑。Windows Actions 套件通常已經把 fork 版 `mercury.exe` 放在同一個資料夾內；若沒有，必須另外安裝我們修改過的 Mercury 數據機。Linux 需要自行指定 fork 版 `mercury`，或讓它出現在 `PATH`。
- `Broadcast port`：廣播/信標 TCP 連接埠。A/B 測試時兩個實例不能使用同一個連接埠。
- `Extra args`：額外傳給 Mercury 的命令列參數。
- `Start on app launch`：啟動 Mercury Chat 時自動啟動數據機。
- `Start Modem` / `Stop Modem`：手動啟動或停止數據機。

當你按 `Start Modem` 時，Mercury Chat 會依照這個分頁與 `Station` 分頁的設定啟動 Mercury 數據機，包括 ARQ 基礎連接埠、廣播連接埠、音效系統、輸入/輸出音效裝置與接收聲道。一般使用者只需要在介面內調整欄位，不需要手動輸入 Mercury 啟動指令。

### Audio I/O

- `Sound system`：選擇 `Auto`、`CoreAudio`、`ALSA`、`PulseAudio`、`DirectSound`、`WASAPI` 等音效系統。
- `Input device`：接收音訊來源，也就是電台或回環線送進電腦的聲音。
- `Output device`：發射音訊目的地，也就是電腦送往電台或回環線的聲音。
- `RX channel`：選擇 `Left`、`Right` 或 `Stereo`。

如果指定的 Mercury 數據機可以執行，Mercury Chat 會嘗試讀取 Mercury 回報的音效裝置清單，讓下拉選單使用 Mercury 實際接受的裝置 ID。讀不到時會退回 Qt 偵測到的作業系統音效裝置名稱。

## Station 分頁

- `Callsign`：自己的呼號。程式會轉成大寫，並保留英數字、`-` 與 `/`。
- `Mercury host`：TNC 主機位址。本機通常是 `127.0.0.1`。
- `ARQ base port`：TNC 控制埠。資料埠會使用下一個連接埠。例如基礎連接埠為 `8300` 時，控制埠是 `8300`，資料埠是 `8301`。
- `Bandwidth`：傳給 Mercury 的 ARQ 頻寬設定。
- `Connect TNC`：連接或中斷 TNC。
- `Initialize`：重新送出站台設定。
- `Disconnect Link`：中斷目前 ARQ 連線。

## Beacons 分頁

信標用來讓其他站台知道你在線上，也可以用來找到可連線的操作員。

- `Send Beacon`：送出一次信標。
- `Auto`：依設定間隔自動送信標。
- 間隔欄位：自動信標間隔，範圍為 30 到 3600 秒。
- `Connect to`：手動輸入對方呼號。
- `Connect`：呼叫 `Connect to` 裡的呼號。
- 信標清單：顯示 `Call`、`BW`、`SNR`、`Last heard`。
- `Connect Selected`：連線到表格中選取的站台。

收到信標時，程式會把對方呼號、頻寬、最後收到時間與近期 SNR 顯示在信標清單。也會寫入 SQLite 資料庫。

ARQ 連線中不會讓你送信標，`Send Beacon` 與 `Auto` 會停用，避免在連線中額外佔用通道。

## Chat 分頁

`Chat` 分頁是主要聊天視窗。

- 上方大文字區：顯示系統事件、歷史聊天記錄與目前聊天內容。
- `輸入訊息：` 下方文字框：輸入要傳送的 UTF-8 文字。
- `Send typing indicator`：預設勾選。你輸入文字時，最多每 15 秒送出一次很小的狀態封包，讓對方看到正在輸入。
- 下方狀態列：顯示 `Idle`、TX/RX 進度、佇列數量，以及目前 TX/RX 速率。
- `Send`：送出訊息。

快捷操作：

- 按 `Enter` 送出訊息。
- 若要在文字框內換行，使用帶修飾鍵的 Enter，例如 `Shift+Enter`。

傳輸規則：

- 如果你在上一則訊息還沒傳完時又送出新訊息，程式會排入佇列，等前一則完成後依序傳送。
- 如果正在接收對方訊息，本機送出的訊息會先排隊，等接收完成後再送。
- 接收端在知道封包長度後會顯示 RX 進度。
- 對方傳送未壓縮文字時，接收端可以先顯示已完整解出的 UTF-8 字元，不必等整則訊息完成。
- 長訊息可能會壓縮後傳送；壓縮訊息必須等完整收到後才能解壓縮顯示。

連線閒置 5 分鐘且雙方沒有互動、沒有佇列、沒有傳輸中資料時，程式會要求中斷 ARQ 連線。

## Modem Status 分頁

`Modem Status` 分頁顯示兩種資訊：

- 上方狀態摘要：數據機、TNC、ARQ 連線、對方呼號、連線時長、頻寬、SNR、TX/RX 速率、PTT、緩衝區。
- 下方記錄視窗：Mercury 數據機輸出、TNC 控制訊息、CAT 訊息與錯誤訊息。

除錯時請先看這個分頁。如果出現 `Mercury modem crashed`、`TNC control disconnected`、`Keepalive miss limit` 或 `Data retry exhausted`，通常表示底層數據機、音訊鏈路或 ARQ 鏈路狀態出了問題。

## CAT 分頁

CAT 分頁用 hamlib 控制電台。Windows 套件會內含 Hamlib 工具；當你在 CAT 分頁按 `Connect CAT` 時，程式會在背景啟動 bundled `rigctld.exe` 並透過本機 TCP 控制它。若 `CAT port` 填入 `127.0.0.1:4532` 這類位址，程式會改連線到既有的外部 `rigctld`。

- `Radio`：選擇 hamlib 支援的電台型號。
- `CAT port`：序列埠或 rigctld 位址，例如 `/dev/ttyUSB0`、`COM3` 或 `127.0.0.1:4532`。
- `Serial baud`：序列埠速率；`0` 表示使用 hamlib 預設值。
- `RTS` / `DTR`：可選擇不設定、強制開啟或強制關閉。
- `PTT method`：選擇使用 CAT、RTS 或 DTR 控制 PTT。
- `Reconnect on app launch`：啟動程式時自動重新連接 CAT。
- `Follow modem PTT`：根據 Mercury 回報的 `PTT ON` / `PTT OFF` 自動控制電台 PTT。
- `Frequency`：讀取或設定頻率，單位為 Hz。

請避免同時讓 Mercury 數據機與 Mercury Chat 兩邊都控制 CAT/PTT。一般建議是 Mercury 不加 `-R` / `-A`，讓 Mercury Chat 的 CAT 分頁跟隨 TNC PTT 狀態。

## A/B 本機回環測試

本儲存庫提供兩組獨立設定檔：

- `profiles/a.ini`：呼號 `TESTA`，ARQ 基礎連接埠 `8300`，廣播連接埠 `8100`。
- `profiles/b.ini`：呼號 `TESTB`，ARQ 基礎連接埠 `8400`，廣播連接埠 `8200`。

建置後可用工具腳本啟動：

```sh
tools/run-a.sh
tools/run-b.sh
```

或一次開兩個視窗：

```sh
tools/run-ab.sh
```

這兩個設定檔會各自使用獨立的 INI 設定檔與 SQLite 資料庫：

```text
profiles/a.ini      -> profiles/a.sqlite3
profiles/b.ini      -> profiles/b.sqlite3
```

若要在同一台電腦測試音訊鏈路，請把 A 與 B 的輸入/輸出音效裝置接到同一組虛擬回環裝置，或依你的虛擬音效線設定交叉連接。兩邊的 ARQ 連接埠和廣播連接埠不能相同。

## 設定與資料庫

未指定設定檔時，Mercury Chat 使用 Qt 的平台設定儲存區，組織名稱與應用程式名稱為 `Rhizomatica/MercuryChat`。

若使用 `--settings-file`，則會使用指定的 INI 檔：

```sh
./mercury-chat --profile A --settings-file profiles/a.ini
./mercury-chat --profile B --settings-file profiles/b.ini
```

SQLite 資料庫位置：

- 有指定 INI 檔時，資料庫會放在同一個資料夾，並使用相同檔名。例如 `a.ini` 對應 `a.sqlite3`。
- 沒有指定 INI 檔時，資料庫會放在 Qt 的 `AppDataLocation`，檔名為 `mercury-chat.sqlite3`。

資料庫會記錄：

- 收到的信標事件。
- 完整收發的聊天訊息。

不會寫入資料庫的內容：

- 接收中的局部預覽文字。
- 正在輸入狀態封包。
- Modem Status 分頁中的完整除錯記錄。

資料庫格式請見 `sql.md`。

## 從原始碼編譯

需要：

- CMake 3.21 或更新版本。
- Qt 6 Core、Network、Sql、Widgets、Multimedia。
- 選用：hamlib 開發套件，用於直接連結 hamlib CAT 控制；若不連結 hamlib，也可以透過外部 `rigctl` / `rigctld` 工具提供 CAT。

一般編譯：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

如果不需要直接連結 hamlib 開發套件，可關閉 hamlib 連結；關閉後仍可透過外部 `rigctl` / `rigctld` 工具提供 CAT：

```sh
cmake -S . -B build -DMERCURYCHAT_USE_HAMLIB=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

執行：

```sh
./build/mercury-chat
```

## Mercury 數據機整合

Mercury Chat 使用 Mercury 的 TCP TNC 介面：

- ARQ 控制埠：預設 `8300`。
- ARQ 資料埠：預設 `8301`，也就是控制埠加 1。
- 廣播/信標埠：預設 `8100`。

連上 TNC 後，Mercury Chat 會自動套用本機呼號、頻寬、聊天模式與接收呼叫狀態；使用者可以透過介面送出信標、連線、斷線與聊天訊息，不需要手動輸入 TNC 指令。

Mercury Chat 不會強制指定 DATAC 模式，也不會覆蓋 Mercury 底層 ARQ 的自適應速率規則。它只傳送使用者在介面選擇的頻寬設定，並把 Mercury 回報的 SNR、TX/RX 速率、緩衝區與 PTT 狀態當成狀態資訊顯示。

## 聊天封包格式

Mercury Chat 在 ARQ 資料埠上使用精簡長度前綴格式。每個封包先送 ASCII 標頭，宣告後面酬載的位元組數：

```text
MCHAT1 7
M你好

MCHAT1 1
T
```

酬載類型：

- `M`：未壓縮 UTF-8 文字。
- `Z`：Qt/zlib 壓縮後的 UTF-8 文字，只在長訊息壓縮後確實比較小時使用。
- `T`：正在輸入狀態封包。

接收端會先讀標頭取得總長度，因此可以顯示 RX 進度。未壓縮文字可在收到過程中預覽；壓縮文字需完整收到後才能解壓縮。為了相容舊版，解碼器仍可接受舊的 JSON 訊框與換行分隔 JSON 格式。

## 常見問題

### TNC 一直連不上

請檢查：

- Mercury 數據機是否已啟動。
- `ARQ base port` 是否和 Mercury 的 `-p` 一致。
- 防火牆是否阻擋本機 TCP 連線。
- A/B 測試時兩個實例是否使用不同的 ARQ 基礎連接埠。

### 有 PTT，但對方解不出來

請檢查：

- 音效輸入/輸出裝置是否選對。
- 作業系統音量、電台資料模式音量、ALC 是否過高或過低。
- RX channel 是否選對左右聲道。
- 對方是否使用相同頻率、相同邊帶與相同頻寬設定。
- Mercury Status 是否顯示合理 SNR 與 BITRATE。

### 傳輸速度看起來很慢

短波 ARQ 會受 SNR、重送、封包確認、半雙工切換與底層 DATAC 模式影響。即使在本機音訊回環中，Mercury 仍會依 ARQ 協定輪流發送與確認，不會像一般區域網路 TCP 那樣連續高速傳輸。

### 訊息送出後沒有馬上出現在對方視窗

如果目前正在接收對方訊息，或數據機緩衝區還有資料，訊息會先排入佇列。可以看下方進度列、`Buffer` 與 `PTT` 狀態判斷目前是否還在傳輸。

### 歷史紀錄沒有載入

歷史紀錄是依對方呼號載入的。請確認：

- 對方呼號和上次記錄一致。
- 使用的是同一個設定檔或同一個 SQLite 資料庫。
- `Modem Status` 分頁沒有出現 SQLite 錯誤。
