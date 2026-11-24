import Foundation
import AppKit
import UniformTypeIdentifiers

private struct RenderedPose {
    struct Appearance: Equatable {
        let coat: CatCoatTheme
        let keyboard: KeyboardTheme
        let eyes: EyeColorTheme
        let showsLegends: Bool
        let size: CGSize
        let flipped: Bool
        let captureMode: Int
        let showEditControls: Bool
        let englishLabels: Bool
    }

    let appearance: Appearance
    let hasEffects: Bool
    let head: CGPoint
    let tail: CGPoint
    let tailBend: CGPoint
    let leftPaw: CGPoint
    let rightPaw: CGPoint
    let eyeClosure: CGFloat
    let headTurn: CGPoint
    let headTilt: CGFloat
    let eyeGaze: CGPoint
    let mouseOffset: CGPoint
    let pawOnMouse: Bool

    init(physics: CatPhysics, settings: CatSettings, size: CGSize) {
        let coat = settings.coatTheme
        appearance = Appearance(coat: coat, keyboard: settings.keyboardTheme, eyes: settings.eyeTheme,
                                showsLegends: settings.showKeyLegends, size: size, flipped: settings.flipHorizontal,
                                captureMode: settings.obsCaptureMode, showEditControls: settings.isEditMode,
                                englishLabels: settings.isEnglish)
        hasEffects = settings.keyboardMarquee || !physics.sparkles.isEmpty
            || !physics.floatingKeycaps.isEmpty || !physics.keyGlow.isEmpty
            || physics.angerIntensity.value > 0.02 || physics.dizzyIntensity.value > 0.02
            || physics.scruffDrag.value > 0.02 || physics.lieIntensity.value > 0.02
        head = CatRenderer.headPosition(physics, coat: coat)
        let tailCurve = CatRenderer.tailCurve(physics)
        tail = tailCurve.tip
        tailBend = tailCurve.control2
        leftPaw = physics.leftPaw.point
        rightPaw = physics.rightPaw.point
        eyeClosure = physics.effectiveEyeClosure
        headTurn = CGPoint(x: physics.headYaw.value, y: physics.headPitch.value)
        headTilt = physics.headTilt.value
        eyeGaze = physics.eyeGaze.point
        mouseOffset = physics.mouseOffset.point
        pawOnMouse = physics.leftPawOnMouse
    }

    func visiblyDiffers(from previous: RenderedPose) -> Bool {
        if appearance != previous.appearance || hasEffects || previous.hasEffects
            || pawOnMouse != previous.pawOnMouse { return true }
        func distance(_ a: CGPoint, _ b: CGPoint) -> CGFloat { hypot(a.x - b.x, a.y - b.y) }
        // Estimate how far visible features moved in canvas coordinates, then convert to screen points.
        let movement = max(distance(head, previous.head), distance(tail, previous.tail),
                           distance(tailBend, previous.tailBend),
                           distance(leftPaw, previous.leftPaw), distance(rightPaw, previous.rightPaw),
                           abs(eyeClosure - previous.eyeClosure) * 40, distance(headTurn, previous.headTurn) * 24,
                           abs(headTilt - previous.headTilt) * 52, distance(eyeGaze, previous.eyeGaze) * 10,
                           distance(mouseOffset, previous.mouseOffset))
        let scale = max(appearance.size.width / CatPhysics.canvasSize.width,
                        appearance.size.height / CatPhysics.canvasSize.height)
        return movement * scale >= 0.65
    }
}

final class CatCanvasView: NSView {
    let physics = CatPhysics()
    var onRequestPermission: (() -> Void)?
    var onRightClickMenu: ((NSEvent) -> Void)?
    /// A `.kurocat` dropped onto the cat is imported and worn right away.
    var onDropSkinFiles: (([URL]) -> Void)?

