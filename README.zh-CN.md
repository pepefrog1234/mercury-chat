# Mercury Chat

[繁体中文](README.md)

Mercury Chat 是一款面向 Mercury HF 调制解调器的 Qt/C++ 聊天程序。它会把调制解调器作为独立进程启动，再通过 Mercury 的 VARA 兼容 TCP TNC 接口控制连接、信标和聊天数据。

这个程序主要用于短波数字通信测试与操作，支持：

- 通过 Mercury ARQ 连接进行 UTF-8 文本聊天。
- 显示对方呼号、连接时长、带宽、SNR、TX/RX 速率、PTT 和缓冲区状态。
- 接收与发送信标，信标列表会显示最后听到时间和近期 SNR。
- 消息队列：传输中再次发送新消息时，会按顺序排队。
- 接收端可以在完整消息解码完成前预览已经收到的文本。
- 可选的“正在输入”状态数据包，默认启用。
- SQLite 记录信标和聊天记录；再次连接同一位操作员时会加载历史会话。
- 可用 hamlib CAT 控制电台、读写频率，并跟随调制解调器 PTT。

## 系统要求

### Windows

建议使用 GitHub Actions 生成的 Windows 软件包：

- `mercury-chat-windows-x64`
- `mercury-chat-windows-arm64`

本仓库 GitHub Actions 生成的 Windows 软件包会包含聊天程序、Qt 运行时文件、Hamlib CAT 工具，以及我们 fork 修改过的 Mercury 调制解调器，也就是同一个文件夹内应该会有 `mercury-chat.exe` 和 `mercury.exe`，并且 `hamlib/bin` 内会有 `rigctl.exe` 和 `rigctld.exe`。如果你拿到的 Windows 软件包没有包含 `mercury.exe`，就必须另外安装我们修改过的 Mercury 调制解调器，并在 `Modem` 选项卡把 `Executable` 指向该可执行文件；只安装聊天程序本身会无法正常收发。

Windows ARM64 软件包中的聊天程序是 ARM64 版本；目前内置的 Mercury 调制解调器和 Hamlib CAT 工具是 x64 版本，会通过 Windows 11 ARM64 的 x64 兼容层运行。

### Linux Debian 13.5 amd64

GitHub Actions 也会生成：

- `mercury-chat-linux-debian13.5-amd64`

这个 Linux 软件包目前只包含 Mercury Chat 主程序，不包含 Mercury 调制解调器。请另外安装或编译 fork 版 Mercury 调制解调器，并在程序的 `Modem` 选项卡配置 `Executable` 路径。若要在 Linux 软件包使用 CAT，请另外安装 Hamlib 的 `rigctl` / `rigctld` 工具，并让它们出现在 `PATH` 中。

Debian 13.5 上运行聊天程序所需的 Qt 运行时软件包：

```sh
sudo apt-get install libqt6core6t64 libqt6gui6 libqt6multimedia6 libqt6network6 libqt6sql6 libqt6sql6-sqlite libqt6widgets6
```

### 音频设备

一般操作时，电脑需要能把 Mercury 调制解调器的音频送到电台，并从电台收回音频。常见做法如下：

- 实体电台声卡：选择电台提供的 USB 音频输入/输出设备。
- 同一台电脑回环测试：使用 BlackHole、VB-CABLE、PipeWire/JACK 或其他虚拟音频线。
- macOS 可使用 CoreAudio；Windows 可用 WASAPI 或 DirectSound；Linux 可用 ALSA 或 PulseAudio。

Mercury 调制解调器内部使用 8000 Hz 音频；如果音频设备是 48000 Hz 或其他采样率，fork 版 Mercury 会做重采样。

## 获取构建产物

在 GitHub 网页进入本仓库的 `Actions` 页面，选择最新成功的工作流：

- `Windows packages`：下载 Windows x64/ARM64 软件包。
- `Linux Debian packages`：下载 Debian 13.5 amd64 软件包。

下载后解压。Windows 用户运行 `mercury-chat.exe`；Linux 用户运行 `./mercury-chat`。Windows 用户若发现文件夹内没有 `mercury.exe`，请另外下载或编译我们 fork 的 Mercury 调制解调器，否则 Mercury Chat 只能打开界面，无法启动底层调制解调器完成通信。Windows 用户若要使用 CAT，请确认软件包内有 `hamlib/bin/rigctld.exe`。

## 首次启动

首次启动时，建议按这个顺序配置。

