// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "SwiftyBitTorrent",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .library(name: "SwiftyBitTorrent", targets: ["SwiftyBitTorrent"]),
        .executable(name: "clt-swiftybt", targets: ["clt-swiftybt"])
    ],
    targets: [
        // System library to obtain include and link flags from pkg-config libtorrent-rasterbar
        .systemLibrary(
            name: "CLibtorrentRB",
            pkgConfig: "libtorrent-rasterbar",
            providers: [
                .brew(["libtorrent-rasterbar"]) // Homebrew formula name
            ]
        ),
        // C++ bridge exposing a C API for Swift to call
        .target(
            name: "SwiftyBitTorrentCore",
            dependencies: [
                .target(name: "CLibtorrentRB", condition: .when(platforms: [.macOS]))
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .unsafeFlags(["-std=c++17"], .when(platforms: [.macOS])),
                .unsafeFlags(["-I/opt/homebrew/include", "-I/usr/local/include"], .when(platforms: [.macOS]))
            ],
            linkerSettings: [
                .unsafeFlags(["-L/opt/homebrew/lib", "-L/usr/local/lib"], .when(platforms: [.macOS])),
                .linkedLibrary("torrent-rasterbar", .when(platforms: [.macOS]))
            ]
        ),
        // Swift wrapper API
        .target(
            name: "SwiftyBitTorrent",
            dependencies: ["SwiftyBitTorrentCore"]
        ),
        // CLI tool
        .executableTarget(
            name: "clt-swiftybt",
            dependencies: ["SwiftyBitTorrent"],
            swiftSettings: [
                .define("SWIFTYBT_CLI", .when(platforms: [.macOS]))
            ]
        )
    ]
)