    private var lastHoverCanvasPt: CGPoint?
    private var isDragging = false
    private var activeResizeCorner: Int = -1 // -1 = none, 0=TL, 1=TR, 2=BL, 3=BR
    private var resizeAnchorScreenPt: CGPoint = .zero
    private var resizeStartFrame: CGRect = .zero
    private var dragStartScreenPt: CGPoint = .zero
    private var dragStartWindowOrigin: CGPoint = .zero
    private var isShowingOpenHand = false
    private var lastDrawnPose: RenderedPose?

    var isDraggingOrResizing: Bool { isDragging || activeResizeCorner >= 0 }

    override var isFlipped: Bool { true }
    override var isOpaque: Bool { false }
    override var acceptsFirstResponder: Bool { false }

    override func draw(_ dirtyRect: NSRect) {
        guard let ctx = NSGraphicsContext.current?.cgContext else { return }
        ctx.clear(bounds)

        let scaleX = bounds.width / CatPhysics.canvasSize.width
        let scaleY = bounds.height / CatPhysics.canvasSize.height
        ctx.saveGState()
        ctx.scaleBy(x: scaleX, y: scaleY)

        let status = InputEngine.shared.status
        CatRenderer.draw(in: ctx, physics: physics, showPermissionPill: status.needsAttention,
                         permissionPillText: status.permissionPillText)
        lastDrawnPose = RenderedPose(physics: physics, settings: CatSettings.shared, size: bounds.size)
        ctx.restoreGState()
    }

    func poseNeedsDisplay() -> Bool {
        guard let previous = lastDrawnPose else { return true }
        let current = RenderedPose(physics: physics, settings: CatSettings.shared, size: bounds.size)
        return current.visiblyDiffers(from: previous)
    }

    func canvasPoint(fromScreenPoint screenPt: CGPoint) -> CGPoint? {
        guard let win = window else { return nil }
        let frame = win.frame
        guard frame.contains(screenPt) else { return nil }
        let localX = (screenPt.x - frame.minX) / frame.width * CatPhysics.canvasSize.width
        // Convert bottom-up screen Y to flipped top-down canvas Y
        let localY = (frame.maxY - screenPt.y) / frame.height * CatPhysics.canvasSize.height
        return CGPoint(x: localX, y: localY)
    }

    /// Called on every render tick (15-45 Hz) to handle smart click-through AND petting strokes.
    func updateHoverAndClickThrough() {
        guard let win = window else { return }
        let screenPt = NSEvent.mouseLocation
        if isDraggingOrResizing {
            win.ignoresMouseEvents = false
            return
        }

        guard let pt = canvasPoint(fromScreenPoint: screenPt) else {
            lastHoverCanvasPt = nil
            physics.updateTailHover(canvasPoint: nil)
            if !win.ignoresMouseEvents {
                win.ignoresMouseEvents = true
            }
            resetCursorIfNeeded()
            return
        }

        if CatSettings.shared.isEditMode {
            let editHit = CatRenderer.hitTestEditOverlay(point: pt)
            win.ignoresMouseEvents = (editHit < 0)
            if editHit == 10 || editHit == 11 {
                NSCursor.pointingHand.set()
                isShowingOpenHand = false
            } else if editHit >= 0 && editHit <= 3 {
                NSCursor.crosshair.set()
                isShowingOpenHand = false
            } else if editHit == 12 {
                if !isShowingOpenHand {
                    NSCursor.openHand.set()
                    isShowingOpenHand = true
                }
            } else {
                resetCursorIfNeeded()
            }
            return
        }

        let catPt = CatSettings.shared.flipHorizontal
            ? CGPoint(x: CatPhysics.canvasSize.width - pt.x, y: pt.y)
            : pt
        let showPill = InputEngine.shared.status.needsAttention
        let zone = CatRenderer.hitTest(point: catPt, physics: physics, showPermissionPill: showPill)
        physics.updateTailHover(canvasPoint: catPt)

        // Petting triggers strictly on the cat's head (never on keyboard or torso)!
        if zone == .head {
            if let prev = lastHoverCanvasPt {
                let delta = hypot(catPt.x - prev.x, catPt.y - prev.y)
                if delta > 1.2 && delta < 80 {
                    physics.registerPetStroke(canvasPoint: catPt, delta: delta, isHead: true)
                }
            }
            lastHoverCanvasPt = catPt
        } else {
            lastHoverCanvasPt = nil
        }

        // Smart Click-Through logic (hold Option ⌥ to temporarily unlock Full Click-Through!)
        let optionHeld = NSEvent.modifierFlags.contains(.option)
        if CatSettings.shared.fullClickThrough && !optionHeld {
            // Keep permission pill clickable only if permission isn't granted yet
            win.ignoresMouseEvents = (zone != .permissionPill)
            resetCursorIfNeeded()
        } else {
            let shouldCapture = (zone != .none)
            if win.ignoresMouseEvents == shouldCapture {
                win.ignoresMouseEvents = !shouldCapture
            }
            if zone == .head || zone == .body {
                if !isShowingOpenHand {
                    NSCursor.openHand.set()
                    isShowingOpenHand = true
                }
            } else if zone == .permissionPill {
                NSCursor.pointingHand.set()
                isShowingOpenHand = false
            } else {
                resetCursorIfNeeded()
            }
        }
    }