1. 打开 `Modem` 选项卡。
2. 确认 `Executable` 指向 Mercury 调制解调器可执行文件。
3. 勾选或取消 `Start on app launch`。勾选时，程序启动后会自动启动调制解调器。
4. 配置 `Broadcast port`。默认为 `8100`。
5. 如果需要额外 Mercury 参数，填写 `Extra args`。
6. 在 `Audio I/O` 配置音频系统、输入设备、输出设备和 RX channel。
7. 打开 `Station` 选项卡，填写自己的呼号。
8. 确认 `Mercury host` 为 `127.0.0.1`，除非你要连接到另一台电脑上的 Mercury TNC。
9. 确认 `ARQ base port`。默认为 `8300`。
10. 选择 ARQ 带宽：500 Hz、2300 Hz 或 2750 Hz。
11. 点击 `Start Modem` 启动调制解调器。
12. 点击 `Connect TNC` 连接调制解调器的 TNC 控制端口和数据端口。

TNC 连接后，程序会自动应用本机呼号、带宽、聊天模式和接收呼叫状态。必要时也可以点击 `Initialize` 手动重新应用台站配置。

## Modem 选项卡

### Mercury Modem

- `Executable`：Mercury 调制解调器可执行文件路径。Windows Actions 软件包通常已经把 fork 版 `mercury.exe` 放在同一个文件夹内；若没有，必须另外安装我们修改过的 Mercury 调制解调器。Linux 需要自行指定 fork 版 `mercury`，或让它出现在 `PATH` 中。
- `Broadcast port`：广播/信标 TCP 端口。A/B 测试时两个实例不能使用同一个端口。
- `Extra args`：额外传给 Mercury 的命令行参数。
- `Start on app launch`：启动 Mercury Chat 时自动启动调制解调器。
- `Start Modem` / `Stop Modem`：手动启动或停止调制解调器。

当你点击 `Start Modem` 时，Mercury Chat 会按照这个选项卡和 `Station` 选项卡的配置启动 Mercury 调制解调器，包括 ARQ 基础端口、广播端口、音频系统、输入/输出音频设备和接收声道。普通用户只需要在界面内调整字段，不需要手动输入 Mercury 启动命令。

### Audio I/O

- `Sound system`：选择 `Auto`、`CoreAudio`、`ALSA`、`PulseAudio`、`DirectSound`、`WASAPI` 等音频系统。
- `Input device`：接收音频来源，也就是电台或回环线送进电脑的声音。
- `Output device`：发射音频目的地，也就是电脑送往电台或回环线的声音。
- `RX channel`：选择 `Left`、`Right` 或 `Stereo`。

如果指定的 Mercury 调制解调器可以运行，Mercury Chat 会尝试读取 Mercury 返回的音频设备列表，让下拉菜单使用 Mercury 实际接受的设备 ID。读不到时会退回 Qt 检测到的操作系统音频设备名称。

## Station 选项卡

- `Callsign`：自己的呼号。程序会转换为大写，并保留英数字、`-` 和 `/`。
- `Mercury host`：TNC 主机地址。本机通常是 `127.0.0.1`。
- `ARQ base port`：TNC 控制端口。数据端口会使用下一个端口。例如基础端口为 `8300` 时，控制端口是 `8300`，数据端口是 `8301`。
- `Bandwidth`：传给 Mercury 的 ARQ 带宽配置。
- `Connect TNC`：连接或断开 TNC。
- `Initialize`：重新发送台站配置。
- `Disconnect Link`：断开当前 ARQ 连接。

## Beacons 选项卡

信标用来让其他台站知道你在线，也可以用来找到可连接的操作员。

- `Send Beacon`：发送一次信标。
- `Auto`：按配置间隔自动发送信标。
- 间隔字段：自动信标间隔，范围为 30 到 3600 秒。
- `Connect to`：手动输入对方呼号。
- `Connect`：呼叫 `Connect to` 中的呼号。
- 信标列表：显示 `Call`、`BW`、`SNR`、`Last heard`。
- `Connect Selected`：连接到表格中选中的台站。

收到信标时，程序会把对方呼号、带宽、最后收到时间和近期 SNR 显示在信标列表。也会写入 SQLite 数据库。

ARQ 连接中不会让你发送信标，`Send Beacon` 和 `Auto` 会停用，避免在连接中额外占用信道。

## Chat 选项卡

`Chat` 选项卡是主要聊天窗口。

- 上方大文本区：显示系统事件、历史聊天记录和当前聊天内容。
- `输入消息：` 下方文本框：输入要发送的 UTF-8 文本。
- `Send typing indicator`：默认勾选。你输入文本时，最多每 15 秒发送一次很小的状态数据包，让对方看到正在输入。
- 下方状态栏：显示 `Idle`、TX/RX 进度、队列数量，以及当前 TX/RX 速率。
- `Send`：发送消息。

快捷操作：

- 按 `Enter` 发送消息。
- 若要在文本框内换行，使用带修饰键的 Enter，例如 `Shift+Enter`。

传输规则：

