import Foundation
import SwiftyBitTorrentCore

public final class BTSession {
    private var raw: UnsafeMutablePointer<swbt_session_t>?
    private let eventQueue = DispatchQueue(label: "swiftybt.session.events")

    public init(config: BTSessionConfig = .init()) {
        var savePathCString: [CChar]? = config.savePath?.path.cString(using: .utf8)
        let savePtr: UnsafePointer<CChar>? = savePathCString?.withUnsafeMutableBufferPointer { buf in
            UnsafePointer<CChar>(buf.baseAddress)
        }
        var c = swbt_session_config_t(
            save_path: savePtr,
            listen_port: Int32(config.listenPort),
            enable_dht: config.enableDHT ? 1 : 0,
            enable_lsd: config.enableLSD ? 1 : 0,
            enable_upnp: config.enableUPnP ? 1 : 0,
            enable_natpmp: config.enableNATPMP ? 1 : 0,
            download_rate_limit: Int32(config.downloadRateLimit ?? 0),
            upload_rate_limit: Int32(config.uploadRateLimit ?? 0),
            post_status_interval_ms: Int32(config.postStatusIntervalMs)
        )

        self.raw = swbt_session_new(&c)
    }

    deinit {
        if let raw { swbt_session_free(raw) }
    }

    public func addTorrent(magnet: String, savePath: URL? = nil) async throws -> BTTorrent {
        guard let raw else { throw NSError(domain: "SwiftyBT", code: -1) }
        var handlePtr: UnsafeMutablePointer<swbt_torrent_handle_t>? = nil
        let code = magnet.withCString { m in
            savePath?.path.withCString { s in
                swbt_add_magnet(raw, m, s, &handlePtr)
            } ?? swbt_add_magnet(raw, m, nil, &handlePtr)
        }
        guard code == SWBT_OK, let hp = handlePtr else { throw NSError(domain: "SwiftyBT", code: Int(code.rawValue)) }
        return BTTorrent(handle: hp)
    }

    public func addTorrent(fileURL: URL, savePath: URL? = nil) async throws -> BTTorrent {
        guard let raw else { throw NSError(domain: "SwiftyBT", code: -1) }
        var handlePtr: UnsafeMutablePointer<swbt_torrent_handle_t>? = nil
        let code = fileURL.path.withCString { p in
            savePath?.path.withCString { s in
                swbt_add_torrent_file(raw, p, s, &handlePtr)
            } ?? swbt_add_torrent_file(raw, p, nil, &handlePtr)
        }
        guard code == SWBT_OK, let hp = handlePtr else { throw NSError(domain: "SwiftyBT", code: Int(code.rawValue)) }
        return BTTorrent(handle: hp)
    }

    public func removeTorrent(_ torrent: BTTorrent, withData: Bool = false) async {
        guard let raw else { return }
        swbt_remove_torrent(raw, torrent.handle, withData ? 1 : 0)
    }

    public func statusUpdatesStream(intervalMs: Int = 1000, batch: Int = 256) -> AsyncStream<[BTTorrentStatus]> {
        AsyncStream { continuation in
            let task = Task {
                var buffer = Array(repeating: swbt_torrent_status_t(
                    progress: 0, download_rate: 0, upload_rate: 0,
                    total_downloaded: 0, total_uploaded: 0,
                    num_peers: 0, num_seeds: 0, state: 0, has_metadata: 0
                ), count: batch)
                while !Task.isCancelled {
                    if let raw {
                        swbt_session_post_torrent_updates(raw)
                        let n = buffer.withUnsafeMutableBufferPointer { buf in
                            swbt_session_poll_updates(raw, Int32(intervalMs), buf.baseAddress, Int32(batch))
                        }
                        if n > 0 {
                            let mapped: [BTTorrentStatus] = (0..<Int(n)).map { i in
                                let s = buffer[i]
                                let st: BTTorrentState
                                switch s.state {
                                case 0, 6: st = .checking
                                case 1, 2: st = .downloading
                                case 3:    st = .downloading
                                case 4:    st = .seeding
                                default:   st = .unknown
                                }
                                return BTTorrentStatus(
                                    progress: s.progress,
                                    downloadRate: s.download_rate,
                                    uploadRate: s.upload_rate,
                                    totalDownloaded: s.total_downloaded,
                                    totalUploaded: s.total_uploaded,
                                    numPeers: Int(s.num_peers),
                                    numSeeds: Int(s.num_seeds),
                                    state: st,
                                    hasMetadata: s.has_metadata != 0
                                )
                            }
                            continuation.yield(mapped)
                        }
                    }
                }
                continuation.finish()
            }
            continuation.onTermination = { _ in task.cancel() }
        }
    }
}

public final class BTTorrent {
    fileprivate let handle: UnsafeMutablePointer<swbt_torrent_handle_t>

    init(handle: UnsafeMutablePointer<swbt_torrent_handle_t>) {
        self.handle = handle
    }

    deinit {
        // Freed via remove. If leaked, core stub will free on remove only.
    }

    public func pause() { swbt_torrent_pause(handle) }
    public func resume() { swbt_torrent_resume(handle) }
    public func forceReannounce() { swbt_torrent_force_reannounce(handle) }

    public func status() -> BTTorrentStatus {
        var cstatus = swbt_torrent_status_t(
            progress: 0, download_rate: 0, upload_rate: 0,
            total_downloaded: 0, total_uploaded: 0,
            num_peers: 0, num_seeds: 0, state: 0, has_metadata: 0
        )
        _ = swbt_torrent_status(handle, &cstatus)
        let mappedState: BTTorrentState
        switch cstatus.state {
        case 0, 6: mappedState = .checking           // checking_files / checking_resume_data
        case 1, 2: mappedState = .downloading        // downloading_metadata / downloading
        case 3:     mappedState = .downloading        // finished (not yet seeding)
        case 4:     mappedState = .seeding
        default:    mappedState = .unknown
        }
        return BTTorrentStatus(
            progress: cstatus.progress,
            downloadRate: cstatus.download_rate,
            uploadRate: cstatus.upload_rate,
            totalDownloaded: cstatus.total_downloaded,
            totalUploaded: cstatus.total_uploaded,
            numPeers: Int(cstatus.num_peers),
            numSeeds: Int(cstatus.num_seeds),
            state: mappedState,
            hasMetadata: cstatus.has_metadata != 0
        )
    }

    public func currentStatus() async -> BTTorrentStatus {
        status()
    }

    public func statusStream(intervalSeconds: Double = 1.0) -> AsyncStream<BTTorrentStatus> {
        AsyncStream { continuation in
            let task = Task {
                while !Task.isCancelled {
                    continuation.yield(self.status())
                    try? await Task.sleep(nanoseconds: UInt64(intervalSeconds * 1_000_000_000))
                }
                continuation.finish()
            }
            continuation.onTermination = { _ in task.cancel() }
        }
    }
}