    private func resetCursorIfNeeded() {
        if isShowingOpenHand {
            NSCursor.arrow.set()
            isShowingOpenHand = false
        }
    }

    override func mouseDown(with event: NSEvent) {
        guard let win = window else { return }
        let screenPt = NSEvent.mouseLocation
        let showPill = InputEngine.shared.status.needsAttention

        if let pt = canvasPoint(fromScreenPoint: screenPt) {
            if CatSettings.shared.isEditMode {
                let hit = CatRenderer.hitTestEditOverlay(point: pt)
                if hit == 10 {
                    CatSettings.shared.flipHorizontal.toggle()
                    needsDisplay = true
                    return
                }
                if hit == 11 {
                    CatSettings.shared.isEditMode = false
                    needsDisplay = true
                    return
                }
                if hit >= 0 && hit <= 3 {
                    activeResizeCorner = hit
                    resizeStartFrame = win.frame
                    // Anchor the opposite corner in screen space (AppKit Y is bottom-up)
                    let f = win.frame
                    switch hit {
                    case 0: resizeAnchorScreenPt = CGPoint(x: f.maxX, y: f.minY) // dragging TL -> anchor BR
                    case 1: resizeAnchorScreenPt = CGPoint(x: f.minX, y: f.minY) // dragging TR -> anchor BL
                    case 2: resizeAnchorScreenPt = CGPoint(x: f.maxX, y: f.maxY) // dragging BL -> anchor TR
                    default: resizeAnchorScreenPt = CGPoint(x: f.minX, y: f.maxY) // dragging BR -> anchor TL
                    }
                    return
                }
            }

            let catPt = CatSettings.shared.flipHorizontal
                ? CGPoint(x: CatPhysics.canvasSize.width - pt.x, y: pt.y)
                : pt
            let zone = CatRenderer.hitTest(point: catPt, physics: physics, showPermissionPill: showPill)
            if zone == .permissionPill {
                onRequestPermission?()
                return
            }

            // Double-clicking the cat toggles the interactive resize & flip box!
            if event.clickCount == 2 && (zone == .head || zone == .body || CatSettings.shared.isEditMode) {
                CatSettings.shared.isEditMode.toggle()
                needsDisplay = true
                return
            }
        }

        isDragging = true
        dragStartScreenPt = screenPt
        dragStartWindowOrigin = win.frame.origin
        physics.setDragging(true, dragDx: 0)
        NSCursor.closedHand.set()
    }