- 如果你在上一条消息还没传完时又发送新消息，程序会放入队列，等前一条完成后按顺序发送。
- 如果正在接收对方消息，本机发送的消息会先排队，等接收完成后再发送。
- 接收端在知道数据包长度后会显示 RX 进度。
- 对方发送未压缩文本时，接收端可以先显示已经完整解码的 UTF-8 字符，不必等整条消息完成。
- 长消息可能会压缩后发送；压缩消息必须等完整收到后才能解压显示。

连接空闲 5 分钟且双方没有互动、没有队列、没有传输中数据时，程序会请求断开 ARQ 连接。

## Modem Status 选项卡

`Modem Status` 选项卡显示两种信息：

- 上方状态摘要：调制解调器、TNC、ARQ 连接、对方呼号、连接时长、带宽、SNR、TX/RX 速率、PTT、缓冲区。
- 下方日志窗口：Mercury 调制解调器输出、TNC 控制消息、CAT 消息和错误消息。

调试时请先看这个选项卡。如果出现 `Mercury modem crashed`、`TNC control disconnected`、`Keepalive miss limit` 或 `Data retry exhausted`，通常表示底层调制解调器、音频链路或 ARQ 链路状态出了问题。

## CAT 选项卡

CAT 选项卡用 hamlib 控制电台。Windows 软件包会内置 Hamlib 工具；当你在 CAT 选项卡点击 `Connect CAT` 时，程序会在后台启动内置的 `rigctld.exe` 并通过本机 TCP 控制它。若 `CAT port` 填入 `127.0.0.1:4532` 这类地址，程序会改为连接到已有的外部 `rigctld`。

- `Radio`：选择 hamlib 支持的电台型号。
- `CAT port`：串口或 rigctld 地址，例如 `/dev/ttyUSB0`、`COM3` 或 `127.0.0.1:4532`。
- `Serial baud`：串口速率；`0` 表示使用 hamlib 默认值。
- `RTS` / `DTR`：可选择不设置、强制开启或强制关闭。
- `PTT method`：选择使用 CAT、RTS 或 DTR 控制 PTT。
- `Reconnect on app launch`：启动程序时自动重新连接 CAT。
- `Follow modem PTT`：根据 Mercury 返回的 `PTT ON` / `PTT OFF` 自动控制电台 PTT。
- `Frequency`：读取或设置频率，单位为 Hz。

请避免同时让 Mercury 调制解调器和 Mercury Chat 两边都控制 CAT/PTT。一般建议是 Mercury 不加 `-R` / `-A`，让 Mercury Chat 的 CAT 选项卡跟随 TNC PTT 状态。

## A/B 本机回环测试

本仓库提供两组独立配置文件：

- `profiles/a.ini`：呼号 `TESTA`，ARQ 基础端口 `8300`，广播端口 `8100`。
- `profiles/b.ini`：呼号 `TESTB`，ARQ 基础端口 `8400`，广播端口 `8200`。

构建后可用工具脚本启动：

```sh
tools/run-a.sh
tools/run-b.sh
```

或一次打开两个窗口：

```sh
tools/run-ab.sh
```

这两个配置文件会各自使用独立的 INI 配置文件和 SQLite 数据库：

```text
profiles/a.ini      -> profiles/a.sqlite3
profiles/b.ini      -> profiles/b.sqlite3
```

若要在同一台电脑测试音频链路，请把 A 和 B 的输入/输出音频设备接到同一组虚拟回环设备，或按你的虚拟音频线配置交叉连接。两边的 ARQ 端口和广播端口不能相同。

## 配置与数据库

未指定配置文件时，Mercury Chat 使用 Qt 的平台配置存储区，组织名称和应用程序名称为 `Rhizomatica/MercuryChat`。

若使用 `--settings-file`，则会使用指定的 INI 文件：

```sh
./mercury-chat --profile A --settings-file profiles/a.ini
./mercury-chat --profile B --settings-file profiles/b.ini
```

SQLite 数据库位置：

- 有指定 INI 文件时，数据库会放在同一个文件夹，并使用相同文件名。例如 `a.ini` 对应 `a.sqlite3`。
- 没有指定 INI 文件时，数据库会放在 Qt 的 `AppDataLocation`，文件名为 `mercury-chat.sqlite3`。

数据库会记录：

- 收到的信标事件。
- 完整收发的聊天消息。

不会写入数据库的内容：

- 接收中的局部预览文本。
- 正在输入状态数据包。
- Modem Status 选项卡中的完整调试日志。

数据库格式请见 `sql.md`。

## 从源代码编译

需要：

- CMake 3.21 或更新版本。
- Qt 6 Core、Network、Sql、Widgets、Multimedia。
- 可选：hamlib 开发包，用于直接链接 hamlib CAT 控制；若不链接 hamlib，也可以通过外部 `rigctl` / `rigctld` 工具提供 CAT。

一般编译：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

