![Version](https://img.shields.io/badge/version-1.0-blue)
![Swift](https://img.shields.io/badge/Swift-6.0%2B-orange)
![Platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Linux-lightgrey)

## SwiftyBitTorrent

Thin Swift wrapper around arvidn/libtorrent (Rasterbar) with an optional macOS CLI tool.

### Features
- **Swift library**: minimal API for starting a session, adding torrents by file or magnet, removing/pause/resume, querying torrent status
- **Async/await first**: async operations and AsyncStream-based status updates
- **CLI tool** (`clt-swiftybt`): add one or many .torrent or magnet links, watch progress, peers/seeds, speeds; configurable download directory

### Requirements
- **Swift 5.9+** (Xcode 15+) and **macOS 13+** for async/await in the public API
- Libtorrent (Rasterbar) 2.0.x installed on the system
  - macOS (Homebrew): `brew install libtorrent-rasterbar`

Notes
- The CLI target is macOS-only. Building it for iOS is not supported.
- The library is annotated with `@available(iOS 13.0, macOS 13.0, *)` for async APIs.

### Installation (as a dependency)
Add to your `Package.swift`:

```swift
// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "YourApp",
    platforms: [ .macOS(.v13), .iOS(.v13) ],
    dependencies: [
        .package(url: "https://github.com/your-org-or-user/SwiftyLibTorrent.git", branch: "main")
    ],
    targets: [
        .target(
            name: "YourAppTarget",
            dependencies: [
                .product(name: "SwiftyBitTorrent", package: "SwiftyLibTorrent")
            ]
        )
    ]
)
```

Then: `brew install libtorrent-rasterbar` (for macOS builds).

### Build from source
```bash
brew install libtorrent-rasterbar
swift build
```

### CLI tool (macOS)
After `swift build`, the binary is at:
- `.build/arm64-apple-macosx/debug/clt-swiftybt`

Usage:
```bash
# default output dir is "torrent_downloads" next to the binary
.build/arm64-apple-macosx/debug/clt-swiftybt <magnet-or-torrent> [more...]

# custom output dir
.build/arm64-apple-macosx/debug/clt-swiftybt --dir /path/to/output <magnet-or-torrent>
```

Examples:
```bash
.build/arm64-apple-macosx/debug/clt-swiftybt /path/file.torrent
.build/arm64-apple-macosx/debug/clt-swiftybt "magnet:?xt=urn:btih:..." "magnet:?xt=urn:btih:..."
```

Xcode run notes:
- Scheme: `clt-swiftybt`
- Destination: My Mac (Apple Silicon)

### Swift API (high level)

Types
- `BTSessionConfig` (macOS/iOS 13+)
  - `savePath: URL?`, `listenPort: Int`, discovery toggles (DHT/LSD/UPnP/NATPMP), rate limits, `postStatusIntervalMs`
- `BTSession` (macOS/iOS 13+)
  - `init(config:)`
  - `addTorrent(magnet:) async throws -> BTTorrent`
  - `addTorrent(fileURL:) async throws -> BTTorrent`
  - `removeTorrent(_:withData:) async`
  - `statusUpdatesStream(intervalMs:batch:) -> AsyncStream<[BTTorrentStatus]>`
- `BTTorrent` (macOS/iOS 13+)
  - `pause()`, `resume()`, `forceReannounce()`
  - `status() -> BTTorrentStatus`
  - `currentStatus() async -> BTTorrentStatus`
  - `statusStream(intervalSeconds:) -> AsyncStream<BTTorrentStatus>`
- `BTTorrentStatus`
  - `progress: Double`, `downloadRate/uploadRate: Int64`, `totalDownloaded/totalUploaded: Int64`, `numPeers/numSeeds: Int`, `state: BTTorrentState`, `hasMetadata: Bool`

Minimal usage
```swift
import SwiftyBitTorrent

@available(macOS 13.0, iOS 13.0, *)
func startDownload() async throws {
    let session = BTSession(config: .init(savePath: URL(fileURLWithPath: "/path/to/output")))
    let torrent = try await session.addTorrent(magnet: "magnet:?xt=urn:btih:...")

    // Single snapshot
    let snapshot = await torrent.currentStatus()
    print("progress:", snapshot.progress)

    // Stream updates for that torrent
    for await st in torrent.statusStream(intervalSeconds: 1) {
        print(st.progress, st.downloadRate, st.numPeers)
        if st.progress >= 1.0 { break }
    }
}
```

Session-wide updates (alerts-based)
```swift
let session = BTSession(config: .init())
for await batch in session.statusUpdatesStream(intervalMs: 1000) {
    for st in batch {
        print(st.state, st.progress)
    }
}
```

### Troubleshooting
- **"Building for 'iOS', but linking in dylib built for 'macOS'"**
  - The CLI is macOS-only. Select `My Mac` destination. Library links to `libtorrent-rasterbar` only on macOS.
- **`'libtorrent/session.hpp' file not found`**
  - Install Homebrew package: `brew install libtorrent-rasterbar`
  - Ensure Xcode uses the macOS toolchain and destination `My Mac` for the CLI.
- **`'main' attribute cannot be used in a module that contains top-level code'`**
  - Use the `clt-swiftybt` scheme with `My Mac` destination. Clean Build Folder. The project uses a top-level `dispatchMain()` entry for the CLI.

### Acknowledgements
- Built on top of libtorrent (Rasterbar): [arvidn/libtorrent](https://github.com/arvidn/libtorrent)

### License
- SwiftyBitTorrent: MIT
- libtorrent (Rasterbar): BSD-3-Clause (see upstream)

Notes on licensing
- BSD-3-Clause of libtorrent permits using an MIT license for this Swift wrapper. Keep attribution and the BSD disclaimer when redistributing binaries/sources that include libtorrent.
- If you ship the CLI or any binary that links against libtorrent, include third‑party license texts (libtorrent, Boost, and any TLS libs in use) in your distribution.
- Reference: libtorrent BSD-3-Clause license text is available upstream: [LICENSE](https://github.com/arvidn/libtorrent/blob/RC_2_0/LICENSE).