    override func mouseDragged(with event: NSEvent) {
        guard let win = window else { return }
        let currentScreenPt = NSEvent.mouseLocation
        if activeResizeCorner >= 0 {
            let dx = abs(currentScreenPt.x - resizeAnchorScreenPt.x)
            let dy = abs(currentScreenPt.y - resizeAnchorScreenPt.y)
            let sx = dx / CatPhysics.canvasSize.width
            let sy = dy / CatPhysics.canvasSize.height
            let newScale = max(0.45, min(2.50, max(sx, sy)))
            let newW = CatPhysics.canvasSize.width * newScale
            let newH = CatPhysics.canvasSize.height * newScale
            let originX = (activeResizeCorner == 0 || activeResizeCorner == 2)
                ? (resizeAnchorScreenPt.x - newW)
                : resizeAnchorScreenPt.x
            let originY = (activeResizeCorner == 2 || activeResizeCorner == 3)
                ? (resizeAnchorScreenPt.y - newH)
                : resizeAnchorScreenPt.y
            CatSettings.shared.scale = newScale
            win.setFrame(CGRect(x: originX, y: originY, width: newW, height: newH), display: true)
            return
        }
        guard isDragging else { return }
        let prevX = win.frame.origin.x
        let newOrigin = CGPoint(
            x: dragStartWindowOrigin.x + (currentScreenPt.x - dragStartScreenPt.x),
            y: dragStartWindowOrigin.y + (currentScreenPt.y - dragStartScreenPt.y)
        )
        let stepDx = newOrigin.x - prevX
        physics.setDragging(true, dragDx: CatSettings.shared.flipHorizontal ? -stepDx : stepDx)
        win.setFrameOrigin(newOrigin)
    }

    override func mouseUp(with event: NSEvent) {
        guard let win = window else { return }
        if activeResizeCorner >= 0 {
            activeResizeCorner = -1
            UserDefaults.standard.set(Double(win.frame.origin.x), forKey: "KuroCat.PosX")
            UserDefaults.standard.set(Double(win.frame.origin.y), forKey: "KuroCat.PosY")
            return
        }
        guard isDragging else { return }
        isDragging = false
        physics.setDragging(false)
        NSCursor.openHand.set()
        // Save position
        UserDefaults.standard.set(Double(win.frame.origin.x), forKey: "KuroCat.PosX")
        UserDefaults.standard.set(Double(win.frame.origin.y), forKey: "KuroCat.PosY")
    }

    override func rightMouseDown(with event: NSEvent) {
        onRightClickMenu?(event)
    }

    // MARK: Drag & drop of .kurocat skins onto the cat

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        registerForDraggedTypes([.fileURL])
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        SkinDropView.fileURLs(from: sender, kurocatOnly: true).isEmpty ? [] : .copy
    }

    override func draggingUpdated(_ sender: NSDraggingInfo) -> NSDragOperation {
        draggingEntered(sender)
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        let urls = SkinDropView.fileURLs(from: sender, kurocatOnly: true)
        guard !urls.isEmpty else { return false }
        // Import after the drag session ends so an error alert never blocks the drag source.
        DispatchQueue.main.async { [weak self] in self?.onDropSkinFiles?(urls) }
        return true
    }
}

final class OverlayWindow: NSPanel {
    let catView = CatCanvasView()

    init() {
        let scale = CatSettings.shared.scale
        let size = CGSize(
            width: CatPhysics.canvasSize.width * scale,
            height: CatPhysics.canvasSize.height * scale
        )
        let origin = OverlayWindow.initialOrigin(for: size)

        super.init(
            contentRect: CGRect(origin: origin, size: size),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )

        isOpaque = false
        backgroundColor = .clear
        hasShadow = false
        isMovable = false
        hidesOnDeactivate = false
        becomesKeyOnlyIfNeeded = true
        acceptsMouseMovedEvents = true
        ignoresMouseEvents = true

        applyWindowLevelAndBehavior()
        applyOBSMode()

        catView.frame = CGRect(origin: .zero, size: size)
        catView.autoresizingMask = [.width, .height]
        contentView = catView
    }

