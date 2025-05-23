#include <token.rms/token.rms.hpp>

namespace eosio {
    void token::setconfig(const bool ram2v_enabled, const bool eos2v_enabled) {
        require_auth(get_self());

        config_row config = _config.get_or_default();
        config.ram2v_enabled = ram2v_enabled;
        config.eos2v_enabled = eos2v_enabled;
        _config.set(config, get_self());
    }
}  // namespace eosio