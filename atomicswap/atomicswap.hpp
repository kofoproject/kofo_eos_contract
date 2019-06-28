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
        eosio::name contract;
        eosio::asset quantity;
        std::string hash_value;
        uint32_t lock_number;
        uint8_t withdrawn = 0;
        uint8_t refunded = 0;

        uint64_t primary_key() const { return uint64_hash(lock_id); }
        uint64_t get_lock_time() const {return lock_number;}
    };
    eosio::multi_index<"lockid"_n, lockid, indexed_by<"locknumber"_n,const_mem_fun<lockid, uint64_t, &lockid::get_lock_time>>> locks;

    struct [[eosio::table]] symbols {

        eosio::extended_symbol extsymbol;

        uint64_t primary_key() const { return uint64_hash(extsymbol); }
    };
    eosio::multi_index<"symbols"_n, symbols> supportsymbols;

    struct [[eosio::table]] feegroup {
        eosio::name account;
        uint64_t fee_rate;

        uint64_t primary_key() const { return account.value; }
    };
    eosio::multi_index<"feegroup"_n, feegroup> fees;

    struct [[eosio::table]] adminstate {
        uint64_t id = 0;
        eosio::name admin;
        eosio::name fee_account;
        uint64_t fee_rate = 500;
        uint64_t decimal = 10000;

        uint64_t primary_key() const { return id; }
    };
    eosio::multi_index<"adminstate"_n, adminstate> adminstates;

    void inline_transfer(eosio::name from, eosio::name to,eosio::name contract, eosio::asset quantity, std::string memo) const {
        struct transfer {
            eosio::name from;
            eosio::name to;
            eosio::asset quantity;
            std::string memo;
        };
        eosio::action(
                eosio::permission_level(from, ("active"_n)),
                contract,
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
        locks(receiver, receiver.value),
        fees(receiver, receiver.value),
        adminstates(receiver, receiver.value),
        supportsymbols(receiver,receiver.value)
    {
    }

    ACTION whentransfer(const eosio::name& from, const eosio::name& to, const eosio::asset& quantity, const std::string& memo);
    ACTION withdraw(const capi_checksum256& lockid, const std::string& preimage,const eosio::name& opaccount);
    ACTION refund(const capi_checksum256& lockid);
    ACTION lockmoney(eosio::name sender, eosio::name receiver, eosio::asset quantity, const std::string& hash_value, uint32_t lock_period);
    ACTION setfeegroup(const eosio::name& account, uint64_t fee_rate);
    ACTION setfeeac(const eosio::name& account);
    ACTION setadmin(const eosio::name& account);
    ACTION setdefee(uint64_t fee_rate);
    ACTION setsymbols(const std::string& symbolname,uint8_t precision,const eosio::name& contract);
    ACTION unsetsymbol(const std::string& symbolname, eosio::name& contract);
    ACTION cleanup();
};

extern "C" {
void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    auto self = receiver;

    if (action == "transfer"_n.value) {
        execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::whentransfer);
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
        case eosio::name("setfeegroup").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::setfeegroup);
            break;
        case eosio::name("setfeeac").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::setfeeac);
            break;
        case eosio::name("setadmin").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::setadmin);
            break;
        case eosio::name("setdefee").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::setdefee);
            break;
        case eosio::name("setsymbols").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::setsymbols);
            break;
        case eosio::name("unsetsymbol").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::unsetsymbol);
            break;
        case eosio::name("cleanup").value:
            execute_action(eosio::name(receiver), eosio::name(code), &atomicswap::cleanup);
            break;


    }
}
}
