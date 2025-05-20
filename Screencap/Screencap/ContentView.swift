//
//  ContentView.swift
//  Screencap
//
//  Created by Alexander Rath on 03.03.25.
//

import SwiftUI
import CoreImage
import Combine
import ScreenCaptureKit
import Cocoa

struct ContentView: View {
    private let captureEngine = CaptureEngine()
    private let capturePreview = CapturePreview()
    private let refreshRate = 144
    private let virtualDisplay: Any?
    private let virtualDisplayWidth = 1920 * 7 / 2
    @ObservedObject private var port = SerialPort()
    
    let backgroundQueue = DispatchQueue(label: "com.app.queue",
                                      qos: .background,
                                      target: nil)
    
    init() {
        virtualDisplay = createVirtualDisplay(Int32(virtualDisplayWidth), 1080, 91, Int32(refreshRate), false, "Virtual Display")
    }
    
    /// Starts capturing screen content.
    func start() async {
        guard let display = virtualDisplay else {
            print("virtual display was not created")
            return
        }
        let displayId = getDisplayId(display)
        
        let availableContent = try! await SCShareableContent.excludingDesktopWindows(false,
                                                                                        onScreenWindowsOnly: true)
        let availableDisplays = availableContent.displays
        guard let selectedDisplay = availableDisplays.first(where: { $0.displayID == displayId }) else {
            print("could not find virtual display")
            return
        }
        
        var streamConfiguration: SCStreamConfiguration {
            let streamConfig = SCStreamConfiguration()
            let scaleFactor = Int(NSScreen.main?.backingScaleFactor ?? 2)
            
            streamConfig.capturesAudio = false
            streamConfig.width = selectedDisplay.width * scaleFactor
            streamConfig.height = selectedDisplay.height * scaleFactor
            
            // Set the capture interval at 120 fps.
            streamConfig.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(refreshRate))
            
            // Increase the depth of the frame queue to ensure high fps at the expense of increasing
            // the memory footprint of WindowServer.
            streamConfig.queueDepth = 5
            
            return streamConfig
        }
        
        var contentFilter: SCContentFilter {
            return SCContentFilter(display: selectedDisplay,
                                     excludingApplications: [],
                                     exceptingWindows: [])
        }

        do {
            let config = streamConfiguration
            let filter = contentFilter
            for try await frame in captureEngine.startCapture(configuration: config, filter: filter) {
                capturePreview.updateFrame(frame)
            }
        } catch {
            print("\(error.localizedDescription)")
        }
    }
    
    var body: some View {
        VStack {
            capturePreview
                .offset(x: port.shift + CGFloat((virtualDisplayWidth - 1920) / 2))
        }
        .onAppear {
            Task { await start() }
        }
    }
}

#Preview {
    ContentView()
}