如果不需要直接链接 hamlib 开发包，可关闭 hamlib 链接；关闭后仍可通过外部 `rigctl` / `rigctld` 工具提供 CAT：

```sh
cmake -S . -B build -DMERCURYCHAT_USE_HAMLIB=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

运行：

```sh
./build/mercury-chat
```

## Mercury 调制解调器集成

Mercury Chat 使用 Mercury 的 TCP TNC 接口：

- ARQ 控制端口：默认 `8300`。
- ARQ 数据端口：默认 `8301`，也就是控制端口加 1。
- 广播/信标端口：默认 `8100`。

连接 TNC 后，Mercury Chat 会自动应用本机呼号、带宽、聊天模式和接收呼叫状态；用户可以通过界面发送信标、连接、断开和聊天消息，不需要手动输入 TNC 命令。

Mercury Chat 不会强制指定 DATAC 模式，也不会覆盖 Mercury 底层 ARQ 的自适应速率规则。它只发送用户在界面选择的带宽配置，并把 Mercury 返回的 SNR、TX/RX 速率、缓冲区和 PTT 状态作为状态信息显示。

### Fork 版调制解调器协议可能带来的优势

本项目搭配的是我们 fork 修改过的 Mercury 调制解调器。这意味着我们不只是给聊天程序外层加功能，也可以调整底层 ARQ 协议和音频处理行为。相比只使用原版调制解调器，fork 版可能带来以下优势：

- 更贴近短波实际链路的自适应速率选择：调制解调器可根据 SNR、重传情况、待发送数据量和近期 ACK 状态，在较稳定时提高数据模式，在不稳定时退回较保守模式。
- 更好的信道使用效率：ARQ 可减少不必要的等待、轮询和过早断线，让半双工收发切换更适合聊天这类间歇性数据。
- 更低的应用层开销：聊天数据使用精简长度前缀格式，长消息才视情况压缩，避免每条短消息都背负过大的格式成本。
- 更好的操作体验：接收端能显示传输进度，未压缩文本可边收边显示；空闲断线和正在输入状态也能更符合人工打字节奏。
- 更容易针对实测修正问题：音频设备采样率、重采样、Windows 音频设备名称、TX 音频输出音量等问题，都可以在 fork 版调制解调器内直接调整。

这些优势不代表在所有短波条件下都会变快；弱信号、多径、QRM、过高或过低音量、错误的音频设备配置仍会限制实际效果。若要使用 fork 协议的改善，通信双方应使用相同或兼容版本的 fork 版 Mercury 调制解调器；原版 Mercury 或其他未同步修改的调制解调器不保证能与 fork 协议互通。

## 聊天数据包格式

Mercury Chat 在 ARQ 数据端口上使用精简长度前缀格式。每个数据包先发送 ASCII 头部，声明后面载荷的字节数：

```text
MCHAT1 7
M你好

MCHAT1 1
T
```

载荷类型：

- `M`：未压缩 UTF-8 文本。
- `Z`：Qt/zlib 压缩后的 UTF-8 文本，只在长消息压缩后确实更小时使用。
- `T`：正在输入状态数据包。

接收端会先读取头部获取总长度，因此可以显示 RX 进度。未压缩文本可在接收过程中预览；压缩文本需要完整收到后才能解压。为了兼容旧版，解码器仍可接受旧的 JSON 帧和换行分隔 JSON 格式。

## 常见问题

### TNC 一直连接不上

请检查：

- Mercury 调制解调器是否已启动。
- `ARQ base port` 是否和 Mercury 的 `-p` 一致。
- 防火墙是否阻止本机 TCP 连接。
- A/B 测试时两个实例是否使用不同的 ARQ 基础端口。

### 有 PTT，但对方解不出来

请检查：

- 音频输入/输出设备是否选对。
- 操作系统音量、电台数据模式音量、ALC 是否过高或过低。
- RX channel 是否选对左右声道。
- 对方是否使用相同频率、相同边带和相同带宽配置。
- Mercury Status 是否显示合理 SNR 和 BITRATE。

### 传输速度看起来很慢

短波 ARQ 会受 SNR、重传、数据包确认、半双工切换和底层 DATAC 模式影响。即使在本机音频回环中，Mercury 仍会按 ARQ 协议轮流发送和确认，不会像普通局域网 TCP 那样连续高速传输。

### 消息发送后没有马上出现在对方窗口

如果当前正在接收对方消息，或调制解调器缓冲区还有数据，消息会先进入队列。可以查看下方进度条、`Buffer` 和 `PTT` 状态判断当前是否还在传输。

### 历史记录没有加载

历史记录是按对方呼号加载的。请确认：

- 对方呼号和上次记录一致。
- 使用的是同一个配置文件或同一个 SQLite 数据库。
- `Modem Status` 选项卡没有出现 SQLite 错误。
