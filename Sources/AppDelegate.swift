import Foundation
import AppKit

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate {
    private var statusItem: NSStatusItem!
    private var overlayWindow: OverlayWindow!
    private var renderTimer: Timer?
    private var windowWatchdogTimer: Timer?
    private var lastFrameTime: CFTimeInterval = CACurrentMediaTime()
    private var lastFullscreenCheckTime: CFTimeInterval = 0
    private var hiddenByFullscreen = false
    private var lastRedrawTime: CFTimeInterval = 0
    private var lastDemoKeyTime: CFTimeInterval = 0
    private var isPetVisible = true
    private var availableUpdateVersion: String?
    private var availableUpdateURL: URL?
    private var availableAssetURL: URL?
    private var isUpdating = false
    private var isHandingOverToNewerCopy = false
    private var dressingRoom: DressingRoomWindowController?

    private var currentAppVersion: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.4.0"
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        if resolveDuplicateRunningCopies() { return }

        overlayWindow = OverlayWindow()
        overlayWindow.orderFrontRegardless()

        let catView = overlayWindow.catView
        catView.onRequestPermission = { [weak self] in
            self?.openInputMonitoringSettings()
        }
        catView.onRightClickMenu = { [weak self] event in
            guard let self else { return }
            let menu = self.buildMenu()
            NSMenu.popUpContextMenu(menu, with: event, for: self.overlayWindow.catView)
        }
        catView.onDropSkinFiles = { [weak self] urls in
            for url in urls { self?.importSkin(from: url) }
        }

        // Wire dual-layer InputEngine -> CatPhysics
        let engine = InputEngine.shared
        engine.onKeyDown = { [weak self] key in
            DispatchQueue.main.async {
                self?.overlayWindow.catView.physics.triggerKeyDown(key)
            }
        }
        engine.onMouseClick = { [weak self] button, isDown in
            DispatchQueue.main.async {
                self?.overlayWindow.catView.physics.triggerMouseClick(button: button, isDown: isDown)
            }
        }
        engine.start()

        setupStatusItem()
        if isAutoStartEnabled() {
            // Keep LaunchAgent path updated if user moved KuroCat.app into /Applications
            _ = setAutoStartEnabled(true)
        }
        startRenderLoop()
        startWindowWatchdog()

        // Auto-check GitHub Releases for updates shortly after launch
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.5) { [weak self] in
            self?.checkForUpdates(manual: false)
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        // Quitting during launch because a newer copy is running: leave its stats alone.
        if isHandingOverToNewerCopy { return }
        renderTimer?.invalidate()
        windowWatchdogTimer?.invalidate()
        NSWorkspace.shared.notificationCenter.removeObserver(self)
        InputEngine.shared.flushStats()
        InputEngine.shared.stop()
    }

    // MARK: - One KuroCat at a time (newest copy wins)

    /// Users often download a new KuroCat.app while an older copy is still running (or is started
    /// by the login item from another folder). That old process kept running and kept announcing
    /// "new version available" even though they had already updated. Keep only the newest copy:
    /// quit older ones, or hand over to an already-running newer one. Returns true if THIS process quits.
    private func resolveDuplicateRunningCopies() -> Bool {
        guard let bundleID = Bundle.main.bundleIdentifier else { return false }
        let myPID = ProcessInfo.processInfo.processIdentifier
        let myURL = Bundle.main.bundleURL.standardizedFileURL
        let others = NSRunningApplication.runningApplications(withBundleIdentifier: bundleID)
            .filter { $0.processIdentifier != myPID }
        if others.isEmpty { return false }

        for other in others {
            guard let otherURL = other.bundleURL?.standardizedFileURL, otherURL != myURL,
                  let otherVersion = Self.shortVersion(ofAppAt: otherURL) else { continue }
            if isVersion(otherVersion, newerThan: currentAppVersion) {
                isHandingOverToNewerCopy = true
                _ = other.activate(options: [])
                NSApp.terminate(nil)
                return true
            }
        }
        for other in others {
            // Same path (replaced in place by an update) or an older/equal copy elsewhere.
            if !other.terminate() { _ = other.forceTerminate() }
            DispatchQueue.main.asyncAfter(deadline: .now() + 3.0) {
                if !other.isTerminated { _ = other.forceTerminate() }
            }
        }
        return false
    }

    /// Reads CFBundleShortVersionString straight from disk (Bundle(url:) may return cached info).
    private static func shortVersion(ofAppAt url: URL) -> String? {
        let plistURL = url.appendingPathComponent("Contents/Info.plist")
        guard let dict = NSDictionary(contentsOf: plistURL) as? [String: Any] else { return nil }
        return dict["CFBundleShortVersionString"] as? String
    }

    /// A newer KuroCat.app that is already on disk (e.g. downloaded to ~/Downloads but never
    /// launched because LaunchServices just re-activated this running copy).
    private func newestDownloadedCopy() -> (url: URL, version: String)? {
        guard let bundleID = Bundle.main.bundleIdentifier else { return nil }
        let myURL = Bundle.main.bundleURL.standardizedFileURL
        var best: (url: URL, version: String)?
        let appURLs: [URL]
        if #available(macOS 12.0, *) {
            appURLs = NSWorkspace.shared.urlsForApplications(withBundleIdentifier: bundleID)
        } else {
            appURLs = (LSCopyApplicationURLsForBundleIdentifier(bundleID as CFString, nil)?.takeRetainedValue() as? [URL]) ?? []
        }
        for url in appURLs {
            let std = url.standardizedFileURL
            guard std != myURL, !std.path.contains("/.Trash/"),
                  let v = Self.shortVersion(ofAppAt: std),
                  isVersion(v, newerThan: best?.version ?? currentAppVersion) else { continue }
            best = (std, v)
        }
        return best
    }

    private func switchToDownloadedCopy(_ url: URL) {
        let config = NSWorkspace.OpenConfiguration()
        config.createsNewApplicationInstance = true
        NSWorkspace.shared.openApplication(at: url, configuration: config) { _, error in
            DispatchQueue.main.async {
                if error == nil { NSApp.terminate(nil) }
            }
        }
    }

    // MARK: - Adaptive Redraw

    private func startRenderLoop() {
        lastFrameTime = CACurrentMediaTime()
        scheduleRenderTick(after: 1.0 / 30.0)
    }

    /// One-shot timers, re-armed with a state-dependent delay, so the process wakes 15-45 times a
    /// second depending on activity (10 while hidden).
    private func scheduleRenderTick(after delay: TimeInterval) {
        renderTimer?.invalidate()
        let timer = Timer(timeInterval: delay, repeats: false) { [weak self] _ in
            guard let self else { return }
            self.scheduleRenderTick(after: self.renderTick())
        }
        RunLoop.main.add(timer, forMode: .common)
        renderTimer = timer
    }

    /// Advances the simulation and requests a redraw when needed. Returns the delay until the next tick.
    private func renderTick() -> TimeInterval {
        guard isPetVisible else { return 0.1 }
        let now = CACurrentMediaTime()
        let dt = CGFloat(now - self.lastFrameTime)
        self.lastFrameTime = now

        // Fullscreen auto-hide check (~2Hz)
        if now - self.lastFullscreenCheckTime > 0.5 {
            self.lastFullscreenCheckTime = now
            let s = CatSettings.shared
            let obsBg = (s.obsCaptureMode > 0 && s.obsHideDesktop)
            if s.autoHideFullscreen && !obsBg {
                let fs = Self.isOtherAppFullscreen()
                if fs != self.hiddenByFullscreen {
                    self.hiddenByFullscreen = fs
                    self.overlayWindow.alphaValue = fs ? 0.0 : 1.0
                    if fs { self.overlayWindow.ignoresMouseEvents = true }
                }
            } else if self.hiddenByFullscreen {
                self.hiddenByFullscreen = false
                self.overlayWindow.alphaValue = 1.0
            }
        }

        let view = self.overlayWindow.catView
        let physics = view.physics
        let input = InputEngine.shared
        let windowFrame = self.overlayWindow.frame
        let previousMouseMoveTime = input.lastMouseMoveTime
        let wasBlinking = physics.isBlinking
        let wasYawning = physics.isYawning
        physics.step(dt: dt, windowFrameInScreen: windowFrame)
        view.updateHoverAndClickThrough()

        // Track the mouse every tick. Ordinary movement away from the pet can share the idle redraw cadence.
        let lastDirectInput = max(input.lastKeyActivityTime, input.lastMouseClickTime, self.lastDemoKeyTime)
        let mouseMoving = now - input.lastMouseMoveTime < 0.12
        let mouseStartedMoving = mouseMoving && now - previousMouseMoveTime >= 0.12
        let mouseNearPet = mouseMoving && windowFrame.insetBy(dx: -24, dy: -24).contains(input.lastMouseLocation)
        let isAsleep = physics.sleepiness >= 0.95
        let isActive = now - lastDirectInput < 0.8 || !input.activeKeys.isEmpty
            || mouseStartedMoving
            || input.leftMouseDown || input.rightMouseDown || input.otherMouseDown || view.isDraggingOrResizing
            || !physics.keyGlow.isEmpty || !physics.floatingKeycaps.isEmpty
            || physics.petHeadIntensity > 0.02 || physics.huntIntensity.value > 0.02
            || physics.scruffDrag.value > 0.02 || physics.lieIntensity.value > 0.02
            || physics.angerIntensity.value > 0.02 || physics.dizzyIntensity.value > 0.02
            || wasYawning || physics.isYawning || (!isAsleep && (wasBlinking || physics.isBlinking))
        // Hard caps: 45 FPS while something is animating (typing, clicks, petting, blinking...), 30 FPS when
        // only following the cursor or calm and awake, 15 FPS asleep. The tick rate matches the redraw cap.
        let followingCursorOnly = mouseNearPet && !isActive
        let tickInterval: TimeInterval = isActive ? 1.0 / 45.0 : (isAsleep ? 1.0 / 15.0 : 1.0 / 30.0)
        let sinceRedraw = now - self.lastRedrawTime
        // Allow for timer jitter; give small idle movements time to accumulate, but refresh at least every 100 ms.
        if sinceRedraw >= tickInterval - 0.004 &&
            (isActive || sinceRedraw >= 0.1 || followingCursorOnly || view.poseNeedsDisplay()) {
            self.lastRedrawTime = now
            view.needsDisplay = true
        }
        return tickInterval
    }

    // MARK: - Menu Bar Icon & Menu

    private func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.image = makeMenuBarCatIcon()
            button.toolTip = "KuroCat 黑猫键盘桌宠"
        }
        let menu = NSMenu()
        menu.delegate = self
        statusItem.menu = menu
    }

    func menuNeedsUpdate(_ menu: NSMenu) {
        menu.removeAllItems()
        for item in buildMenu().items {
            item.menu?.removeItem(item)
            menu.addItem(item)
        }
    }

    private func buildMenu() -> NSMenu {
        let menu = NSMenu()
        let engine = InputEngine.shared
        let settings = CatSettings.shared
        let physics = overlayWindow.catView.physics
        let en = settings.isEnglish

        func tr(_ zh: String, _ eng: String) -> String { en ? eng : zh }

        // Status header
        let statusPrefix = engine.status.needsAttention ? "⚠️" : "🟢"
        let statusText: String
        if en {
            switch engine.status {
            case .fullHardware: statusText = "Hardware Input Active (HID + EventTap)"
            case .hidOnly: statusText = "Hardware HID Active"
            case .eventTapOnly: statusText = "Global EventTap Active"
            case .needsPermission: statusText = "Input Monitoring Permission Needed"
            case .needsRestart: statusText = "Authorized — Restart to Retry"
            }
        } else {
            statusText = engine.status.rawValue
        }
        let statusItem = NSMenuItem(title: "\(statusPrefix) \(statusText)", action: engine.status.needsAttention ? #selector(openInputMonitoringSettings) : nil, keyEquivalent: "")
        statusItem.target = self
        menu.addItem(statusItem)

        if engine.status == .needsPermission {
            let grantItem = NSMenuItem(title: tr("🔑 前往系统设置开启「输入监控」...", "🔑 Open Input Monitoring Settings..."), action: #selector(openInputMonitoringSettings), keyEquivalent: "")
            grantItem.target = self
            menu.addItem(grantItem)
        }

        // Live stats
        let statsTitle = en
            ? "⌨️ Today \(engine.totalKeystrokesToday)  ·  KPM \(engine.currentKPM)  ·  Pets \(physics.totalPetSessions)"
            : "⌨️ 今日击键 \(engine.totalKeystrokesToday)  ·  KPM \(engine.currentKPM)  ·  摸猫 \(physics.totalPetSessions) 次"
        let statsItem = NSMenuItem(title: statsTitle, action: nil, keyEquivalent: "")
        statsItem.isEnabled = false
        menu.addItem(statsItem)

        menu.addItem(.separator())

        // Visibility & Interactive modes
        let visItem = NSMenuItem(title: isPetVisible ? tr("隐藏桌宠", "Hide Pet") : tr("显示桌宠", "Show Pet"), action: #selector(toggleVisibility), keyEquivalent: "")
        visItem.target = self
        menu.addItem(visItem)

        let clickThroughItem = NSMenuItem(title: tr("完全鼠标穿透 (按住 ⌥Option 可临时右键/拖动)", "Full Click-Through (Hold ⌥Option to Drag/Menu)"), action: #selector(toggleClickThrough), keyEquivalent: "")
        clickThroughItem.target = self
        clickThroughItem.state = settings.fullClickThrough ? .on : .off
        menu.addItem(clickThroughItem)

        let fullscreenItem = NSMenuItem(title: tr("全屏游戏/应用置顶显示", "Always On Top Over Fullscreen"), action: #selector(toggleFullScreenOverlay), keyEquivalent: "")
        fullscreenItem.target = self
        fullscreenItem.state = settings.fullScreenOverlay ? .on : .off
        menu.addItem(fullscreenItem)

        let autoHideFsItem = NSMenuItem(title: tr("全屏应用时自动隐藏", "Auto-Hide in Fullscreen Apps"), action: #selector(toggleAutoHideFullscreen), keyEquivalent: "")
        autoHideFsItem.target = self
        autoHideFsItem.state = settings.autoHideFullscreen ? .on : .off
        menu.addItem(autoHideFsItem)

        let autoStartItem = NSMenuItem(title: tr("开机自动启动", "Launch at Login"), action: #selector(toggleAutoStart), keyEquivalent: "")
        autoStartItem.target = self
        autoStartItem.state = isAutoStartEnabled() ? .on : .off
        menu.addItem(autoStartItem)

        menu.addItem(.separator())

        // Features
        let badgesItem = NSMenuItem(title: tr("显示按键气泡 ([S] [W] [⇧])", "Show Key & Click Badges"), action: #selector(toggleKeyBadges), keyEquivalent: "")
        badgesItem.target = self
        badgesItem.state = settings.showKeyBadges ? .on : .off
        menu.addItem(badgesItem)

        let legendsItem = NSMenuItem(title: tr("显示键盘字符", "Show Keycap Legends"), action: #selector(toggleKeyLegends), keyEquivalent: "")
        legendsItem.target = self
        legendsItem.state = settings.showKeyLegends ? .on : .off
        menu.addItem(legendsItem)

        let marqueeItem = NSMenuItem(title: tr("键盘跑马灯流光", "Keyboard RGB Marquee"), action: #selector(toggleKeyboardMarquee), keyEquivalent: "")
        marqueeItem.target = self
        marqueeItem.state = settings.keyboardMarquee ? .on : .off
        menu.addItem(marqueeItem)

        // OBS Live Capture Mode submenu
        let obsMenu = NSMenu()
        let obsModes: [(String, Int)] = [
            (tr("关闭（正常桌面悬浮）", "Off (Normal Overlay)"), 0),
            (tr("OBS 透明窗口捕获", "OBS Transparent Window Capture"), 1),
            (tr("OBS 绿幕模式 (#00FF00)", "OBS Green Screen (#00FF00)"), 2)
        ]
        for (title, mode) in obsModes {
            let item = NSMenuItem(title: title, action: #selector(selectOBSCaptureMode(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = NSNumber(value: mode)
            item.state = (settings.obsCaptureMode == mode) ? .on : .off
            obsMenu.addItem(item)
        }
        obsMenu.addItem(.separator())
        let obsHideItem = NSMenuItem(
            title: tr("后台捕获（隐藏桌面小猫，OBS仍正常捕获）", "Background Capture (Hide on Desktop)"),
            action: #selector(toggleOBSHideDesktop),
            keyEquivalent: ""
        )
        obsHideItem.target = self
        obsHideItem.state = settings.obsHideDesktop ? .on : .off
        obsHideItem.isEnabled = (settings.obsCaptureMode > 0)
        obsMenu.addItem(obsHideItem)
        let obsParent = NSMenuItem(title: tr("📺 直播捕获模式 (OBS)", "📺 OBS Live Capture Mode"), action: nil, keyEquivalent: "")
        obsParent.submenu = obsMenu
        menu.addItem(obsParent)

        menu.addItem(.separator())

        // Interactive resize & flip toggle (replaces static scale submenu; also triggered by double-clicking cat)
        let editItem = NSMenuItem(
            title: settings.isEditMode
                ? tr("✓ 完成调整大小与方向", "✓ Finish Resize & Flip")
                : tr("调整大小与方向 (双击小猫)", "Resize & Flip (Double-Click Cat)"),
            action: #selector(toggleEditMode),
            keyEquivalent: ""
        )
        editItem.target = self
        editItem.state = settings.isEditMode ? .on : .off
        menu.addItem(editItem)

        // Cat coat submenu
        let coatMenu = NSMenu()
        let library = SkinLibrary.shared
        let customActive = library.skin(id: settings.customSkin) != nil
        for coat in CatCoatTheme.allCases {
            let coatTitle: String
            if en {
                switch coat {
                case .classicBlack: coatTitle = "Classic Black Cat"
                case .siamese: coatTitle = "Siamese Cat"
                case .shorthair: coatTitle = "Silver Tabby Shorthair"
                case .orangeWhite: coatTitle = "Orange & White Cat"
                case .britishShorthair: coatTitle = "British Shorthair (Gray)"
                }
            } else {
                coatTitle = coat.rawValue
            }
            let item = NSMenuItem(title: coatTitle, action: #selector(selectCoatTheme(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = coat.rawValue
            item.state = !customActive && settings.coatTheme == coat ? .on : .off
            coatMenu.addItem(item)
        }
        // Imported .kurocat skins (docs/skin-format.md)
        if !library.skins.isEmpty {
            coatMenu.addItem(.separator())
            for skin in library.skins {
                let item = NSMenuItem(title: skin.name, action: #selector(selectCustomSkin(_:)), keyEquivalent: "")
                item.target = self
                item.representedObject = skin.id
                item.state = settings.customSkin == skin.id ? .on : .off
                if !skin.author.isEmpty { item.toolTip = tr("作者：\(skin.author)", "Author: \(skin.author)") }
                coatMenu.addItem(item)
            }
        }
        coatMenu.addItem(.separator())
        let importItem = NSMenuItem(title: tr("导入皮肤…", "Import Skin…"), action: #selector(showDressingRoom), keyEquivalent: "")
        importItem.target = self
        coatMenu.addItem(importItem)
        let workshopItem = NSMenuItem(title: tr("打开皮肤工坊（制作皮肤）…", "Open Skin Workshop…"), action: #selector(openSkinWorkshop), keyEquivalent: "")
        workshopItem.target = self
        coatMenu.addItem(workshopItem)
        let openDirItem = NSMenuItem(title: tr("打开皮肤文件夹（备份）", "Open Skins Folder (Backup)"), action: #selector(openSkinsFolder), keyEquivalent: "")
        openDirItem.target = self
        coatMenu.addItem(openDirItem)
        if !library.skins.isEmpty {
            let deleteMenu = NSMenu()
            for skin in library.skins {
                let item = NSMenuItem(title: skin.name, action: #selector(deleteCustomSkin(_:)), keyEquivalent: "")
                item.target = self
                item.representedObject = skin.id
                deleteMenu.addItem(item)
            }
            let deleteParent = NSMenuItem(title: tr("删除皮肤", "Delete Skin"), action: nil, keyEquivalent: "")
            deleteParent.submenu = deleteMenu
            coatMenu.addItem(deleteParent)
        }
        let coatParent = NSMenuItem(title: tr("猫咪皮肤 🐱", "Cat Skin 🐱"), action: nil, keyEquivalent: "")
        coatParent.submenu = coatMenu
        menu.addItem(coatParent)

        // Keyboard Theme submenu
        let kbMenu = NSMenu()
        for theme in KeyboardTheme.allCases {
            let kbTitle: String
            if en {
                switch theme {
                case .classicMilk: kbTitle = "Classic Milk White"
                case .midnightHacker: kbTitle = "Midnight Hacker Black"
                case .retroCream: kbTitle = "Retro Cream Gray"
                case .graphite: kbTitle = "Graphite Gray"
                }
            } else {
                kbTitle = theme.rawValue
            }
            let item = NSMenuItem(title: kbTitle, action: #selector(selectKeyboardTheme(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = theme.rawValue
            item.state = settings.keyboardTheme == theme ? .on : .off
            kbMenu.addItem(item)
        }
        let kbParent = NSMenuItem(title: tr("键盘配色", "Keyboard Theme"), action: nil, keyEquivalent: "")
        kbParent.submenu = kbMenu
        menu.addItem(kbParent)

        // Eye Color submenu
        let eyeMenu = NSMenu()
        for eye in EyeColorTheme.allCases {
            let eyeTitle: String
            if en {
                switch eye {
                case .neonChartreuse: eyeTitle = "Neon Chartreuse"
                case .siameseBlue: eyeTitle = "Siamese Ice Blue"
                case .warmAmber: eyeTitle = "Warm Amber"
                case .iceCyan: eyeTitle = "Ice Cyan"
                case .softAmber: eyeTitle = "Soft Amber"
                case .referenceHazel: eyeTitle = "Golden Hazel"
                }
            } else {
                eyeTitle = eye.rawValue
            }
            let item = NSMenuItem(title: eyeTitle, action: #selector(selectEyeTheme(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = eye.rawValue
            item.state = settings.eyeTheme == eye ? .on : .off
            eyeMenu.addItem(item)
        }
        let eyeParent = NSMenuItem(title: tr("瞳色切换", "Eye Color"), action: nil, keyEquivalent: "")
        eyeParent.submenu = eyeMenu
        menu.addItem(eyeParent)

        // Drawing Tablet & Stylus submenu
        let tabMenu = NSMenu()
        let tabOptions: [(String, Int)] = [
            (tr("自动检测（连接数位板时自动切换）", "Auto-Detect (Switch When Connected)"), 0),
            (tr("始终显示数位板与触控笔", "Always Show Tablet & Stylus"), 1),
            (tr("始终显示鼠标", "Always Show Mouse"), 2)
        ]
        for (title, mode) in tabOptions {
            let item = NSMenuItem(title: title, action: #selector(selectTabletMode(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = mode
            item.state = (settings.tabletMode == mode) ? .on : .off
            tabMenu.addItem(item)
        }
        let tabParent = NSMenuItem(title: tr("🎨 数位板与触控笔", "🎨 Drawing Tablet & Stylus"), action: nil, keyEquivalent: "")
        tabParent.submenu = tabMenu
        menu.addItem(tabParent)

        // Language submenu
        let langMenu = NSMenu()
        for (title, code) in [("简体中文", "zh"), ("English", "en")] {
            let item = NSMenuItem(title: title, action: #selector(selectLanguage(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = code
            item.state = (settings.language == code) ? .on : .off
            langMenu.addItem(item)
        }
        let langParent = NSMenuItem(title: "🌐 语言 / Language", action: nil, keyEquivalent: "")
        langParent.submenu = langMenu
        menu.addItem(langParent)

        menu.addItem(.separator())

        let relaunchItem = NSMenuItem(title: tr("重启 KuroCat (授权后刷新)", "Restart KuroCat"), action: #selector(relaunchApp), keyEquivalent: "")
        relaunchItem.target = self
        menu.addItem(relaunchItem)

        if let ver = availableUpdateVersion {
            let title = isUpdating
                ? tr("⏳ 正在下载更新 \(ver)...", "⏳ Downloading update \(ver)...")
                : tr("🆕 发现新版本 \(ver) — 点击自动更新", "🆕 Update Available \(ver) — Click to Update")
            let updateItem = NSMenuItem(title: title, action: #selector(openUpdateDownload), keyEquivalent: "")
            updateItem.target = self
            updateItem.isEnabled = !isUpdating
            menu.addItem(updateItem)
        } else {
            let checkItem = NSMenuItem(
                title: tr("检查更新 (当前 v\(currentAppVersion))", "Check for Updates (v\(currentAppVersion))"),
                action: #selector(manualCheckForUpdates),
                keyEquivalent: ""
            )
            checkItem.target = self
            menu.addItem(checkItem)
        }

        menu.addItem(.separator())

        let quitItem = NSMenuItem(title: tr("退出 KuroCat", "Quit KuroCat"), action: #selector(quitApp), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)

        return menu
    }

    // MARK: - Actions

    @objc private func openInputMonitoringSettings() {
        if InputEngine.shared.status == .needsRestart {
            NSApp.activate(ignoringOtherApps: true)
            let alert = NSAlert()
            alert.messageText = "已授权，键盘监听启动失败"
            alert.informativeText = "请重启 KuroCat，让系统重新建立键盘监听。\n当前程序位置：\(Bundle.main.bundlePath)"
            alert.addButton(withTitle: "重启 KuroCat")
            alert.addButton(withTitle: "打开输入监控")
            alert.addButton(withTitle: "取消")
            let response = alert.runModal()
            if response == .alertFirstButtonReturn {
                relaunchApp()
                return
            }
            if response != .alertSecondButtonReturn { return }
        }
        InputEngine.shared.requestPermissionsAndOpenSettings()
    }

    @objc private func toggleVisibility() {
        isPetVisible.toggle()
        if isPetVisible {
            lastFrameTime = CACurrentMediaTime()
            lastRedrawTime = 0
            overlayWindow.catView.needsDisplay = true
            overlayWindow.orderFrontRegardless()
            scheduleRenderTick(after: 1.0 / 30.0)
        } else {
            overlayWindow.orderOut(nil)
        }
    }

    @objc private func selectTabletMode(_ sender: NSMenuItem) {
        if let mode = sender.representedObject as? Int {
            CatSettings.shared.tabletMode = mode
            overlayWindow.catView.needsDisplay = true
        }
    }

    @objc private func toggleClickThrough() {
        CatSettings.shared.fullClickThrough.toggle()
    }

    @objc private func toggleFullScreenOverlay() {
        CatSettings.shared.fullScreenOverlay.toggle()
        overlayWindow.applyWindowLevelAndBehavior()
        syncWindowTopmost()
    }

    // MARK: - Topmost Watchdog (Space / App Activation)

    private func startWindowWatchdog() {
        let nc = NSWorkspace.shared.notificationCenter
        nc.addObserver(self, selector: #selector(handleWorkspaceStateChanged),
                       name: NSWorkspace.activeSpaceDidChangeNotification, object: nil)
        nc.addObserver(self, selector: #selector(handleWorkspaceStateChanged),
                       name: NSWorkspace.didActivateApplicationNotification, object: nil)

        windowWatchdogTimer?.invalidate()
        let timer = Timer(timeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.syncWindowTopmost()
        }
        RunLoop.main.add(timer, forMode: .common)
        windowWatchdogTimer = timer
    }

    @objc private func handleWorkspaceStateChanged() {
        syncWindowTopmost()
    }

    private func syncWindowTopmost() {
        guard let overlayWindow, isPetVisible, !hiddenByFullscreen else { return }
        let s = CatSettings.shared
        let obsBg = (s.obsCaptureMode > 0 && s.obsHideDesktop)
        if obsBg { return }
        if s.fullScreenOverlay {
            overlayWindow.orderFrontRegardless()
        }
    }

    // MARK: - Launch at Login (macOS LaunchAgent — works reliably for signed & ad-hoc builds)

    private var launchAgentURL: URL {
        let dir = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/LaunchAgents", isDirectory: true)
        return dir.appendingPathComponent("com.chanke.kurocat.plist")
    }

    private func isAutoStartEnabled() -> Bool {
        FileManager.default.fileExists(atPath: launchAgentURL.path)
    }

    @discardableResult
    private func setAutoStartEnabled(_ enable: Bool) -> Bool {
        let fm = FileManager.default
        let url = launchAgentURL
        if enable {
            do {
                try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                let appPath = Bundle.main.bundlePath
                let plist: [String: Any] = [
                    "Label": "com.chanke.kurocat",
                    "ProgramArguments": ["/usr/bin/open", "-a", appPath],
                    "RunAtLoad": true,
                    "KeepAlive": false
                ]
                let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
                try data.write(to: url, options: .atomic)
                return true
            } catch {
                return false
            }
        } else {
            do {
                if fm.fileExists(atPath: url.path) {
                    try fm.removeItem(at: url)
                }
                return true
            } catch {
                return false
            }
        }
    }

    @objc private func toggleAutoStart() {
        _ = setAutoStartEnabled(!isAutoStartEnabled())
    }

    @objc private func toggleKeyBadges() {
        CatSettings.shared.showKeyBadges.toggle()
    }

    @objc private func toggleMouseBadges() {
        CatSettings.shared.showMouseBadges.toggle()
    }

    @objc private func toggleKeyLegends() {
        CatSettings.shared.showKeyLegends.toggle()
    }

    @objc private func toggleFootEgg() {
        CatSettings.shared.footEasterEgg.toggle()
    }

    @objc private func toggleTailEgg() {
        CatSettings.shared.tailEasterEgg.toggle()
    }

    @objc private func toggleKeyboardMarquee() {
        CatSettings.shared.keyboardMarquee.toggle()
    }

    @objc private func togglePetInteraction() {
        CatSettings.shared.petInteraction.toggle()
    }

    @objc private func selectOBSCaptureMode(_ sender: NSMenuItem) {
        guard let num = sender.representedObject as? NSNumber else { return }
        CatSettings.shared.obsCaptureMode = num.intValue
        if num.intValue == 0 {
            CatSettings.shared.obsHideDesktop = false
        }
        overlayWindow.applyOBSMode()
    }

    @objc private func toggleOBSHideDesktop() {
        CatSettings.shared.obsHideDesktop.toggle()
        overlayWindow.applyOBSMode()
    }

    @objc private func toggleAutoHideFullscreen() {
        CatSettings.shared.autoHideFullscreen.toggle()
        if !CatSettings.shared.autoHideFullscreen && hiddenByFullscreen {
            hiddenByFullscreen = false
            overlayWindow.alphaValue = 1.0
        }
    }

    private static func isOtherAppFullscreen() -> Bool {
        guard let front = NSWorkspace.shared.frontmostApplication,
              front.processIdentifier != ProcessInfo.processInfo.processIdentifier,
              front.bundleIdentifier != "com.apple.finder" else {
            return false
        }
        guard let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] else {
            return false
        }
        let pid = front.processIdentifier
        for w in list {
            guard let ownerPID = w[kCGWindowOwnerPID as String] as? pid_t, ownerPID == pid,
                  let layer = w[kCGWindowLayer as String] as? Int, layer == 0,
                  let boundsDict = w[kCGWindowBounds as String] as? [String: Any],
                  let rect = CGRect(dictionaryRepresentation: boundsDict as CFDictionary) else { continue }
            for screen in NSScreen.screens {
                if rect.width >= screen.frame.width - 2 && rect.height >= screen.frame.height - 2 {
                    return true
                }
            }
        }
        return false
    }

    @objc private func toggleEditMode() {
        CatSettings.shared.isEditMode.toggle()
        overlayWindow.catView.needsDisplay = true
    }

    @objc private func selectLanguage(_ sender: NSMenuItem) {
        guard let code = sender.representedObject as? String else { return }
        CatSettings.shared.language = code
        overlayWindow.catView.needsDisplay = true
    }

    @objc private func selectScale(_ sender: NSMenuItem) {
        guard let num = sender.representedObject as? NSNumber else { return }
        overlayWindow.applyScale(CGFloat(num.doubleValue))
    }

    @objc private func selectCoatTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let coat = CatCoatTheme(rawValue: raw) else { return }
        CatSettings.shared.customSkin = ""
        CatSettings.shared.coatTheme = coat
        // Auto-pair signature eye color (still freely overridable via 瞳色切换!)
        switch coat {
        case .classicBlack:
            CatSettings.shared.eyeTheme = .neonChartreuse
        case .siamese:
            CatSettings.shared.eyeTheme = .siameseBlue
        case .shorthair:
            CatSettings.shared.eyeTheme = .referenceHazel
        case .orangeWhite, .britishShorthair:
            CatSettings.shared.eyeTheme = .warmAmber
        }
        dressingRoom?.refresh()
    }

    // MARK: - Imported skins (换装间)

    private static let skinWorkshopURLString = "https://chanke17.github.io/KuroCat-Workshop/"

    func application(_ application: NSApplication, open urls: [URL]) {
        for url in urls where url.pathExtension.lowercased() == "kurocat" {
            importSkin(from: url)
        }
    }

    @objc private func openSkinWorkshop() {
        if let url = URL(string: Self.skinWorkshopURLString) {
            NSWorkspace.shared.open(url)
        }
    }

    @objc private func openSkinsFolder() {
        let dir = SkinLibrary.directory
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        NSWorkspace.shared.open(dir)
    }

    @objc private func showDressingRoom() {
        if dressingRoom == nil {
            let controller = DressingRoomWindowController()
            controller.onImport = { [weak self] url in self?.importSkin(from: url) }
            dressingRoom = controller
        }
        dressingRoom?.present()
    }

    /// Validates and stores a `.kurocat`, then wears it. Invalid files get a Chinese explanation.
    private func importSkin(from url: URL) {
        do {
            let id = try SkinLibrary.shared.importFile(url)
            CatSettings.shared.customSkin = id
            let skin = SkinLibrary.shared.skin(id: id)
            var message = "已换上「\(skin?.name ?? "新皮肤")」"
            if let author = skin?.author, !author.isEmpty { message += "（作者：\(author)）" }
            dressingRoom?.setStatus("✓ " + message, kind: 1)
            dressingRoom?.refresh()
        } catch {
            let reason = error.localizedDescription
            // With 换装间 open the red status line explains it in place; an alert is only for drops onto the cat.
            if let room = dressingRoom, room.isVisible {
                room.setStatus("没能导入「\(url.lastPathComponent)」：\(reason)", kind: 2)
                return
            }
            let alert = NSAlert()
            alert.alertStyle = .warning
            alert.messageText = "无法导入这个皮肤"
            alert.informativeText = "「\(url.lastPathComponent)」：\(reason)"
            alert.addButton(withTitle: "知道了")
            NSApp.activate(ignoringOtherApps: true)
            alert.runModal()
        }
    }

    @objc private func selectCustomSkin(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? String else { return }
        CatSettings.shared.customSkin = id
        dressingRoom?.refresh()
    }

    @objc private func deleteCustomSkin(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? String, let skin = SkinLibrary.shared.skin(id: id) else { return }
        let alert = NSAlert()
        alert.messageText = "删除皮肤「\(skin.name)」？"
        alert.informativeText = "会从皮肤文件夹里删除这个文件，删除后需要重新导入。"
        alert.addButton(withTitle: "删除")
        alert.addButton(withTitle: "取消")
        NSApp.activate(ignoringOtherApps: true)
        guard alert.runModal() == .alertFirstButtonReturn else { return }
        if !SkinLibrary.shared.remove(id: id) {
            let failed = NSAlert()
            failed.messageText = "删除失败"
            failed.informativeText = "无法删除 \(SkinLibrary.directory.appendingPathComponent("\(id).kurocat").path)"
            failed.runModal()
            return
        }
        if CatSettings.shared.customSkin == id { CatSettings.shared.customSkin = "" }
        dressingRoom?.refresh()
    }

    @objc private func selectKeyboardTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let theme = KeyboardTheme(rawValue: raw) else { return }
        CatSettings.shared.keyboardTheme = theme
    }

    @objc private func selectEyeTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let theme = EyeColorTheme(rawValue: raw) else { return }
        CatSettings.shared.eyeTheme = theme
    }

    @objc private func resetPosition() {
        overlayWindow.resetPosition()
    }

    @objc private func runDemoBurst() {
        let keys: [PhysicalKey] = [.w, .a, .s, .d, .f, .j, .o, .e, .space, .leftShift]
        for (idx, k) in keys.enumerated() {
            DispatchQueue.main.asyncAfter(deadline: .now() + Double(idx) * 0.13) { [weak self] in
                guard let self else { return }
                let now = CACurrentMediaTime()
                self.lastDemoKeyTime = now
                self.overlayWindow.catView.physics.triggerKeyDown(k, at: now)
            }
        }
    }

    @objc private func relaunchApp() {
        let bundleURL = Bundle.main.bundleURL
        let config = NSWorkspace.OpenConfiguration()
        config.createsNewApplicationInstance = true
        NSWorkspace.shared.openApplication(at: bundleURL, configuration: config) { _, _ in
            DispatchQueue.main.async {
                NSApp.terminate(nil)
            }
        }
    }

    @objc private func quitApp() {
        NSApp.terminate(nil)
    }

    // MARK: - Auto Update Checker & In-App Self-Updater

    private static let updatePageURLString = "https://kurocat-download.app.workbuddy.host/?sharecode=JVAABIVZ7L"
    private static let updateVersionJSONURLString = "https://kurocat-download.app.workbuddy.host/version.json?sharecode=JVAABIVZ7L"
    private static let defaultMacAssetURLString = "https://kurocat-download.app.workbuddy.host/downloads/KuroCat-macOS-universal.zip?sharecode=JVAABIVZ7L"

    @objc private func manualCheckForUpdates() {
        checkForUpdates(manual: true)
    }

    @objc private func openUpdateDownload() {
        performInAppUpdate()
    }

    private func parseUpdateInfo(from data: Data) -> (tag: String, pageURL: URL?, assetURL: URL?)? {
        if let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
           let tagName = json["tag_name"] as? String, !tagName.isEmpty {
            let pageStr = (json["html_url"] as? String) ?? Self.updatePageURLString
            let assets = json["assets"] as? [String: Any]
            let macStr = (assets?["macos"] as? String) ?? Self.defaultMacAssetURLString
            return (tagName, URL(string: pageStr), URL(string: macStr))
        }
        if let html = String(data: data, encoding: .utf8) {
            let patterns = [
                #"v(\d+\.\d+\.\d+)\s*已发布"#,
                #"版本\s*v(\d+\.\d+\.\d+)"#
            ]
            for pattern in patterns {
                if let regex = try? NSRegularExpression(pattern: pattern),
                   let match = regex.firstMatch(in: html, range: NSRange(html.startIndex..., in: html)),
                   let r = Range(match.range(at: 1), in: html) {
                    return ("v" + String(html[r]), URL(string: Self.updatePageURLString), URL(string: Self.defaultMacAssetURLString))
                }
            }
        }
        return nil
    }

    private func fetchUpdateEndpoint(_ urlString: String, completion: @escaping ((tag: String, pageURL: URL?, assetURL: URL?)?) -> Void) {
        guard let url = URL(string: urlString) else {
            completion(nil)
            return
        }
        var req = URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 10)
        req.setValue("KuroCat/\(currentAppVersion)", forHTTPHeaderField: "User-Agent")

        URLSession.shared.dataTask(with: req) { [weak self] data, _, error in
            guard let self else { return }
            guard error == nil, let data, let parsed = self.parseUpdateInfo(from: data) else {
                completion(nil)
                return
            }
            completion(parsed)
        }.resume()
    }

    private func checkForUpdates(manual: Bool) {
        fetchUpdateEndpoint(Self.updateVersionJSONURLString) { [weak self] primary in
            guard let self else { return }
            if let info = primary {
                DispatchQueue.main.async { self.handleUpdateResult(info, manual: manual) }
                return
            }
            self.fetchUpdateEndpoint(Self.updatePageURLString) { [weak self] fallback in
                guard let self else { return }
                DispatchQueue.main.async {
                    if let info = fallback {
                        self.handleUpdateResult(info, manual: manual)
                    } else if manual {
                        self.showInfoAlert(title: "检查更新失败", message: "无法连接到更新服务器，请稍后重试或检查网络连接。")
                    }
                }
            }
        }
    }

    private func handleUpdateResult(_ info: (tag: String, pageURL: URL?, assetURL: URL?), manual: Bool) {
        if isVersion(info.tag, newerThan: currentAppVersion),
           let local = newestDownloadedCopy(), !isVersion(info.tag, newerThan: local.version) {
            // The user already downloaded the latest version; they are just still running the old copy.
            NSApp.activate(ignoringOtherApps: true)
            let alert = NSAlert()
            alert.alertStyle = .informational
            alert.messageText = "你已经下载了新版本 v\(local.version)"
            alert.informativeText = "但现在运行的还是旧版 v\(currentAppVersion)：\n\(Bundle.main.bundlePath)\n\n新版本位置：\n\(local.url.path)\n\n点击「切换到新版本」会关闭旧版并打开新版。建议把新版拖进「应用程序」文件夹并删除旧版。"
            alert.addButton(withTitle: "切换到新版本")
            alert.addButton(withTitle: "稍后再说")
            if alert.runModal() == .alertFirstButtonReturn {
                switchToDownloadedCopy(local.url)
            }
            return
        }
        if isVersion(info.tag, newerThan: currentAppVersion) {
            availableUpdateVersion = info.tag
            availableUpdateURL = info.pageURL
            availableAssetURL = info.assetURL
            promptUpdateAvailable(latestTag: info.tag)
        } else if manual {
            showInfoAlert(title: "已是最新版本", message: "当前版本 v\(currentAppVersion) 已是最新版本。")
        }
    }

    private func isVersion(_ lhs: String, newerThan rhs: String) -> Bool {
        func parse(_ v: String) -> [Int] {
            let trimmed = v.trimmingCharacters(in: CharacterSet(charactersIn: "vV "))
            return trimmed.split(separator: ".").map { Int($0.prefix(while: { $0.isNumber })) ?? 0 }
        }
        let a = parse(lhs)
        let b = parse(rhs)
        let count = max(a.count, b.count)
        for i in 0..<count {
            let va = i < a.count ? a[i] : 0
            let vb = i < b.count ? b[i] : 0
            if va != vb { return va > vb }
        }
        return false
    }

    private func promptUpdateAvailable(latestTag: String) {
        NSApp.activate(ignoringOtherApps: true)
        let alert = NSAlert()
        alert.alertStyle = .informational
        alert.messageText = "发现 KuroCat 新版本 \(latestTag)"
        alert.informativeText = "当前版本：v\(currentAppVersion)\n最新版本：\(latestTag)\n当前运行位置：\(Bundle.main.bundlePath)\n\n点击「立即更新」将在后台自动下载安装包并重启应用。"
        alert.addButton(withTitle: "立即更新")
        alert.addButton(withTitle: "打开下载页")
        alert.addButton(withTitle: "稍后再说")
        let resp = alert.runModal()
        if resp == .alertFirstButtonReturn {
            performInAppUpdate()
        } else if resp == .alertSecondButtonReturn {
            let fallback = URL(string: Self.updatePageURLString)!
            NSWorkspace.shared.open(availableUpdateURL ?? fallback)
        }
    }

    private func performInAppUpdate() {
        guard !isUpdating else { return }
        guard let assetURL = availableAssetURL ?? URL(string: Self.defaultMacAssetURLString) else { return }
        isUpdating = true

        var req = URLRequest(url: assetURL, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 60)
        req.setValue("KuroCat/\(currentAppVersion)", forHTTPHeaderField: "User-Agent")

        URLSession.shared.downloadTask(with: req) { [weak self] tempURL, response, error in
            guard let self else { return }
            let httpStatus = (response as? HTTPURLResponse)?.statusCode ?? 0
            guard error == nil, let tempURL, httpStatus == 200 || httpStatus == 0 else {
                DispatchQueue.main.async {
                    self.isUpdating = false
                    self.handleUpdateDownloadFailure()
                }
                return
            }

            let fm = FileManager.default
            let workDir = fm.temporaryDirectory.appendingPathComponent("KuroCatUpdate-\(UUID().uuidString)")
            let zipURL = workDir.appendingPathComponent("KuroCat-macOS-universal.zip")
            let extractDir = workDir.appendingPathComponent("extracted")
            do {
                try fm.createDirectory(at: extractDir, withIntermediateDirectories: true)
                try fm.copyItem(at: tempURL, to: zipURL)
                let attrs = try fm.attributesOfItem(atPath: zipURL.path)
                let fileSize = (attrs[.size] as? NSNumber)?.intValue ?? 0
                guard fileSize > 50_000 else {
                    throw NSError(domain: "KuroCatUpdate", code: 1)
                }

                let unzip = Process()
                unzip.executableURL = URL(fileURLWithPath: "/usr/bin/ditto")
                unzip.arguments = ["-x", "-k", zipURL.path, extractDir.path]
                try unzip.run()
                unzip.waitUntilExit()
                guard unzip.terminationStatus == 0 else {
                    throw NSError(domain: "KuroCatUpdate", code: 2)
                }

                let newAppURL = extractDir.appendingPathComponent("KuroCat.app")
                guard fm.fileExists(atPath: newAppURL.appendingPathComponent("Contents/MacOS/KuroCat").path) else {
                    throw NSError(domain: "KuroCatUpdate", code: 3)
                }

                let currentAppURL = Bundle.main.bundleURL
                let parentWritable = fm.isWritableFile(atPath: currentAppURL.deletingLastPathComponent().path)

                DispatchQueue.main.async {
                    self.installDownloadedApp(newAppURL: newAppURL, currentAppURL: currentAppURL, workDir: workDir, needsAdmin: !parentWritable)
                }
            } catch {
                try? fm.removeItem(at: workDir)
                DispatchQueue.main.async {
                    self.isUpdating = false
                    self.handleUpdateDownloadFailure()
                }
            }
        }.resume()
    }

    private func installDownloadedApp(newAppURL: URL, currentAppURL: URL, workDir: URL, needsAdmin: Bool) {
        func shQuote(_ s: String) -> String {
            "'" + s.replacingOccurrences(of: "'", with: "'\\''") + "'"
        }
        let pid = ProcessInfo.processInfo.processIdentifier
        let dst = shQuote(currentAppURL.path)
        let src = shQuote(newAppURL.path)
        let tmp = shQuote(workDir.path)

        if needsAdmin {
            let cmd = "rm -rf \(dst).bak && mv \(dst) \(dst).bak && cp -R \(src) \(dst) && rm -rf \(dst).bak \(tmp)"
            let escaped = cmd.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"")
            let osa = Process()
            osa.executableURL = URL(fileURLWithPath: "/usr/bin/osascript")
            osa.arguments = ["-e", "do shell script \"\(escaped)\" with administrator privileges"]
            do {
                try osa.run()
                osa.waitUntilExit()
                if osa.terminationStatus == 0 {
                    let relaunch = Process()
                    relaunch.executableURL = URL(fileURLWithPath: "/usr/bin/open")
                    relaunch.arguments = ["-n", currentAppURL.path]
                    try? relaunch.run()
                    NSApp.terminate(nil)
                    return
                }
            } catch {}
            try? FileManager.default.removeItem(at: workDir)
            isUpdating = false
            handleUpdateDownloadFailure()
            return
        }

        let script = """
        while kill -0 \(pid) 2>/dev/null; do sleep 0.2; done
        rm -rf \(dst).bak
        if mv \(dst) \(dst).bak && cp -R \(src) \(dst); then
            rm -rf \(dst).bak
            open -n \(dst)
        else
            mv \(dst).bak \(dst) 2>/dev/null
        fi
        rm -rf \(tmp)
        """
        let proc = Process()
        proc.executableURL = URL(fileURLWithPath: "/bin/sh")
        proc.arguments = ["-c", script]
        do {
            try proc.run()
            NSApp.terminate(nil)
        } catch {
            try? FileManager.default.removeItem(at: workDir)
            isUpdating = false
            handleUpdateDownloadFailure()
        }
    }

    private func handleUpdateDownloadFailure() {
        NSApp.activate(ignoringOtherApps: true)
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = "自动更新失败"
        alert.informativeText = "下载或替换安装包时出错，是否打开下载页手动下载？"
        alert.addButton(withTitle: "打开下载页")
        alert.addButton(withTitle: "取消")
        if alert.runModal() == .alertFirstButtonReturn {
            let fallback = URL(string: Self.updatePageURLString)!
            NSWorkspace.shared.open(availableUpdateURL ?? fallback)
        }
    }

    private func showInfoAlert(title: String, message: String) {
        NSApp.activate(ignoringOtherApps: true)
        let alert = NSAlert()
        alert.alertStyle = .informational
        alert.messageText = title
        alert.informativeText = message
        alert.addButton(withTitle: "好")
        alert.runModal()
    }

    // MARK: - Crisp Vector Menu Bar Cat Silhouette Icon

    private func makeMenuBarCatIcon() -> NSImage {
        let size = NSSize(width: 18, height: 18)
        let img = NSImage(size: size, flipped: true) { _ in
            guard let ctx = NSGraphicsContext.current?.cgContext else { return false }
            ctx.setFillColor(NSColor.black.cgColor)

            // Cute cat head silhouette with ears
            let path = CGMutablePath()
            path.move(to: CGPoint(x: 3.0, y: 8.5))
            path.addLine(to: CGPoint(x: 3.5, y: 2.5))
            path.addLine(to: CGPoint(x: 7.2, y: 6.0))
            path.addQuadCurve(to: CGPoint(x: 10.8, y: 6.0), control: CGPoint(x: 9.0, y: 5.5))
            path.addLine(to: CGPoint(x: 14.5, y: 2.5))
            path.addLine(to: CGPoint(x: 15.0, y: 8.5))
            path.addCurve(to: CGPoint(x: 9.0, y: 15.5), control1: CGPoint(x: 16.0, y: 12.8), control2: CGPoint(x: 13.0, y: 15.5))
            path.addCurve(to: CGPoint(x: 3.0, y: 8.5), control1: CGPoint(x: 5.0, y: 15.5), control2: CGPoint(x: 2.0, y: 12.8))
            path.closeSubpath()

            // Punch out cute cat eyes
            path.addEllipse(in: CGRect(x: 5.4, y: 8.8, width: 2.4, height: 2.2))
            path.addEllipse(in: CGRect(x: 10.2, y: 8.8, width: 2.4, height: 2.2))

            ctx.addPath(path)
            ctx.fillPath(using: .evenOdd)
            return true
        }
        img.isTemplate = true
        return img
    }
}
