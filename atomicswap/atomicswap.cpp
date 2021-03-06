#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>

#include "atomicswap.hpp"

ACTION atomicswap::whentransfer(const eosio::name& from,
                            const eosio::name& to, 
                            const eosio::asset& quantity,
                            const std::string& memo) {
    if (from == this->_self || to != this->_self) {
        return;
    }
    print("transfer memo, ", memo);
    eosio::name receiver;
    capi_checksum256 hash;
    uint32_t lock_period;

    parse_memo(memo, &receiver, &hash, &lock_period);

    eosio::name contract = _code;
    auto extsymbol = eosio::extended_symbol(quantity.symbol,contract);
    uint64_t symkey = uint64_hash(extsymbol);
    auto existsymbol = this->supportsymbols.find(symkey);
    eosio_assert(existsymbol != this->supportsymbols.end(), "not supported symbol-contract!");

    eosio_assert(quantity.symbol == (existsymbol->extsymbol).get_symbol(), "wrong precision!");
    eosio_assert(quantity.is_valid(), "invalid token transfer");
    eosio_assert(quantity.amount > 0, "amount should be bigger than 0");

    const uint32_t _now = now();
    eosio_assert(lock_period > _now, "lock_period must be greater than current time");

    double balance = (double) quantity.amount / 10000;
    std::string balance_str = std::to_string(balance);
    auto dot_pos = balance_str.find('.', 0);
    std::string final_balance_str = balance_str.substr(0, dot_pos + 5);
    eosio_assert(!final_balance_str.empty(), "no eos asset amount");

    std::string hash_value = sha256_to_hex(hash);
    std::string input = from.to_string() +
                        receiver.to_string() +
                        final_balance_str+
                        " " + quantity.symbol.code().to_string() +
                        hash_value +
                        std::to_string(lock_period);
    const char* data_cstr = input.c_str();

    capi_checksum256 lid;
    sha256(data_cstr, strlen(data_cstr), &lid);

    const uint64_t key = uint64_hash(lid);
    auto it = this->locks.find(key);
    eosio_assert(it == this->locks.end(), "there are same lockId!");
    this->locks.emplace(this->_self, [lid, from, receiver, quantity, contract, hash_value, lock_period](auto& lockid) {
            lockid.lock_id = lid;
            lockid.sender = from;
            lockid.receiver = receiver;
            lockid.quantity = quantity;
            lockid.hash_value = hash_value;
            lockid.lock_number = lock_period;
            lockid.contract = contract;
            lockid.withdrawn = 0;
            lockid.refunded = 0;
            
    });

    require_recipient(this->_self);

    struct lockmoney {
        eosio::name from;
        eosio::name to; 
        eosio::asset quantity;
        std::string hash_value;
        uint32_t lock_period;
    };  

    eosio::action(
            eosio::permission_level(this->_self, ("active"_n)),
            this->_self,
            ("lockmoney"_n),
            lockmoney{from, receiver, quantity, hash_value, lock_period}
    ).send();
}

ACTION atomicswap::lockmoney(eosio::name sender, eosio::name receiver, eosio::asset quantity, const std::string& hash_value, uint32_t lock_period) {
    require_auth(this->_self);
}

ACTION atomicswap::withdraw(const capi_checksum256& lockid, const std::string& preimage , const eosio::name& opaccount) {
    require_auth(opaccount);
    const uint64_t key = uint64_hash(lockid);
    auto it = this->locks.find(key);
    eosio_assert(it != this->locks.end(), "no lock for this lockId");

    capi_checksum256 hash_value;
    const char* data1_cstr = preimage.c_str();
    sha256(data1_cstr, strlen(data1_cstr), &hash_value);
    sha256((const char *)hash_value.hash, sizeof(hash_value.hash), &hash_value);
    string hash_second = sha256_to_hex(hash_value);

    eosio_assert(it->hash_value.compare(hash_second) == 0, "preimage error");
    eosio_assert(it->withdrawn == 0, "lock is already withdraw");
    eosio_assert(it->lock_number > now(), "time is already greater than lock time");

    this->locks.modify(it, this->_self, [](auto& lockid) {
            lockid.withdrawn = 1;
    });

    uint64_t fee_rate;
    auto fee_iter = this->fees.find(opaccount.value);
    if (fee_iter != this->fees.end()) {
        fee_rate = fee_iter->fee_rate;
    } else {
        fee_rate = this->adminstates.cbegin()->fee_rate;
    }

    auto fee = it->quantity * fee_rate / this->adminstates.cbegin()->decimal;

    if (fee.amount > 0)
    {
        // transfer fee to feeAccount
        this->inline_transfer(this->_self, this->adminstates.cbegin()->fee_account, it->contract, fee, "withdraw fee");
    }
    
    // transfer with sub fee
    auto amount = it->quantity - fee;
    this->inline_transfer(this->_self, it->receiver,it->contract, amount, "withdraw");
}

