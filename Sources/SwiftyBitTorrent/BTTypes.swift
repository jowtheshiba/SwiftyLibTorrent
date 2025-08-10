import Foundation

@available(iOS 13.0, macOS 13.0, *)
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

@available(iOS 13.0, macOS 13.0, *)
public enum BTTorrentState: String, Sendable {
    case unknown
    case checking
    case downloading
    case seeding
    case paused
    case queued
    case error
}

@available(iOS 13.0, macOS 13.0, *)
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
    public let id: String
    public let name: String
}

@available(iOS 13.0, macOS 13.0, *)
public struct BTTorrentOverview: Sendable {
    public let id: String
    public let name: String
}

@available(iOS 13.0, macOS 13.0, *)
public struct BTFileInfo: Sendable {
    public let index: Int
    public let size: Int64
    public let offset: Int64
    public let path: String
    public let priority: Int
}

@available(iOS 13.0, macOS 13.0, *)
public enum BTAlertType: Int, Sendable {
    case torrentFinished = 1
    case torrentError = 2
    case metadataReceived = 3
    case trackerError = 4
}

@available(iOS 13.0, macOS 13.0, *)
public struct BTAlert: Sendable {
    public let type: BTAlertType
    public let id: String
    public let errorCode: Int
    public let message: String
}

@available(iOS 13.0, macOS 13.0, *)
public struct BTResumeDataItem: Sendable {
    public let id: String
    public let data: Data
}


