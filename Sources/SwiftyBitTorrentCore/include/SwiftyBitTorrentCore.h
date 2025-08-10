#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Opaque wrapper types exposed to Swift. Implementation details are hidden
// behind an internal pointer.
typedef struct swbt_session_t { void* impl; } swbt_session_t;
typedef struct swbt_torrent_handle_t { void* impl; } swbt_torrent_handle_t;

typedef enum swbt_error_code_e {
    SWBT_OK = 0,
    SWBT_ERR_GENERIC = 1,
    SWBT_ERR_INVALID_ARG = 2
} swbt_error_code_e;

typedef struct swbt_session_config_t {
    const char* save_path; // optional default save path
    int32_t listen_port;   // 0 for auto
    int32_t enable_dht;    // bool
    int32_t enable_lsd;    // bool
    int32_t enable_upnp;   // bool
    int32_t enable_natpmp; // bool
    int32_t download_rate_limit; // bytes/sec, 0 = unlimited
    int32_t upload_rate_limit;   // bytes/sec, 0 = unlimited
    int32_t post_status_interval_ms; // e.g. 1000
} swbt_session_config_t;

typedef struct swbt_torrent_status_t {
    double progress;           // 0.0..1.0
    int64_t download_rate;     // bytes/sec
    int64_t upload_rate;       // bytes/sec
    int64_t total_downloaded;  // bytes
    int64_t total_uploaded;    // bytes
    int32_t num_peers;
    int32_t num_seeds;
    int32_t state;             // implementation-defined
    int32_t has_metadata;      // bool
} swbt_torrent_status_t;

// Session lifecycle
swbt_session_t* swbt_session_new(const swbt_session_config_t* config);
void swbt_session_free(swbt_session_t* session);

// Torrent operations
swbt_error_code_e swbt_add_magnet(swbt_session_t* session,
                                  const char* magnet_uri,
                                  const char* save_path,
                                  swbt_torrent_handle_t** out_handle);

swbt_error_code_e swbt_add_torrent_file(swbt_session_t* session,
                                        const char* torrent_file_path,
                                        const char* save_path,
                                        swbt_torrent_handle_t** out_handle);

void swbt_remove_torrent(swbt_session_t* session,
                         swbt_torrent_handle_t* handle,
                         int with_data);

// Control
void swbt_torrent_pause(swbt_torrent_handle_t* handle);
void swbt_torrent_resume(swbt_torrent_handle_t* handle);
void swbt_torrent_force_reannounce(swbt_torrent_handle_t* handle);

// Status
swbt_error_code_e swbt_torrent_status(swbt_torrent_handle_t* handle,
                                      swbt_torrent_status_t* out_status);

#ifdef __cplusplus
} // extern "C"
#endif