    func applyOBSMode() {
        let mode = CatSettings.shared.obsCaptureMode
        let hideDesktop = (mode > 0 && CatSettings.shared.obsHideDesktop)
        title = "KuroCat"
        sharingType = .readOnly
        if mode == 2 {
            isOpaque = true
            backgroundColor = NSColor(calibratedRed: 0.0, green: 1.0, blue: 0.0, alpha: 1.0)
        } else {
            isOpaque = false
            backgroundColor = .clear
        }
        if hideDesktop {
            // Tuck behind desktop/all apps while staying rendered so OBS Window Capture stays live!
            level = NSWindow.Level(rawValue: Int(CGWindowLevelForKey(.desktopIconWindow)) - 1)
            collectionBehavior = [.canJoinAllSpaces, .stationary, .ignoresCycle]
            ignoresMouseEvents = true
        } else {
            applyWindowLevelAndBehavior()
        }
        catView.needsDisplay = true
    }

    func applyWindowLevelAndBehavior() {
        if CatSettings.shared.fullScreenOverlay {
            // popUpMenuWindow (101) stays reliably above WPS/Keynote slideshows, fullscreen apps and Dock
            // while remaining below Spotlight and system Force Quit dialogs.
            level = NSWindow.Level(rawValue: Int(CGWindowLevelForKey(.popUpMenuWindow)))
            collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary, .ignoresCycle]
        } else {
            level = .floating
            collectionBehavior = [.canJoinAllSpaces, .stationary, .ignoresCycle]
        }
    }

    func applyScale(_ newScale: CGFloat) {
        CatSettings.shared.scale = newScale
        let oldFrame = frame
        let newSize = CGSize(
            width: CatPhysics.canvasSize.width * newScale,
            height: CatPhysics.canvasSize.height * newScale
        )
        // Anchor bottom-center so scaling feels grounded
        let newOrigin = CGPoint(
            x: oldFrame.midX - newSize.width * 0.5,
            y: oldFrame.minY
        )
        setFrame(CGRect(origin: newOrigin, size: newSize), display: true)
        UserDefaults.standard.set(Double(newOrigin.x), forKey: "KuroCat.PosX")
        UserDefaults.standard.set(Double(newOrigin.y), forKey: "KuroCat.PosY")
    }

    func resetPosition() {
        let origin = OverlayWindow.defaultOrigin(for: frame.size)
        setFrameOrigin(origin)
        UserDefaults.standard.set(Double(origin.x), forKey: "KuroCat.PosX")
        UserDefaults.standard.set(Double(origin.y), forKey: "KuroCat.PosY")
    }

    private static func initialOrigin(for size: CGSize) -> CGPoint {
        let d = UserDefaults.standard
        if d.object(forKey: "KuroCat.PosX") != nil && d.object(forKey: "KuroCat.PosY") != nil {
            let x = CGFloat(d.double(forKey: "KuroCat.PosX"))
            let y = CGFloat(d.double(forKey: "KuroCat.PosY"))
            let saved = CGPoint(x: x, y: y)
            // Ensure saved origin is still on a connected display
            let rect = CGRect(origin: saved, size: size)
            if NSScreen.screens.contains(where: { $0.frame.intersects(rect) }) {
                return saved
            }
        }
        return defaultOrigin(for: size)
    }

    private static func defaultOrigin(for size: CGSize) -> CGPoint {
        let screen = NSScreen.main?.visibleFrame ?? CGRect(x: 0, y: 0, width: 1440, height: 900)
        return CGPoint(
            x: screen.maxX - size.width - 28,
            y: screen.minY + 18
        )
    }
}

// MARK: - 「换装间」 skin import window (warm cream card UI shared with Windows/Linux)

