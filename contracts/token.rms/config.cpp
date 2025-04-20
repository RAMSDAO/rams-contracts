#include <token.rms/token.rms.hpp>

namespace eosio {
    void token::setconfig(const bool ram2rams_enabled, const bool eos2rams_enabled) {
        require_auth(get_self());

        config_row config = _config.get_or_default();
        config.ram2rams_enabled = ram2rams_enabled;
        config.eos2rams_enabled = eos2rams_enabled;
        _config.set(config, get_self());
    }
}  // namespace eosio