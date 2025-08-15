void miner::clearpool(const uint64_t pool_id) {
    require_auth(get_self());
    auto itr = _poolinfo.require_find(pool_id, "miner.rms::clearpool: pool not found");
    _poolinfo.erase(itr);
}

void miner::clearuser(const uint64_t pool_id) {
    userinfo_index _userinfo(get_self(), pool_id);
    auto itr = _userinfo.begin();
    while (itr != _userinfo.end()) {
        itr = _userinfo.erase(itr);
    }
}