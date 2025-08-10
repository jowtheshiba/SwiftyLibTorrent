#include "SwiftyBitTorrentCore.h"

#include <string>
#include <memory>
#include <cstdio>

// libtorrent
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

namespace lt = libtorrent;

struct SwbtSessionImpl {
    std::unique_ptr<lt::session> session;
    std::string default_save_path;
};

struct SwbtTorrentHandleImpl {
    lt::torrent_handle handle;
};

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
    return SWBT_OK;
}


