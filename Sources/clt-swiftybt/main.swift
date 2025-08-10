import Foundation
import SwiftyBitTorrent

@main
struct CLT {
    static func main() async throws {
        var args = CommandLine.arguments
        _ = args.removeFirst() // executable name
        guard !args.isEmpty else {
            fputs("Usage: clt-swiftybt <magnet-or-torrent> [more...]\n", stderr)
            exit(2)
        }

        let session = BTSession(config: .init())
        var torrents: [BTTorrent] = []

        for a in args {
            if a.hasPrefix("magnet:") {
                let t = try await session.addTorrent(magnet: a)
                torrents.append(t)
            } else {
                let url = URL(fileURLWithPath: a)
                let t = try await session.addTorrent(fileURL: url)
                torrents.append(t)
            }
        }

        while true {
            print("\u{001B}[2J\u{001B}[H") // clear screen
            for (idx, t) in torrents.enumerated() {
                let st = await t.currentStatus()
                let pct = Int((st.progress * 100).rounded())
                print("[")
                print(String(format: "%2d", idx+1), terminator: "] ")
                print(String(format: "%3d%%", pct),
                      "down:", humanize(bytesPerSec: st.downloadRate),
                      "up:", humanize(bytesPerSec: st.uploadRate),
                      "peers:", st.numPeers,
                      "seeds:", st.numSeeds)
            }
            try? await Task.sleep(nanoseconds: 1_000_000_000)
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


