#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

#include "utils.hpp"

class [[eosio::contract]] atomicswap : public eosio::contract {
private:
    struct [[eosio::table]] lockid {
        capi_checksum256 lock_id;
        eosio::name sender;
        eosio::name receiver;
        eosio::asset quantity;
        std::string hash_value;
        uint32_t lock_number;
        uint8_t withdrawn = 0;
        uint8_t refunded = 0;

        uint64_t primary_key() const { return uint64_hash(lock_id); }
    };

    eosio::multi_index<"lockid"_n, lockid> locks;

    void inline_transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo) const {
        struct transfer {
            eosio::name from;
            eosio::name to;
            eosio::asset quantity;
            std::string memo;
        };
        eosio::action(
                eosio::permission_level(from, ("active"_n)),
                ("eosio.token"_n),
                ("transfer"_n),
                transfer{from, to, quantity, memo}
        ).send();
    }

    void parse_memo(std::string memo,
                    eosio::name* receiver,
                    capi_checksum256* hash_value,
                    uint32_t* lock_period) {
        // remove space
        memo.erase(std::remove_if(memo.begin(),
                                  memo.end(),
                                  [](unsigned char x) { return std::isspace(x); }),
                   memo.end());

        size_t sep_count = std::count(memo.begin(), memo.end(), '-');
        eosio_assert(sep_count == 2, "invalid memo");

        size_t pos;
        string container;
        pos = sub2sep(memo, &container, '-', 0, true);
        eosio_assert(!container.empty(), "no receiver");
        *receiver = eosio::name(container.c_str());
        pos = sub2sep(memo, &container, '-', ++pos, true);
        eosio_assert(!container.empty(), "no hash");
        *hash_value = hex_to_sha256(container);
        container = memo.substr(++pos);
        eosio_assert(!container.empty(), "no lock");
        *lock_period = stoul(container);
    }

public:
    using contract::contract;

    atomicswap(eosio::name receiver, eosio::name code, eosio::datastream<const char*> ds) :
        contract(receiver, code, ds),
        locks(receiver, receiver.value)
    {
    }

    ACTION transfer(const eosio::name& from, const eosio::name& to, const eosio::asset& quantity, const std::string& memo);
    ACTION lockmoney(eosio::name sender, eosio::name receiver, eosio::asset quantity, const std::string& hash_value, uint32_t lock_period);
    ACTION withdraw(const capi_checksum256& lockid, const std::string& preimage, eosio::name receiver);
    ACTION refund(const capi_checksum256& lockid, eosio::name sender);
    ACTION cleanup();
};

extern "C" {
void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    auto self = receiver;

    if ((code == "eosio.token"_n.value) && (action == "transfer"_n.value)) {
        execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::transfer);
        return;
    }

    if (code != receiver) return;
    atomicswap thiscontract(name(receiver), name(code), datastream<const char*>(nullptr, 0));

    switch (action) {
        case eosio::name("lockmoney").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::lockmoney);
            break;
        case eosio::name("withdraw").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::withdraw);
            break;
        case eosio::name("refund").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::refund);
            break;
        case eosio::name("cleanup").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::cleanup);
            break;
    }
}
}