private enum DressingPalette {
    static let background = NSColor(calibratedRed: 0.984, green: 0.969, blue: 0.941, alpha: 1)   // #FBF7F0
    static let card = NSColor(calibratedRed: 0.953, green: 0.914, blue: 0.863, alpha: 1)         // #F3E9DC
    static let ink = NSColor(calibratedRed: 0.231, green: 0.184, blue: 0.165, alpha: 1)          // #3B2F2A
    static let brown = NSColor(calibratedRed: 0.353, green: 0.290, blue: 0.251, alpha: 1)        // #5A4A40
    static let muted = NSColor(calibratedRed: 0.659, green: 0.600, blue: 0.553, alpha: 1)        // #A8998D
    static let orange = NSColor(calibratedRed: 0.910, green: 0.588, blue: 0.306, alpha: 1)       // #E8964E
    static let orangeDown = NSColor(calibratedRed: 0.843, green: 0.518, blue: 0.243, alpha: 1)   // #D7843E
    static let dash = NSColor(calibratedRed: 0.890, green: 0.765, blue: 0.627, alpha: 1)         // #E3C3A0
    static let zone = NSColor(calibratedRed: 1.0, green: 0.992, blue: 0.976, alpha: 1)           // #FFFDF9
    static let zoneHover = NSColor(calibratedRed: 1.0, green: 0.945, blue: 0.886, alpha: 1)      // #FFF1E2
    static let ok = NSColor(calibratedRed: 0.243, green: 0.557, blue: 0.353, alpha: 1)           // #3E8E5A
    static let err = NSColor(calibratedRed: 0.761, green: 0.286, blue: 0.231, alpha: 1)          // #C2493B
    static let note = NSColor(calibratedRed: 0.541, green: 0.482, blue: 0.439, alpha: 1)         // #8A7B70
    static let footer = NSColor(calibratedRed: 0.702, green: 0.651, blue: 0.608, alpha: 1)       // #B3A69B
}

private final class FlippedView: NSView {
    override var isFlipped: Bool { true }
}

/// Live preview of the skin being worn, drawn by the real renderer into a rounded card.
private final class SkinPreviewView: NSView {
    private let pose = CatPhysics()
    override var isFlipped: Bool { true }
    override func draw(_ dirtyRect: NSRect) {
        guard let ctx = NSGraphicsContext.current?.cgContext else { return }
        let card = NSBezierPath(roundedRect: bounds.insetBy(dx: 0.5, dy: 0.5), xRadius: 18, yRadius: 18)
        DressingPalette.card.setFill()
        card.fill()
        card.addClip()
        ctx.saveGState()
        ctx.scaleBy(x: bounds.width / 290, y: bounds.height / 260)
        ctx.translateBy(x: -40, y: -36)
        CatRenderer.draw(in: ctx, physics: pose, showPermissionPill: false)
        ctx.restoreGState()
    }
}

/// Dashed drop zone of the 「换装间」 window; click it to pick a file.
final class SkinDropView: NSView {
    var onFiles: (([URL]) -> Void)?
    var onClick: (() -> Void)?
    private var highlighted = false { didSet { needsDisplay = true } }
    override var isFlipped: Bool { true }

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        registerForDraggedTypes([.fileURL])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    static func fileURLs(from info: NSDraggingInfo, kurocatOnly: Bool) -> [URL] {
        let objects = info.draggingPasteboard.readObjects(forClasses: [NSURL.self], options: [.urlReadingFileURLsOnly: true])
        let urls = (objects as? [URL]) ?? []
        return kurocatOnly ? urls.filter { $0.pathExtension.lowercased() == "kurocat" } : urls
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        let ok = !SkinDropView.fileURLs(from: sender, kurocatOnly: false).isEmpty
        highlighted = ok
        return ok ? .copy : []
    }

    override func draggingExited(_ sender: NSDraggingInfo?) {
        highlighted = false
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        highlighted = false
        let urls = SkinDropView.fileURLs(from: sender, kurocatOnly: false)
        guard !urls.isEmpty else { return false }
        DispatchQueue.main.async { [weak self] in self?.onFiles?(urls) }
        return true
    }

    override func mouseDown(with event: NSEvent) {
        // Claim the click so the matching mouseUp comes back to this view.
    }

    override func mouseUp(with event: NSEvent) {
        if bounds.contains(convert(event.locationInWindow, from: nil)) { onClick?() }
    }

    override func resetCursorRects() {
        addCursorRect(bounds, cursor: .pointingHand)
    }

