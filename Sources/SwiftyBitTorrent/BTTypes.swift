import Foundation

public struct BTSessionConfig {
    public var savePath: URL?
    public var listenPort: Int
    public var enableDHT: Bool
    public var enableLSD: Bool
    public var enableUPnP: Bool
    public var enableNATPMP: Bool
    public var downloadRateLimit: Int?
    public var uploadRateLimit: Int?
    public var postStatusIntervalMs: Int

    public init(
        savePath: URL? = nil,
        listenPort: Int = 0,
        enableDHT: Bool = true,
        enableLSD: Bool = true,
        enableUPnP: Bool = true,
        enableNATPMP: Bool = true,
        downloadRateLimit: Int? = nil,
        uploadRateLimit: Int? = nil,
        postStatusIntervalMs: Int = 1000
    ) {
        self.savePath = savePath
        self.listenPort = listenPort
        self.enableDHT = enableDHT
        self.enableLSD = enableLSD
        self.enableUPnP = enableUPnP
        self.enableNATPMP = enableNATPMP
        self.downloadRateLimit = downloadRateLimit
        self.uploadRateLimit = uploadRateLimit
        self.postStatusIntervalMs = postStatusIntervalMs
    }
}

public enum BTTorrentState: String, Sendable {
    case unknown
    case checking
    case downloading
    case seeding
    case paused
    case queued
    case error
}

public struct BTTorrentStatus: Sendable {
    public let progress: Double
    public let downloadRate: Int64
    public let uploadRate: Int64
    public let totalDownloaded: Int64
    public let totalUploaded: Int64
    public let numPeers: Int
    public let numSeeds: Int
    public let state: BTTorrentState
    public let hasMetadata: Bool
}


