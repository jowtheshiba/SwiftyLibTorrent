import Foundation
import SwiftyBitTorrentCore

@available(iOS 13.0, macOS 13.0, *)
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

    @available(iOS 13.0, macOS 13.0, *)
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

    @available(iOS 13.0, macOS 13.0, *)
    public func addTorrent(magnet: String, resumeData: Data?, savePath: URL? = nil) async throws -> BTTorrent {
        guard let raw else { throw NSError(domain: "SwiftyBT", code: -1) }
        var handlePtr: UnsafeMutablePointer<swbt_torrent_handle_t>? = nil
        let code = magnet.withCString { m in
            if let rd = resumeData {
                return rd.withUnsafeBytes { rb in
                    let base = rb.baseAddress?.assumingMemoryBound(to: UInt8.self)
                    return savePath?.path.withCString { s in
                        swbt_add_magnet_with_resume(raw, m, s, base, Int32(rb.count), &handlePtr)
                    } ?? swbt_add_magnet_with_resume(raw, m, nil, base, Int32(rb.count), &handlePtr)
                }
            } else {
                return savePath?.path.withCString { s in
                    swbt_add_magnet(raw, m, s, &handlePtr)
                } ?? swbt_add_magnet(raw, m, nil, &handlePtr)
            }
        }
        guard code == SWBT_OK, let hp = handlePtr else { throw NSError(domain: "SwiftyBT", code: Int(code.rawValue)) }
        return BTTorrent(handle: hp)
    }

    @available(iOS 13.0, macOS 13.0, *)
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

    @available(iOS 13.0, macOS 13.0, *)
    public func addTorrent(fileURL: URL, resumeData: Data?, savePath: URL? = nil) async throws -> BTTorrent {
        guard let raw else { throw NSError(domain: "SwiftyBT", code: -1) }
        var handlePtr: UnsafeMutablePointer<swbt_torrent_handle_t>? = nil
        let code = fileURL.path.withCString { p in
            if let rd = resumeData {
                return rd.withUnsafeBytes { rb in
                    let base = rb.baseAddress?.assumingMemoryBound(to: UInt8.self)
                    return savePath?.path.withCString { s in
                        swbt_add_torrent_file_with_resume(raw, p, s, base, Int32(rb.count), &handlePtr)
                    } ?? swbt_add_torrent_file_with_resume(raw, p, nil, base, Int32(rb.count), &handlePtr)
                }
            } else {
                return savePath?.path.withCString { s in
                    swbt_add_torrent_file(raw, p, s, &handlePtr)
                } ?? swbt_add_torrent_file(raw, p, nil, &handlePtr)
            }
        }
        guard code == SWBT_OK, let hp = handlePtr else { throw NSError(domain: "SwiftyBT", code: Int(code.rawValue)) }
        return BTTorrent(handle: hp)
    }

    @available(iOS 13.0, macOS 13.0, *)
    public func removeTorrent(_ torrent: BTTorrent, withData: Bool = false) async {
        guard let raw else { return }
        swbt_remove_torrent(raw, torrent.handle, withData ? 1 : 0)
    }

    @available(iOS 13.0, macOS 13.0, *)
    public func listTorrents(max: Int = 1024) -> [BTTorrentOverview] {
        guard let raw else { return [] }
        var buffer = Array(repeating: swbt_torrent_overview_t(), count: max)
        let n = buffer.withUnsafeMutableBufferPointer { buf in
            swbt_session_list_overview(raw, buf.baseAddress, Int32(max))
        }
        if n <= 0 { return [] }
        return (0..<Int(n)).map { i in
            var o = buffer[i]
            let id = withUnsafePointer(to: &o.info_hash) { ptr in
                ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
            }
            let name = withUnsafePointer(to: &o.name) { ptr in
                ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
            }
            return BTTorrentOverview(id: id, name: name)
        }
    }

    @available(iOS 13.0, macOS 13.0, *)
    public func alertsStream(pollIntervalMs: Int = 500, batch: Int = 256) -> AsyncStream<[BTAlert]> {
        AsyncStream { continuation in
            let task = Task {
                var buffer = Array(repeating: swbt_alert_t(), count: batch)
                while !Task.isCancelled {
                    if let raw {
                        let n = buffer.withUnsafeMutableBufferPointer { buf in
                            swbt_session_poll_alerts(raw, Int32(pollIntervalMs), buf.baseAddress, Int32(batch))
                        }
                        if n > 0 {
                            let mapped: [BTAlert] = (0..<Int(n)).map { i in
                                var a = buffer[i]
                                let id = withUnsafePointer(to: &a.info_hash) { ptr in
                                    ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
                                }
                                let message = withUnsafePointer(to: &a.message) { ptr in
                                    ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
                                }
                                return BTAlert(type: BTAlertType(rawValue: Int(a.type.rawValue)) ?? .torrentError, id: id, errorCode: Int(a.error_code), message: message)
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

    @available(iOS 13.0, macOS 13.0, *)
    public func pollResumeData(timeoutMs: Int = 1000, batch: Int = 128) -> [BTResumeDataItem] {
        guard let raw else { return [] }
        var buffer = Array(repeating: swbt_resume_data_t(), count: batch)
        let n = buffer.withUnsafeMutableBufferPointer { buf in
            swbt_session_poll_resume(raw, Int32(timeoutMs), buf.baseAddress, Int32(batch))
        }
        var result: [BTResumeDataItem] = []
        if n > 0 {
            result.reserveCapacity(Int(n))
            for i in 0..<Int(n) {
                var it = buffer[i]
                let id = withUnsafePointer(to: &it.info_hash) { ptr in
                    ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
                }
                let data = Data(bytes: it.data, count: Int(it.size))
                result.append(BTResumeDataItem(id: id, data: data))
            }
            // free buffers
            buffer.withUnsafeMutableBufferPointer { buf in
                swbt_resume_data_free(buf.baseAddress, Int32(n))
            }
        }
        return result
    }

    @available(iOS 13.0, macOS 13.0, *)
    public func setRateLimits(download: Int?, upload: Int?) {
        guard let raw else { return }
        swbt_session_set_rate_limits(raw, Int32(download ?? -1), Int32(upload ?? -1))
    }

    @available(iOS 13.0, macOS 13.0, *)
    public func statusUpdatesStream(intervalMs: Int = 1000, batch: Int = 256) -> AsyncStream<[BTTorrentStatus]> {
        AsyncStream { continuation in
            let task = Task {
                var buffer = Array(repeating: swbt_torrent_status_t(), count: batch)
                while !Task.isCancelled {
                    if let raw {
                        swbt_session_post_torrent_updates(raw)
                        let n = buffer.withUnsafeMutableBufferPointer { buf in
                            swbt_session_poll_updates(raw, Int32(intervalMs), buf.baseAddress, Int32(batch))
                        }
                        if n > 0 {
                            let mapped: [BTTorrentStatus] = (0..<Int(n)).map { i in
                                var s = buffer[i]
                                let st: BTTorrentState
                                switch s.state {
                                case 0, 6: st = .checking
                                case 1, 2: st = .downloading
                                case 3:    st = .downloading
                                case 4:    st = .seeding
                                default:   st = .unknown
                                }
                                let id = withUnsafePointer(to: &s.info_hash) { ptr in
                                    ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
                                }
                                let name = withUnsafePointer(to: &s.name) { ptr in
                                    ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
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
                                    hasMetadata: s.has_metadata != 0,
                                    id: id,
                                    name: name
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

@available(iOS 13.0, macOS 13.0, *)
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
        var cstatus = swbt_torrent_status_t()
        _ = swbt_torrent_status(handle, &cstatus)
        let mappedState: BTTorrentState
        switch cstatus.state {
        case 0, 6: mappedState = .checking           // checking_files / checking_resume_data
        case 1, 2: mappedState = .downloading        // downloading_metadata / downloading
        case 3:     mappedState = .downloading        // finished (not yet seeding)
        case 4:     mappedState = .seeding
        default:    mappedState = .unknown
        }
        let id = withUnsafePointer(to: &cstatus.info_hash) { ptr in
            ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
        }
        let name = withUnsafePointer(to: &cstatus.name) { ptr in
            ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
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
            hasMetadata: cstatus.has_metadata != 0,
            id: id,
            name: name
        )
    }

    @available(iOS 13.0, macOS 13.0, *)
    public func currentStatus() async -> BTTorrentStatus {
        status()
    }

    @available(iOS 13.0, macOS 13.0, *)
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

    public func id() -> String {
        var buf = [CChar](repeating: 0, count: 65)
        _ = swbt_torrent_infohash(handle, &buf, 65)
        return String(cString: buf)
    }

    public func saveResumeData() { swbt_torrent_save_resume(handle) }

    public func totalSize() -> Int64 { swbt_torrent_total_size(handle) }

    public func files() -> [BTFileInfo] {
        let count = Int(swbt_torrent_file_count(handle))
        guard count > 0 else { return [] }
        var result: [BTFileInfo] = []
        result.reserveCapacity(count)
        for i in 0..<count {
            var fi = swbt_file_info_t()
            if swbt_torrent_file_info(handle, Int32(i), &fi) == SWBT_OK {
                let path = withUnsafePointer(to: &fi.path) { ptr in
                    ptr.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
                }
                result.append(BTFileInfo(index: Int(fi.index), size: fi.size, offset: fi.offset, path: path, priority: Int(fi.priority)))
            }
        }
        return result
    }

    public func setFilePriority(index: Int, priority: Int) {
        swbt_torrent_set_file_priority(handle, Int32(index), Int32(priority))
    }

    public func moveStorage(to newPath: URL) {
        newPath.path.withCString { p in _ = swbt_torrent_move_storage(handle, p) }
    }

    public func setRateLimits(download: Int?, upload: Int?) {
        swbt_torrent_set_rate_limits(handle, Int32(download ?? -1), Int32(upload ?? -1))
    }
}