    override func draw(_ dirtyRect: NSRect) {
        let box = NSBezierPath(roundedRect: bounds.insetBy(dx: 1.5, dy: 1.5), xRadius: 16, yRadius: 16)
        (highlighted ? DressingPalette.zoneHover : DressingPalette.zone).setFill()
        box.fill()
        box.lineWidth = highlighted ? 2.4 : 2
        if !highlighted {
            let dash: [CGFloat] = [7, 5]
            box.setLineDash(dash, count: dash.count, phase: 0)
        }
        (highlighted ? DressingPalette.orange : DressingPalette.dash).setStroke()
        box.stroke()

        // Paw print
        let k: CGFloat = highlighted ? 1.15 : 1
        let c = CGPoint(x: bounds.midX, y: bounds.midY - 26)
        DressingPalette.orange.withAlphaComponent(highlighted ? 1 : 0.85).setFill()
        NSBezierPath(ovalIn: NSRect(x: c.x - 9.5 * k, y: c.y + (6 - 7.8) * k, width: 19 * k, height: 15.6 * k)).fill()
        for (tx, ty) in [(-10.5, -5.5), (-3.8, -11.5), (3.8, -11.5), (10.5, -5.5)] as [(CGFloat, CGFloat)] {
            NSBezierPath(ovalIn: NSRect(x: c.x + (tx - 3.7) * k, y: c.y + (ty - 4.4) * k, width: 7.4 * k, height: 8.8 * k)).fill()
        }

        func centered(_ string: String, font: NSFont, color: NSColor, y: CGFloat) {
            let attrs: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: color]
            let size = (string as NSString).size(withAttributes: attrs)
            (string as NSString).draw(at: NSPoint(x: bounds.midX - size.width / 2, y: y), withAttributes: attrs)
        }
        centered(highlighted ? "松手就换上啦" : "把 .kurocat 皮肤拖到这里", font: .systemFont(ofSize: 15, weight: .bold),
                 color: DressingPalette.brown, y: bounds.midY + 2)
        centered("也可以直接拖到桌面上的小猫身上 · 点这里选择文件", font: .systemFont(ofSize: 11.5),
                 color: DressingPalette.note, y: bounds.midY + 28)
    }
}

/// Orange pill button.
private final class PillButton: NSButton {
    override func draw(_ dirtyRect: NSRect) {
        let pill = NSBezierPath(roundedRect: bounds.insetBy(dx: 1, dy: 1), xRadius: (bounds.height - 2) / 2, yRadius: (bounds.height - 2) / 2)
        (isHighlighted ? DressingPalette.orangeDown : DressingPalette.orange).setFill()
        pill.fill()
        let attrs: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 14.5, weight: .bold), .foregroundColor: NSColor.white]
        let size = (title as NSString).size(withAttributes: attrs)
        (title as NSString).draw(at: NSPoint(x: bounds.midX - size.width / 2, y: bounds.midY - size.height / 2), withAttributes: attrs)
    }
    override func resetCursorRects() {
        addCursorRect(bounds, cursor: .pointingHand)
    }
}

/// 「换装间」: import `.kurocat` skins with drag & drop or a file picker.
final class DressingRoomWindowController: NSWindowController {
    var onImport: ((URL) -> Void)?
    private let preview = SkinPreviewView(frame: NSRect(x: 22, y: 22, width: 150, height: 134))
    private let nameLabel = NSTextField(labelWithString: "")
    private let authorLabel = NSTextField(labelWithString: "")
    private let statusLabel = NSTextField(wrappingLabelWithString: "")

