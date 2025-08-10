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
    char info_hash[65];        // hex-encoded SHA1/SHA256 (null-terminated)
    char name[256];            // torrent name (best-effort, null-terminated)
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

// Alerts-based updates (session-wide)
void swbt_session_post_torrent_updates(swbt_session_t* session);
// Wait up to timeout_ms and collect torrent_status from state_update_alerts.
// Returns number of entries written to out_statuses (<= max_count).
int swbt_session_poll_updates(swbt_session_t* session,
                              int timeout_ms,
                              swbt_torrent_status_t* out_statuses,
                              int max_count);

// Identification
// Writes hex id (up to 64 hex chars + NUL) to out_hex buffer.
swbt_error_code_e swbt_torrent_infohash(swbt_torrent_handle_t* handle,
                                        char* out_hex,
                                        int out_len);

// Resume data
void swbt_torrent_save_resume(swbt_torrent_handle_t* handle);

typedef struct swbt_resume_data_t {
    char info_hash[65];     // hex id of torrent
    const uint8_t* data;    // owned buffer allocated by core
    int32_t size;           // size of data in bytes
} swbt_resume_data_t;

// Poll save_resume_data alerts. Returns number of items written (<= max_count).
int swbt_session_poll_resume(swbt_session_t* session,
                             int timeout_ms,
                             swbt_resume_data_t* out_items,
                             int max_count);

// Free buffers allocated within swbt_resume_data_t array returned from poll.
void swbt_resume_data_free(swbt_resume_data_t* items, int count);

// Add with resume data
swbt_error_code_e swbt_add_magnet_with_resume(swbt_session_t* session,
                                              const char* magnet_uri,
                                              const char* save_path,
                                              const uint8_t* resume_data,
                                              int resume_size,
                                              swbt_torrent_handle_t** out_handle);

swbt_error_code_e swbt_add_torrent_file_with_resume(swbt_session_t* session,
                                                    const char* torrent_file_path,
                                                    const char* save_path,
                                                    const uint8_t* resume_data,
                                                    int resume_size,
                                                    swbt_torrent_handle_t** out_handle);

// Overview listing (id + name) without handles
typedef struct swbt_torrent_overview_t {
    char info_hash[65];
    char name[256];
} swbt_torrent_overview_t;

int swbt_session_list_overview(swbt_session_t* session,
                               swbt_torrent_overview_t* out_items,
                               int max_count);

// Find torrent by hex info-hash and return a handle (caller owns and must free via remove_torrent)
swbt_error_code_e swbt_session_find_torrent(swbt_session_t* session,
                                            const char* info_hash_hex,
                                            swbt_torrent_handle_t** out_handle);

// File metadata and priorities
typedef struct swbt_file_info_t {
    int32_t index;
    int64_t size;
    int64_t offset;
    char path[512];
    int32_t priority; // 0=skip, 1..7 increasing priority
} swbt_file_info_t;

int64_t swbt_torrent_total_size(swbt_torrent_handle_t* handle);
int     swbt_torrent_file_count(swbt_torrent_handle_t* handle);
swbt_error_code_e swbt_torrent_file_info(swbt_torrent_handle_t* handle,
                                         int index,
                                         swbt_file_info_t* out_info);
void swbt_torrent_set_file_priority(swbt_torrent_handle_t* handle,
                                    int index,
                                    int priority);

// Alerts (simplified)
typedef enum swbt_alert_type_e {
    SWBT_ALERT_TORRENT_FINISHED = 1,
    SWBT_ALERT_TORRENT_ERROR = 2,
    SWBT_ALERT_METADATA_RECEIVED = 3,
    SWBT_ALERT_TRACKER_ERROR = 4
} swbt_alert_type_e;

typedef struct swbt_alert_t {
    swbt_alert_type_e type;
    char info_hash[65];
    int32_t error_code;     // if any
    char message[256];
} swbt_alert_t;

int swbt_session_poll_alerts(swbt_session_t* session,
                              int timeout_ms,
                              swbt_alert_t* out_alerts,
                              int max_count);

// Storage and rate limits
swbt_error_code_e swbt_torrent_move_storage(swbt_torrent_handle_t* handle,
                                            const char* new_path);

void swbt_session_set_rate_limits(swbt_session_t* session,
                                  int download_rate,
                                  int upload_rate);

void swbt_torrent_set_rate_limits(swbt_torrent_handle_t* handle,
                                  int download_rate,
                                  int upload_rate);

#ifdef __cplusplus
} // extern "C"
#endif