ACTION atomicswap::refund(const capi_checksum256& lockid) {

    const uint64_t key = uint64_hash(lockid);
    auto it = this->locks.find(key);
    eosio_assert(it != this->locks.end(), "no lock for this lockId");

    eosio_assert(it->withdrawn == 0, "lock is already withdraw");
    eosio_assert(it->refunded == 0, "lock is already refund");
    eosio_assert(it->lock_number < now(), "time is less than lock time");

    this->locks.modify(it, this->_self, [](auto& lockid) {
            lockid.refunded = 1;
    });

    this->inline_transfer(this->_self, it->sender,it->contract, it->quantity, "refund");
}

// add del account fee methods
// change auth to admin instead of contract self
ACTION atomicswap::setfeegroup(const eosio::name& account, uint64_t fee_rate) {
    require_auth(this->_self);

    eosio_assert(fee_rate >= 0 && fee_rate < this->adminstates.cbegin()->decimal, "invalid fee_rate");
    auto it = this->fees.find(account.value);
    if (it != this->fees.end()) {
        this->fees.modify(it, this->_self, [fee_rate](auto& feegroup) {
                feegroup.fee_rate = fee_rate;
                });
    } else {
        this->fees.emplace(this->_self, [account, fee_rate](auto& feegroup) {
                feegroup.account = account;
                feegroup.fee_rate = fee_rate;
                });
    }
}

ACTION atomicswap::setfeeac(const eosio::name& account) {
    require_auth(this->_self);

    if (this->adminstates.cbegin() == this->adminstates.cend()) {
        this->adminstates.emplace(this->_self, [&](auto &target) {
                target.id = 0;
                target.fee_account = account;
                });
    } else {
        this->adminstates.modify(this->adminstates.cbegin(), this->_self, [&](auto &target) {
                target.fee_account = account;
                });
    }
}

ACTION atomicswap::setadmin(const eosio::name& account) {
    require_auth(this->_self);

    if (this->adminstates.cbegin() == this->adminstates.cend()) {
        this->adminstates.emplace(this->_self, [&](auto &target) {
                target.id = 0;
                target.admin = account;
                });
    } else {
        this->adminstates.modify(this->adminstates.cbegin(), this->_self, [&](auto &target) {
                target.admin = account;
                });
    }
}

ACTION atomicswap::setdefee(uint64_t fee_rate) {
    require_auth(this->_self);

    eosio_assert(fee_rate >= 0 && fee_rate < this->adminstates.cbegin()->decimal, "invalid fee_rate");
    if (this->adminstates.cbegin() == this->adminstates.cend()) {
        this->adminstates.emplace(this->_self, [&](auto &target) {
                target.id = 0;
                target.fee_rate = fee_rate;
                });
    } else {
        this->adminstates.modify(this->adminstates.cbegin(), this->_self, [&](auto &target) {
                target.fee_rate = fee_rate;
                });
    }
}

ACTION atomicswap::setsymbols(const std::string& symbolname, uint8_t precision, const eosio::name& contract) {
    require_auth(this->_self);
    eosio_assert(precision >= 1 && precision <= 18, "invalid precision");
    auto extendedsymbol = eosio::extended_symbol(eosio::symbol(symbolname, precision),contract);
    auto it = this->supportsymbols.find(uint64_hash(extendedsymbol));
    if (it != this->supportsymbols.end()) {
        this->supportsymbols.modify(it, this->_self, [extendedsymbol](auto& target) {
                target.extsymbol = extendedsymbol;
                });
    } else {
        this->supportsymbols.emplace(this->_self, [extendedsymbol](auto& target) {
                target.extsymbol = extendedsymbol;
                });
    }
}

ACTION atomicswap::unsetsymbol(const std::string& symbolname, eosio::name& contract) {
    require_auth(this->_self);
    uint8_t precision = 4;
    auto extendedsymbol = eosio::extended_symbol(eosio::symbol(symbolname, precision),contract);
    auto it = this->supportsymbols.find(uint64_hash(extendedsymbol));
    if (it != this->supportsymbols.end()) {
         this->supportsymbols.erase(it);
    } 
}



ACTION atomicswap::cleanup() {
    require_auth(this->_self);
    auto iter = this->locks.begin();
    while (iter != this->locks.cend()) {
        iter = this->locks.erase(iter);
    }
    auto adminiter = this->adminstates.begin();
    while (adminiter != this->adminstates.cend()) {
        adminiter = this->adminstates.erase(adminiter);
    }
    auto supportsymbol = this->supportsymbols.begin();
    while (supportsymbol != this->supportsymbols.cend()) {
        supportsymbol = this->supportsymbols.erase(supportsymbol);
    }
}
