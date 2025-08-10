#if os(macOS)
import Foundation
import SwiftyBitTorrent
import Dispatch

@available(macOS 13.0, *)
struct CLT {
    static func run() async throws {
        var args = CommandLine.arguments
        let exePath = args.removeFirst()
        var downloadDir: URL?

        // Parse optional --dir <path>
        var rest: [String] = []
        var i = 0
        while i < args.count {
            let a = args[i]
            if a == "--dir", i + 1 < args.count {
                downloadDir = URL(fileURLWithPath: args[i + 1], isDirectory: true)
                i += 2
                continue
            }
            rest.append(a)
            i += 1
        }

        // Default download directory next to the executable
        if downloadDir == nil {
            let exeURL = URL(fileURLWithPath: exePath).resolvingSymlinksInPath()
            downloadDir = exeURL.deletingLastPathComponent().appendingPathComponent("torrent_downloads", isDirectory: true)
        }

        guard let downloadDir else {
            fputs("Failed to determine download directory\n", stderr)
            exit(2)
        }

        // Ensure directory exists
        try FileManager.default.createDirectory(at: downloadDir, withIntermediateDirectories: true)

        guard !rest.isEmpty else {
            fputs("Usage: clt-swiftybt [--dir <path>] <magnet-or-torrent> [more...]\n", stderr)
            exit(2)
        }

        let session = BTSession(config: .init(savePath: downloadDir))
        var torrents: [BTTorrent] = []

        for a in rest {
            if a.hasPrefix("magnet:") {
                let t = try await session.addTorrent(magnet: a)
                torrents.append(t)
            } else {
                let url = URL(fileURLWithPath: a)
                let t = try await session.addTorrent(fileURL: url)
                torrents.append(t)
            }
        }

        for await batch in session.statusUpdatesStream(intervalMs: 1000) {
            print("\u{001B}[2J\u{001B}[H") // clear screen
            print("Saving to: \(downloadDir.path)\n")
            for (idx, st) in batch.enumerated() {
                let pct = Int((st.progress * 100).rounded())
                print("[")
                print(String(format: "%2d", idx+1), terminator: "] ")
                print(String(format: "%3d%%", pct),
                      "down:", humanize(bytesPerSec: st.downloadRate),
                      "up:", humanize(bytesPerSec: st.uploadRate),
                      "peers:", st.numPeers,
                      "seeds:", st.numSeeds,
                      st.state.rawValue)
            }
        }
    }

    static func humanize(bytesPerSec: Int64) -> String {
        let units = ["B/s","KB/s","MB/s","GB/s"]
        var value = Double(bytesPerSec)
        var unit = 0
        while value >= 1024.0 && unit < units.count-1 {
            value /= 1024.0
            unit += 1
        }
        return String(format: "%.1f %@", value, units[unit])
    }
}

if #available(macOS 13.0, *) {
    Task {
        do { try await CLT.run() } catch {
            fputs("\(error)\n", stderr)
            exit(1)
        }
    }
    dispatchMain()
} else {
    fputs("Requires macOS 13 or newer\n", stderr)
    exit(1)
}
#else
// Non-macOS platforms are not supported for this CLI target.
#endif
