//
//  SerialPort.swift
//  Screencap
//
//  Created by Alex on 04.03.25.
//

import SwiftSerial
import Cocoa

extension NSScreen {
    static func currentScreenForMouseLocation() -> NSScreen? {
        let mouseLocation = NSEvent.mouseLocation
        return screens.first(where: { NSMouseInRect(mouseLocation, $0.frame, false) })
    }
}

class SerialPort : ObservableObject {
    var port: SwiftSerial.SerialPort!
    @Published var shift: CGFloat = 0
    
    let screenWidthPx: Float = 1920
    let screenWidthSteps: Float = 5300
    
    init() {
        if setupSerialPort() {
            try! port.openPort()
            try! port.setSettings(baudRateSetting: .symmetrical(.baud115200), minimumBytesToRead: 4)
            //try! port.setSettings(baudRateSetting: .symmetrical(.init(1000000)), minimumBytesToRead: 4)
        } else {
            print("could not connect to serial")
            return
        }
        
        print("connected!")
        
        Task {
            let readStream = try port.asyncData()
            for await data in readStream {
                if data.count != 4 { continue }
                let (pos, velocity) = data.withUnsafeBytes { bytes in
                    let b = bytes.assumingMemoryBound(to: Int16.self)
                    return (b[0], b[1])
                }
                await updatePosition(pos, velocity)
                sendUpdate()
            }
        }
    }
    
    private func updatePosition(_ pos: Int16, _ velocity: Int16) async {
        await MainActor.run {
            shift = CGFloat(-(Float(pos) + 0.02 * Float(velocity)) * (screenWidthPx / screenWidthSteps))
        }
    }
    
    private func setupSerialPort() -> Bool {
        guard let devices = try? FileManager.default.contentsOfDirectory(atPath: "/dev") else {
            print("could not enumerate")
            return false
        }
        
        guard let serialPort = devices.first(where: { $0.hasPrefix("cu.usbmodemC") }) else {
            print("no port found")
            return false
        }
        
        print("connecting to /dev/\(serialPort)")
        port = .init(path: "/dev/\(serialPort)")
        return true
    }
    
    private func getPosition() -> Int16 {
        let cursorLocation = NSEvent.mouseLocation
        guard let screen = NSScreen.currentScreenForMouseLocation() else { return 0 }
        let cursorX = Float(cursorLocation.x - screen.frame.origin.x)
        var position = (cursorX / screenWidthPx - 0.5) * screenWidthSteps
        position = max(position, 0)
        //position = min(position, screenWidthSteps * 2)
        return Int16(truncatingIfNeeded: Int(position))
    }
    
    private func sendUpdate() {
        let position = getPosition()
        var blob = Data(capacity: 2)
        blob.append(contentsOf: [
            UInt8(truncatingIfNeeded: position),
            UInt8(truncatingIfNeeded: position >> 8)
        ])
        try! port.writeData(blob)
        print("sent \(position)")
    }
}