    init() {
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 444, height: 472),
                              styleMask: [.titled, .closable], backing: .buffered, defer: false)
        window.title = "换装间"
        window.isReleasedWhenClosed = false
        window.level = .floating
        window.titlebarAppearsTransparent = true
        window.backgroundColor = DressingPalette.background
        window.appearance = NSAppearance(named: .aqua)
        super.init(window: window)

        let content = FlippedView(frame: NSRect(x: 0, y: 0, width: 444, height: 472))
        func label(_ text: String, _ font: NSFont, _ color: NSColor, _ frame: NSRect, wraps: Bool = false) -> NSTextField {
            let field = wraps ? NSTextField(wrappingLabelWithString: text) : NSTextField(labelWithString: text)
            field.font = font
            field.textColor = color
            field.frame = frame
            return field
        }
        content.addSubview(preview)
        content.addSubview(label("换装间", .systemFont(ofSize: 21, weight: .heavy), DressingPalette.ink, NSRect(x: 190, y: 22, width: 240, height: 30)))
        content.addSubview(label("正在穿", .systemFont(ofSize: 11.5), DressingPalette.muted, NSRect(x: 191, y: 58, width: 230, height: 16)))
        nameLabel.font = .systemFont(ofSize: 15, weight: .bold)
        nameLabel.textColor = DressingPalette.brown
        nameLabel.lineBreakMode = .byTruncatingTail
        nameLabel.frame = NSRect(x: 190, y: 74, width: 236, height: 22)
        authorLabel.font = .systemFont(ofSize: 11.5)
        authorLabel.textColor = DressingPalette.muted
        authorLabel.lineBreakMode = .byTruncatingTail
        authorLabel.frame = NSRect(x: 191, y: 97, width: 232, height: 16)
        content.addSubview(nameLabel)
        content.addSubview(authorLabel)
        let hint = label("导入皮肤工坊做好的 .kurocat 文件，\n小猫会立刻换上新毛色。", .systemFont(ofSize: 11.5), DressingPalette.muted,
                         NSRect(x: 191, y: 120, width: 236, height: 36), wraps: true)
        content.addSubview(hint)

        let drop = SkinDropView(frame: NSRect(x: 22, y: 172, width: 400, height: 150))
        drop.onFiles = { [weak self] urls in urls.forEach { self?.onImport?($0) } }
        drop.onClick = { [weak self] in self?.chooseFile() }
        content.addSubview(drop)

        let button = PillButton(title: "选择文件…", target: self, action: #selector(chooseFile))
        button.isBordered = false
        button.frame = NSRect(x: 147, y: 338, width: 150, height: 40)
        content.addSubview(button)

        statusLabel.alignment = .center
        statusLabel.font = .systemFont(ofSize: 12.5)
        statusLabel.frame = NSRect(x: 22, y: 390, width: 400, height: 40)
        content.addSubview(statusLabel)

        let footerPath = (SkinLibrary.directory.path as NSString).abbreviatingWithTildeInPath
        let footer = label("皮肤独立保存在 \(footerPath)（软件更新会自动保留）  ·  右键小猫 → 猫咪皮肤 可切换或备份", .systemFont(ofSize: 10.5),
                           DressingPalette.footer, NSRect(x: 22, y: 434, width: 400, height: 34), wraps: true)
        footer.alignment = .center
        content.addSubview(footer)

        window.contentView = content
        window.center()
        setStatus("支持拖入多个文件；坏掉的文件会告诉你哪里不对。", kind: 0)
        refresh()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    /// kind: 0 neutral, 1 success, 2 error
    func setStatus(_ text: String, kind: Int) {
        statusLabel.stringValue = text
        statusLabel.textColor = kind == 1 ? DressingPalette.ok : (kind == 2 ? DressingPalette.err : DressingPalette.note)
        statusLabel.font = .systemFont(ofSize: 12.5, weight: kind == 0 ? .regular : .semibold)
    }

    /// Re-reads the worn skin (name, author, live preview).
    func refresh() {
        let skin = CatSettings.shared.activeSkin
        nameLabel.stringValue = skin.name
        authorLabel.stringValue = skin.author.isEmpty ? "" : "作者 \(skin.author)"
        preview.needsDisplay = true
    }

    var isVisible: Bool { window?.isVisible ?? false }

    func present() {
        refresh()
        NSApp.activate(ignoringOtherApps: true)
        showWindow(nil)
        window?.makeKeyAndOrderFront(nil)
    }

    @objc private func chooseFile() {
        guard let window else { return }
        let panel = NSOpenPanel()
        panel.title = "选择 .kurocat 皮肤文件"
        panel.prompt = "导入"
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = true
        panel.allowedContentTypes = [UTType(filenameExtension: "kurocat") ?? .data]
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK else { return }
            for url in panel.urls { self?.onImport?(url) }
        }
    }
}
