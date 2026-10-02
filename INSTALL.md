# Installation / 安装说明

## English

**Heaven Saw Synth v1.2.1 · macOS 12+ · Apple Silicon arm64 · VST3 instrument**

1. Quit your DAW before installing or replacing the plugin.
2. Unzip `Heaven-Saw-Synth-v1.2.1-macOS-arm64-VST3.zip`.
3. Copy the entire `Heaven Saw Synth.vst3` bundle to `/Library/Audio/Plug-Ins/VST3/` (all users, administrator access may be needed), or `~/Library/Audio/Plug-Ins/VST3/` (your account). Create the VST3 folder if it is missing.
4. Keep one active copy when updating, then restart the DAW and rescan VST3 plugins if needed.
5. Add the instrument to a MIDI track and confirm `v1.2.1` in the header. Start with INIT - Heaven Saw or DROP - Festival Wall.

Play complete chords through MIDI. Automatic CHORD mode was removed in 1.2.0; older projects using it need their full chord notes in the DAW. Other sound settings are retained.

PUMP follows DAW tempo and playback position. Set PUMP to zero when comparing static tones. SAVE writes `.heavensaw` preset files; the gear menu loads them. The gear menu also offers interface size settings.

The build is locally signed and not notarized. It is a development release for DAW testing. If the plugin does not load, report your macOS version, DAW version, and the displayed error in the repository's Issues section.

Optional download integrity check, run in the folder containing the release files:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

## 简体中文

**Heaven Saw Synth v1.2.1 · macOS 12+ · Apple Silicon arm64 · VST3 乐器**

1. 安装或替换插件前退出 DAW。
2. 解压 `Heaven-Saw-Synth-v1.2.1-macOS-arm64-VST3.zip`。
3. 将完整的 `Heaven Saw Synth.vst3` 插件包复制到 `/Library/Audio/Plug-Ins/VST3/`（所有用户，可能需要管理员验证），或 `~/Library/Audio/Plug-Ins/VST3/`（当前账户）。目录不存在时先创建。
4. 更新时保留一份有效插件，然后重新启动 DAW，必要时重新扫描 VST3。
5. 在 MIDI 轨道加载插件，确认界面显示 `v1.2.1`。建议先试 INIT - Heaven Saw 或 DROP - Festival Wall。

请通过 MIDI 弹奏完整和弦。1.2.0 已删除自动 CHORD 模式，原来依赖该模式的项目需要在 DAW 中补上完整和弦音符；其他音色设置保留。

PUMP 跟随 DAW 速度和播放位置。比较静态音色时可设为零。SAVE 保存 `.heavensaw` 个人预设，齿轮菜单负责载入预设，也提供界面尺寸选项。

下载版使用本机签名，尚未公证，供 DAW 测试。如无法加载，请在仓库 Issues 中提供 macOS 版本、DAW 版本和报错信息。

可在包含发布文件的目录中校验下载文件：

```sh
shasum -a 256 -c SHA256SUMS.txt
```

## Licence / 许可证

Heaven Saw Synth is released under AGPLv3, version 3 only. Source code and build instructions are available at https://github.com/RSK21X/heaven-saw-synth and in the source release asset. See LICENSE and THIRD_PARTY_NOTICES.md for the terms and dependency notices.

Heaven Saw Synth 使用 AGPLv3，仅第 3 版。源代码与构建说明见上述仓库及发布页附带的源码包；许可条款和依赖说明见 LICENSE 与 THIRD_PARTY_NOTICES.md。
