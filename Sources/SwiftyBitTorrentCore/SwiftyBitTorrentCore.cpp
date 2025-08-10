#include "SwiftyBitTorrentCore.h"

#include <string>
#include <memory>
#include <cstdio>
#include <vector>
#include <cstring>
#include <algorithm>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(SWBT_USE_LIBTORRENT)
// libtorrent (macOS only)
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/write_resume_data.hpp>

namespace lt = libtorrent;

struct SwbtSessionImpl {
    std::unique_ptr<lt::session> session;
    std::string default_save_path;
};

struct SwbtTorrentHandleImpl {
    lt::torrent_handle handle;
};

static void hex_encode(const unsigned char* data, int len, char* out, int out_len) {
    static const char* hex = "0123456789abcdef";
    int need = len * 2 + 1;
    if (out_len <= 0) return;
    int n = std::min(need, out_len);
    int pairs = std::min(len, (out_len - 1) / 2);
    for (int i = 0; i < pairs; ++i) {
        out[i * 2] = hex[(data[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[data[i] & 0xF];
    }
    out[std::min(out_len - 1, pairs * 2)] = '\0';
}

static void fill_infohash_hex(const lt::torrent_handle& th, char* out, int out_len) {
#if LT_VERSION_NUM >= 10200
    auto ih = th.info_hashes();
    if (ih.has_v2()) {
        auto v2 = ih.v2;
        hex_encode(reinterpret_cast<const unsigned char*>(v2.data()), v2.size(), out, out_len);
    } else if (ih.has_v1()) {
        auto v1 = ih.v1;
        hex_encode(reinterpret_cast<const unsigned char*>(v1.data()), v1.size(), out, out_len);
    } else {
        if (out_len > 0) out[0] = '\0';
    }
#else
    auto ih = th.info_hash();
    hex_encode(reinterpret_cast<const unsigned char*>(ih.data()), ih.size(), out, out_len);
#endif
}

static void copy_cstr_safe(char* dst, int dst_len, const std::string& s) {
    if (dst_len <= 0) return;
    std::size_t n = std::min<std::size_t>(dst_len - 1, s.size());
    std::memcpy(dst, s.data(), n);
    dst[n] = '\0';
}

static lt::settings_pack build_settings(const swbt_session_config_t* c) {
    lt::settings_pack pack;
    if (c) {
        // Networking / discovery toggles
        pack.set_bool(lt::settings_pack::enable_dht, c->enable_dht != 0);
        pack.set_bool(lt::settings_pack::enable_lsd, c->enable_lsd != 0);
        pack.set_bool(lt::settings_pack::enable_upnp, c->enable_upnp != 0);
        pack.set_bool(lt::settings_pack::enable_natpmp, c->enable_natpmp != 0);
        if (c->download_rate_limit > 0) pack.set_int(lt::settings_pack::download_rate_limit, c->download_rate_limit);
        if (c->upload_rate_limit > 0) pack.set_int(lt::settings_pack::upload_rate_limit, c->upload_rate_limit);

        if (c->listen_port > 0) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "0.0.0.0:%d,[::]:%d", c->listen_port, c->listen_port);
            pack.set_str(lt::settings_pack::listen_interfaces, buf);
        }
    }
    return pack;
}

swbt_session_t* swbt_session_new(const swbt_session_config_t* config) {
    auto s = new swbt_session_t{};
    auto impl = new SwbtSessionImpl{};
    lt::settings_pack pack = build_settings(config);
    impl->session = std::make_unique<lt::session>(pack);
    if (config && config->save_path) impl->default_save_path = config->save_path;
    s->impl = impl;
    return s;
}

void swbt_session_free(swbt_session_t* session) {
    if (!session) return;
    delete static_cast<SwbtSessionImpl*>(session->impl);
    delete session;
}

swbt_error_code_e swbt_add_magnet(swbt_session_t* session,
                                  const char* magnet_uri,
                                  const char* save_path,
                                  swbt_torrent_handle_t** out_handle) {
    if (!session || !magnet_uri || !out_handle) return SWBT_ERR_INVALID_ARG;
    lt::error_code ec;
    lt::add_torrent_params p = lt::parse_magnet_uri(magnet_uri, ec);
    if (ec) return SWBT_ERR_GENERIC;
    if (save_path && save_path[0] != '\0') p.save_path = save_path;
    else if (!static_cast<SwbtSessionImpl*>(session->impl)->default_save_path.empty()) p.save_path = static_cast<SwbtSessionImpl*>(session->impl)->default_save_path;
    else p.save_path = ".";

    lt::torrent_handle th = static_cast<SwbtSessionImpl*>(session->impl)->session->add_torrent(std::move(p), ec);
    if (ec) return SWBT_ERR_GENERIC;
    auto h = new swbt_torrent_handle_t{};
    h->impl = new SwbtTorrentHandleImpl{th};
    *out_handle = h;
    return SWBT_OK;
}

swbt_error_code_e swbt_add_torrent_file(swbt_session_t* session,
                                        const char* torrent_file_path,
                                        const char* save_path,
                                        swbt_torrent_handle_t** out_handle) {
    if (!session || !torrent_file_path || !out_handle) return SWBT_ERR_INVALID_ARG;
    lt::error_code ec;
    auto ti = std::make_shared<lt::torrent_info>(torrent_file_path, ec);
    if (ec) return SWBT_ERR_GENERIC;
    lt::add_torrent_params p;
    p.ti = std::move(ti);
    if (save_path && save_path[0] != '\0') p.save_path = save_path;
    else if (!static_cast<SwbtSessionImpl*>(session->impl)->default_save_path.empty()) p.save_path = static_cast<SwbtSessionImpl*>(session->impl)->default_save_path;
    else p.save_path = ".";

    lt::torrent_handle th = static_cast<SwbtSessionImpl*>(session->impl)->session->add_torrent(std::move(p), ec);
    if (ec) return SWBT_ERR_GENERIC;
    auto h = new swbt_torrent_handle_t{};
    h->impl = new SwbtTorrentHandleImpl{th};
    *out_handle = h;
    return SWBT_OK;
}

void swbt_remove_torrent(swbt_session_t* session,
                         swbt_torrent_handle_t* handle,
                         int with_data) {
    if (!session || !handle) return;
    lt::remove_flags_t flags = {};
    if (with_data) flags |= lt::session::delete_files;
    auto himpl = static_cast<SwbtTorrentHandleImpl*>(handle->impl);
    static_cast<SwbtSessionImpl*>(session->impl)->session->remove_torrent(himpl->handle, flags);
    delete himpl;
    delete handle;
}

void swbt_torrent_pause(swbt_torrent_handle_t* handle) {
    if (!handle) return;
    static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.pause();
}

void swbt_torrent_resume(swbt_torrent_handle_t* handle) {
    if (!handle) return;
    static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.resume();
}

void swbt_torrent_force_reannounce(swbt_torrent_handle_t* handle) {
    if (!handle) return;
    static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.force_reannounce();
}

swbt_error_code_e swbt_torrent_status(swbt_torrent_handle_t* handle,
                                      swbt_torrent_status_t* out_status) {
    if (!handle || !out_status) return SWBT_ERR_INVALID_ARG;
    lt::status_flags_t flags = lt::torrent_handle::query_name
        | lt::torrent_handle::query_accurate_download_counters;
    lt::torrent_status st = static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.status(flags);

    out_status->progress = st.progress;
    out_status->download_rate = st.download_rate;
    out_status->upload_rate = st.upload_rate;
    out_status->total_downloaded = st.total_download;
    out_status->total_uploaded = st.total_upload;
    out_status->num_peers = st.num_peers;
#ifdef __APPLE__
    // num_seeds exists in libtorrent 1.2/2.0; if absent, leave zero
#endif
    out_status->num_seeds = st.num_seeds;
    out_status->state = static_cast<int32_t>(st.state);
    out_status->has_metadata = st.has_metadata ? 1 : 0;
    // id and name
    fill_infohash_hex(static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle, out_status->info_hash, sizeof(out_status->info_hash));
    copy_cstr_safe(out_status->name, sizeof(out_status->name), st.name);
    return SWBT_OK;
}

void swbt_session_post_torrent_updates(swbt_session_t* session) {
    if (!session) return;
    static_cast<SwbtSessionImpl*>(session->impl)->session->post_torrent_updates();
}

int swbt_session_poll_updates(swbt_session_t* session,
                              int timeout_ms,
                              swbt_torrent_status_t* out_statuses,
                              int max_count) {
    if (!session || !out_statuses || max_count <= 0) return 0;
    lt::session* s = static_cast<SwbtSessionImpl*>(session->impl)->session.get();
    s->wait_for_alert(lt::milliseconds(timeout_ms));
    std::vector<lt::alert*> alerts;
    s->pop_alerts(&alerts);
    int written = 0;
    for (lt::alert* a : alerts) {
        if (auto* upd = lt::alert_cast<lt::state_update_alert>(a)) {
            for (const auto& st : upd->status) {
                if (written >= max_count) return written;
                swbt_torrent_status_t& o = out_statuses[written++];
                o.progress = st.progress;
                o.download_rate = st.download_rate;
                o.upload_rate = st.upload_rate;
                o.total_downloaded = st.total_download;
                o.total_uploaded = st.total_upload;
                o.num_peers = st.num_peers;
                o.num_seeds = st.num_seeds;
                o.state = static_cast<int32_t>(st.state);
                o.has_metadata = st.has_metadata ? 1 : 0;
                // fill id and name
#if LT_VERSION_NUM >= 10200
                if (st.info_hashes.has_v2()) hex_encode(reinterpret_cast<const unsigned char*>(st.info_hashes.v2.data()), st.info_hashes.v2.size(), o.info_hash, sizeof(o.info_hash));
                else if (st.info_hashes.has_v1()) hex_encode(reinterpret_cast<const unsigned char*>(st.info_hashes.v1.data()), st.info_hashes.v1.size(), o.info_hash, sizeof(o.info_hash));
                else if (sizeof(o.info_hash) > 0) o.info_hash[0] = '\0';
#else
                hex_encode(reinterpret_cast<const unsigned char*>(st.info_hash.data()), st.info_hash.size(), o.info_hash, sizeof(o.info_hash));
#endif
                copy_cstr_safe(o.name, sizeof(o.name), st.name);
            }
        }
    }
    return written;
}

swbt_error_code_e swbt_torrent_infohash(swbt_torrent_handle_t* handle, char* out_hex, int out_len) {
    if (!handle || !out_hex || out_len <= 0) return SWBT_ERR_INVALID_ARG;
    fill_infohash_hex(static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle, out_hex, out_len);
    return SWBT_OK;
}

void swbt_torrent_save_resume(swbt_torrent_handle_t* handle) {
    if (!handle) return;
#if LT_VERSION_NUM >= 10200
    static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.save_resume_data(lt::torrent_handle::save_info_dict);
#else
    static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.save_resume_data(lt::torrent_handle::save_info_dict);
#endif
}

int swbt_session_poll_resume(swbt_session_t* session,
                             int timeout_ms,
                             swbt_resume_data_t* out_items,
                             int max_count) {
    if (!session || !out_items || max_count <= 0) return 0;
    lt::session* s = static_cast<SwbtSessionImpl*>(session->impl)->session.get();
    s->wait_for_alert(lt::milliseconds(timeout_ms));
    std::vector<lt::alert*> alerts;
    s->pop_alerts(&alerts);
    int written = 0;
    for (lt::alert* a : alerts) {
        if (auto* ok = lt::alert_cast<lt::save_resume_data_alert>(a)) {
            if (written >= max_count) return written;
            auto& item = out_items[written++];
            fill_infohash_hex(ok->handle, item.info_hash, sizeof(item.info_hash));
            // encode resume data to buffer
            std::vector<char> buf = lt::write_resume_data_buf(ok->params);
            uint8_t* raw = static_cast<uint8_t*>(std::malloc(buf.size()));
            if (raw && !buf.empty()) std::memcpy(raw, buf.data(), buf.size());
            item.data = raw;
            item.size = static_cast<int32_t>(buf.size());
        }
    }
    return written;
}

void swbt_resume_data_free(swbt_resume_data_t* items, int count) {
    if (!items || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        if (items[i].data) std::free(const_cast<uint8_t*>(items[i].data));
        items[i].data = nullptr;
        items[i].size = 0;
    }
}

static lt::add_torrent_params build_add_params_with_resume(const lt::add_torrent_params& base,
                                                           const uint8_t* resume_data,
                                                           int resume_size) {
    lt::add_torrent_params p = base;
    if (resume_data && resume_size > 0) {
        p.resume_data.assign(reinterpret_cast<const char*>(resume_data), reinterpret_cast<const char*>(resume_data) + resume_size);
    }
    return p;
}

swbt_error_code_e swbt_add_magnet_with_resume(swbt_session_t* session,
                                              const char* magnet_uri,
                                              const char* save_path,
                                              const uint8_t* resume_data,
                                              int resume_size,
                                              swbt_torrent_handle_t** out_handle) {
    if (!session || !magnet_uri || !out_handle) return SWBT_ERR_INVALID_ARG;
    lt::error_code ec;
    lt::add_torrent_params base = lt::parse_magnet_uri(magnet_uri, ec);
    if (ec) return SWBT_ERR_GENERIC;
    if (save_path && save_path[0] != '\0') base.save_path = save_path;
    else if (!static_cast<SwbtSessionImpl*>(session->impl)->default_save_path.empty()) base.save_path = static_cast<SwbtSessionImpl*>(session->impl)->default_save_path;
    else base.save_path = ".";
    lt::add_torrent_params p = build_add_params_with_resume(base, resume_data, resume_size);
    lt::torrent_handle th = static_cast<SwbtSessionImpl*>(session->impl)->session->add_torrent(std::move(p), ec);
    if (ec) return SWBT_ERR_GENERIC;
    auto h = new swbt_torrent_handle_t{};
    h->impl = new SwbtTorrentHandleImpl{th};
    *out_handle = h;
    return SWBT_OK;
}

swbt_error_code_e swbt_add_torrent_file_with_resume(swbt_session_t* session,
                                                    const char* torrent_file_path,
                                                    const char* save_path,
                                                    const uint8_t* resume_data,
                                                    int resume_size,
                                                    swbt_torrent_handle_t** out_handle) {
    if (!session || !torrent_file_path || !out_handle) return SWBT_ERR_INVALID_ARG;
    lt::error_code ec;
    auto ti = std::make_shared<lt::torrent_info>(torrent_file_path, ec);
    if (ec) return SWBT_ERR_GENERIC;
    lt::add_torrent_params base;
    base.ti = std::move(ti);
    if (save_path && save_path[0] != '\0') base.save_path = save_path;
    else if (!static_cast<SwbtSessionImpl*>(session->impl)->default_save_path.empty()) base.save_path = static_cast<SwbtSessionImpl*>(session->impl)->default_save_path;
    else base.save_path = ".";
    lt::add_torrent_params p = build_add_params_with_resume(base, resume_data, resume_size);
    lt::torrent_handle th = static_cast<SwbtSessionImpl*>(session->impl)->session->add_torrent(std::move(p), ec);
    if (ec) return SWBT_ERR_GENERIC;
    auto h = new swbt_torrent_handle_t{};
    h->impl = new SwbtTorrentHandleImpl{th};
    *out_handle = h;
    return SWBT_OK;
}

int swbt_session_list_overview(swbt_session_t* session,
                               swbt_torrent_overview_t* out_items,
                               int max_count) {
    if (!session || !out_items || max_count <= 0) return 0;
    lt::session* s = static_cast<SwbtSessionImpl*>(session->impl)->session.get();
    std::vector<lt::torrent_handle> v = s->get_torrents();
    int written = 0;
    for (auto& th : v) {
        if (written >= max_count) break;
        auto& o = out_items[written++];
        fill_infohash_hex(th, o.info_hash, sizeof(o.info_hash));
        std::string name;
        lt::torrent_status st = th.status(lt::torrent_handle::query_name);
        name = st.name;
        copy_cstr_safe(o.name, sizeof(o.name), name);
    }
    return written;
}

swbt_error_code_e swbt_session_find_torrent(swbt_session_t* session,
                                            const char* info_hash_hex,
                                            swbt_torrent_handle_t** out_handle) {
    if (!session || !info_hash_hex || !out_handle) return SWBT_ERR_INVALID_ARG;
    *out_handle = nullptr;
    lt::session* s = static_cast<SwbtSessionImpl*>(session->impl)->session.get();
    std::vector<lt::torrent_handle> v = s->get_torrents();
    for (auto& th : v) {
        char buf[65] = {0};
        fill_infohash_hex(th, buf, sizeof(buf));
        if (std::strcmp(buf, info_hash_hex) == 0) {
            auto h = new swbt_torrent_handle_t{};
            h->impl = new SwbtTorrentHandleImpl{th};
            *out_handle = h;
            return SWBT_OK;
        }
    }
    return SWBT_ERR_GENERIC;
}

int64_t swbt_torrent_total_size(swbt_torrent_handle_t* handle) {
    if (!handle) return 0;
    auto th = static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle;
    std::shared_ptr<const lt::torrent_info> ti = th.torrent_file();
    if (!ti) return 0;
    return ti->total_size();
}

int swbt_torrent_file_count(swbt_torrent_handle_t* handle) {
    if (!handle) return 0;
    auto th = static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle;
    std::shared_ptr<const lt::torrent_info> ti = th.torrent_file();
    if (!ti) return 0;
    return static_cast<int>(ti->num_files());
}

swbt_error_code_e swbt_torrent_file_info(swbt_torrent_handle_t* handle,
                                         int index,
                                         swbt_file_info_t* out_info) {
    if (!handle || !out_info || index < 0) return SWBT_ERR_INVALID_ARG;
    auto th = static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle;
    std::shared_ptr<const lt::torrent_info> ti = th.torrent_file();
    if (!ti) return SWBT_ERR_GENERIC;
    if (index >= ti->num_files()) return SWBT_ERR_INVALID_ARG;
    const lt::file_storage& fs = ti->files();
    lt::file_index_t idx(index);
    out_info->index = index;
    out_info->size = fs.file_size(idx);
    out_info->offset = fs.file_offset(idx);
    copy_cstr_safe(out_info->path, sizeof(out_info->path), fs.file_path(idx));
    // get priority
    int prio = static_cast<int>(th.file_priority(idx));
    out_info->priority = prio;
    return SWBT_OK;
}

void swbt_torrent_set_file_priority(swbt_torrent_handle_t* handle,
                                    int index,
                                    int priority) {
    if (!handle || index < 0) return;
    auto th = static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle;
    th.file_priority(lt::file_index_t(index), lt::download_priority_t(priority));
}

int swbt_session_poll_alerts(swbt_session_t* session,
                              int timeout_ms,
                              swbt_alert_t* out_alerts,
                              int max_count) {
    if (!session || !out_alerts || max_count <= 0) return 0;
    lt::session* s = static_cast<SwbtSessionImpl*>(session->impl)->session.get();
    s->wait_for_alert(lt::milliseconds(timeout_ms));
    std::vector<lt::alert*> alerts;
    s->pop_alerts(&alerts);
    int written = 0;
    for (lt::alert* a : alerts) {
        if (written >= max_count) break;
        swbt_alert_t* out = &out_alerts[written];
        bool matched = false;
        if (auto* fin = lt::alert_cast<lt::torrent_finished_alert>(a)) {
            out->type = SWBT_ALERT_TORRENT_FINISHED;
            fill_infohash_hex(fin->handle, out->info_hash, sizeof(out->info_hash));
            out->error_code = 0;
            copy_cstr_safe(out->message, sizeof(out->message), a->message());
            matched = true;
        } else if (auto* md = lt::alert_cast<lt::metadata_received_alert>(a)) {
            out->type = SWBT_ALERT_METADATA_RECEIVED;
            fill_infohash_hex(md->handle, out->info_hash, sizeof(out->info_hash));
            out->error_code = 0;
            copy_cstr_safe(out->message, sizeof(out->message), a->message());
            matched = true;
        } else if (auto* te = lt::alert_cast<lt::torrent_error_alert>(a)) {
            out->type = SWBT_ALERT_TORRENT_ERROR;
            fill_infohash_hex(te->handle, out->info_hash, sizeof(out->info_hash));
            out->error_code = te->error.value();
            copy_cstr_safe(out->message, sizeof(out->message), te->error.message());
            matched = true;
        } else if (auto* tr = lt::alert_cast<lt::tracker_error_alert>(a)) {
            out->type = SWBT_ALERT_TRACKER_ERROR;
            fill_infohash_hex(tr->handle, out->info_hash, sizeof(out->info_hash));
            out->error_code = tr->error.value();
            copy_cstr_safe(out->message, sizeof(out->message), tr->error.message());
            matched = true;
        }
        if (matched) {
            ++written;
        }
    }
    return written;
}

swbt_error_code_e swbt_torrent_move_storage(swbt_torrent_handle_t* handle,
                                            const char* new_path) {
    if (!handle || !new_path) return SWBT_ERR_INVALID_ARG;
    static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle.move_storage(new_path, lt::move_flags_t::always_replace_files);
    return SWBT_OK;
}

void swbt_session_set_rate_limits(swbt_session_t* session,
                                  int download_rate,
                                  int upload_rate) {
    if (!session) return;
    lt::settings_pack p;
    if (download_rate >= 0) p.set_int(lt::settings_pack::download_rate_limit, download_rate);
    if (upload_rate >= 0) p.set_int(lt::settings_pack::upload_rate_limit, upload_rate);
    static_cast<SwbtSessionImpl*>(session->impl)->session->apply_settings(p);
}

void swbt_torrent_set_rate_limits(swbt_torrent_handle_t* handle,
                                  int download_rate,
                                  int upload_rate) {
    if (!handle) return;
    auto th = static_cast<SwbtTorrentHandleImpl*>(handle->impl)->handle;
    if (download_rate >= 0) th.set_download_limit(download_rate);
    if (upload_rate >= 0) th.set_upload_limit(upload_rate);
}

#else // non-macOS (stubs)

swbt_session_t* swbt_session_new(const swbt_session_config_t* /*config*/) {
    return new swbt_session_t{};
}

void swbt_session_free(swbt_session_t* session) {
    delete session;
}

swbt_error_code_e swbt_add_magnet(swbt_session_t* /*session*/, const char* /*magnet_uri*/, const char* /*save_path*/, swbt_torrent_handle_t** out_handle) {
    if (out_handle) *out_handle = nullptr;
    return SWBT_ERR_GENERIC;
}

swbt_error_code_e swbt_add_torrent_file(swbt_session_t* /*session*/, const char* /*torrent_file_path*/, const char* /*save_path*/, swbt_torrent_handle_t** out_handle) {
    if (out_handle) *out_handle = nullptr;
    return SWBT_ERR_GENERIC;
}

void swbt_remove_torrent(swbt_session_t* /*session*/, swbt_torrent_handle_t* handle, int /*with_data*/) {
    delete handle;
}

void swbt_torrent_pause(swbt_torrent_handle_t* /*handle*/) {}
void swbt_torrent_resume(swbt_torrent_handle_t* /*handle*/) {}
void swbt_torrent_force_reannounce(swbt_torrent_handle_t* /*handle*/) {}

swbt_error_code_e swbt_torrent_status(swbt_torrent_handle_t* /*handle*/, swbt_torrent_status_t* out_status) {
    if (!out_status) return SWBT_ERR_INVALID_ARG;
    *out_status = swbt_torrent_status_t{0};
    return SWBT_OK;
}

void swbt_session_post_torrent_updates(swbt_session_t* /*session*/) {}

int swbt_session_poll_updates(swbt_session_t* /*session*/, int /*timeout_ms*/, swbt_torrent_status_t* /*out_statuses*/, int /*max_count*/) {
    return 0;
}

#endif // platform guard


