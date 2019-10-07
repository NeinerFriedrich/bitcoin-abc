// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/wallet.h>

#include <amount.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <interfaces/chain.h>
#include <interfaces/handler.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <support/allocators/secure.h>
#include <sync.h>
#include <ui_interface.h>
#include <util/system.h>
#include <wallet/fees.h>
#include <wallet/ismine.h>
#include <wallet/load.h>
#include <wallet/psbtwallet.h>
#include <wallet/rpcdump.h>
#include <wallet/wallet.h>

namespace interfaces {
namespace {

    //! Construct wallet tx struct.
    WalletTx MakeWalletTx(CWallet &wallet, const CWalletTx &wtx) {
        WalletTx result;
        result.tx = wtx.tx;
        result.txin_is_mine.reserve(wtx.tx->vin.size());
        for (const auto &txin : wtx.tx->vin) {
            result.txin_is_mine.emplace_back(wallet.IsMine(txin));
        }
        result.txout_is_mine.reserve(wtx.tx->vout.size());
        result.txout_address.reserve(wtx.tx->vout.size());
        result.txout_address_is_mine.reserve(wtx.tx->vout.size());
        for (const auto &txout : wtx.tx->vout) {
            result.txout_is_mine.emplace_back(wallet.IsMine(txout));
            result.txout_address.emplace_back();
            result.txout_address_is_mine.emplace_back(
                ExtractDestination(txout.scriptPubKey,
                                   result.txout_address.back())
                    ? wallet.IsMine(result.txout_address.back())
                    : ISMINE_NO);
        }
        result.credit = wtx.GetCredit(ISMINE_ALL);
        result.debit = wtx.GetDebit(ISMINE_ALL);
        result.change = wtx.GetChange();
        result.time = wtx.GetTxTime();
        result.value_map = wtx.mapValue;
        result.is_coinbase = wtx.IsCoinBase();
        return result;
    }

    //! Construct wallet tx status struct.
    WalletTxStatus MakeWalletTxStatus(interfaces::Chain::Lock &locked_chain,
                                      const CWalletTx &wtx) {
        WalletTxStatus result;
        result.block_height =
            locked_chain.getBlockHeight(wtx.m_confirm.hashBlock)
                .get_value_or(std::numeric_limits<int>::max());
        result.blocks_to_maturity = wtx.GetBlocksToMaturity();
        result.depth_in_main_chain = wtx.GetDepthInMainChain();
        result.time_received = wtx.nTimeReceived;
        result.lock_time = wtx.tx->nLockTime;
        TxValidationState state;
        result.is_final =
            locked_chain.contextualCheckTransactionForCurrentBlock(
                Params().GetConsensus(), *wtx.tx, state);
        result.is_trusted = wtx.IsTrusted(locked_chain);
        result.is_abandoned = wtx.isAbandoned();
        result.is_coinbase = wtx.IsCoinBase();
        result.is_in_main_chain = wtx.IsInMainChain();
        return result;
    }

    //! Construct wallet TxOut struct.
    WalletTxOut MakeWalletTxOut(CWallet &wallet, const CWalletTx &wtx, int n,
                                int depth)
        EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet) {
        WalletTxOut result;
        result.txout = wtx.tx->vout[n];
        result.time = wtx.GetTxTime();
        result.depth_in_main_chain = depth;
        result.is_spent = wallet.IsSpent(COutPoint(wtx.GetId(), n));
        return result;
    }

    class WalletImpl : public Wallet {
    public:
        explicit WalletImpl(const std::shared_ptr<CWallet> &wallet)
            : m_wallet(wallet) {}

        bool encryptWallet(const SecureString &wallet_passphrase) override {
            return m_wallet->EncryptWallet(wallet_passphrase);
        }
        bool isCrypted() override { return m_wallet->IsCrypted(); }
        bool lock() override { return m_wallet->Lock(); }
        bool unlock(const SecureString &wallet_passphrase) override {
            return m_wallet->Unlock(wallet_passphrase);
        }
        bool isLocked() override { return m_wallet->IsLocked(); }
        bool changeWalletPassphrase(
            const SecureString &old_wallet_passphrase,
            const SecureString &new_wallet_passphrase) override {
            return m_wallet->ChangeWalletPassphrase(old_wallet_passphrase,
                                                    new_wallet_passphrase);
        }
        void abortRescan() override { m_wallet->AbortRescan(); }
        bool backupWallet(const std::string &filename) override {
            return m_wallet->BackupWallet(filename);
        }
        std::string getWalletName() override { return m_wallet->GetName(); }
        std::set<CTxDestination>
        getLabelAddresses(const std::string &label) override {
            return m_wallet->GetLabelAddresses(label);
        };
        bool getNewDestination(const OutputType type, const std::string label,
                               CTxDestination &dest) override {
            LOCK(m_wallet->cs_wallet);
            std::string error;
            return m_wallet->GetNewDestination(type, label, dest, error);
        }
        const CChainParams &getChainParams() override {
            return m_wallet->chainParams;
        }
        bool getPubKey(const CScript &script, const CKeyID &address,
                       CPubKey &pub_key) override {
            const SigningProvider *provider =
                m_wallet->GetSigningProvider(script);
            if (provider) {
                return provider->GetPubKey(address, pub_key);
            }
            return false;
        }
        bool getPrivKey(const CScript &script, const CKeyID &address,
                        CKey &key) override {
            const SigningProvider *provider =
                m_wallet->GetSigningProvider(script);
            if (provider) {
                return provider->GetKey(address, key);
            }
            return false;
        }
        bool isSpendable(const CTxDestination &dest) override {
            return m_wallet->IsMine(dest) & ISMINE_SPENDABLE;
        }
        bool haveWatchOnly() override {
            auto spk_man = m_wallet->GetLegacyScriptPubKeyMan();
            if (spk_man) {
                return spk_man->HaveWatchOnly();
            }
            return false;
        };
        bool setAddressBook(const CTxDestination &dest, const std::string &name,
                            const std::string &purpose) override {
            return m_wallet->SetAddressBook(dest, name, purpose);
        }
        bool delAddressBook(const CTxDestination &dest) override {
            return m_wallet->DelAddressBook(dest);
        }
        bool getAddress(const CTxDestination &dest, std::string *name,
                        isminetype *is_mine, std::string *purpose) override {
            LOCK(m_wallet->cs_wallet);
            auto it = m_wallet->m_address_book.find(dest);
            if (it == m_wallet->m_address_book.end() || it->second.IsChange()) {
                return false;
            }
            if (name) {
                *name = it->second.GetLabel();
            }
            if (is_mine) {
                *is_mine = m_wallet->IsMine(dest);
            }
            if (purpose) {
                *purpose = it->second.purpose;
            }
            return true;
        }
        std::vector<WalletAddress> getAddresses() override {
            LOCK(m_wallet->cs_wallet);
            std::vector<WalletAddress> result;
            for (const auto &item : m_wallet->m_address_book) {
                if (item.second.IsChange()) {
                    continue;
                }
                result.emplace_back(item.first, m_wallet->IsMine(item.first),
                                    item.second.GetLabel(),
                                    item.second.purpose);
            }
            return result;
        }
        bool addDestData(const CTxDestination &dest, const std::string &key,
                         const std::string &value) override {
            LOCK(m_wallet->cs_wallet);
            WalletBatch batch{m_wallet->GetDatabase()};
            return m_wallet->AddDestData(batch, dest, key, value);
        }
        bool eraseDestData(const CTxDestination &dest,
                           const std::string &key) override {
            LOCK(m_wallet->cs_wallet);
            WalletBatch batch{m_wallet->GetDatabase()};
            return m_wallet->EraseDestData(batch, dest, key);
        }
        std::vector<std::string>
        getDestValues(const std::string &prefix) override {
            LOCK(m_wallet->cs_wallet);
            return m_wallet->GetDestValues(prefix);
        }
        void lockCoin(const COutPoint &output) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->LockCoin(output);
        }
        void unlockCoin(const COutPoint &output) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->UnlockCoin(output);
        }
        bool isLockedCoin(const COutPoint &output) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->IsLockedCoin(output);
        }
        void listLockedCoins(std::vector<COutPoint> &outputs) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->ListLockedCoins(outputs);
        }
        CTransactionRef
        createTransaction(const std::vector<CRecipient> &recipients,
                          const CCoinControl &coin_control, bool sign,
                          int &change_pos, Amount &fee,
                          bilingual_str &fail_reason) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            CTransactionRef tx;
            if (!m_wallet->CreateTransaction(*locked_chain, recipients, tx, fee,
                                             change_pos, fail_reason,
                                             coin_control, sign)) {
                return {};
            }
            return tx;
        }
        void commitTransaction(CTransactionRef tx, WalletValueMap value_map,
                               WalletOrderForm order_form) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            m_wallet->CommitTransaction(std::move(tx), std::move(value_map),
                                        std::move(order_form));
        }
        bool transactionCanBeAbandoned(const TxId &txid) override {
            return m_wallet->TransactionCanBeAbandoned(txid);
        }
        bool abandonTransaction(const TxId &txid) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->AbandonTransaction(txid);
        }
        CTransactionRef getTx(const TxId &txid) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            auto mi = m_wallet->mapWallet.find(txid);
            if (mi != m_wallet->mapWallet.end()) {
                return mi->second.tx;
            }
            return {};
        }
        WalletTx getWalletTx(const TxId &txid) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            auto mi = m_wallet->mapWallet.find(txid);
            if (mi != m_wallet->mapWallet.end()) {
                return MakeWalletTx(*m_wallet, mi->second);
            }
            return {};
        }
        std::vector<WalletTx> getWalletTxs() override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            std::vector<WalletTx> result;
            result.reserve(m_wallet->mapWallet.size());
            for (const auto &entry : m_wallet->mapWallet) {
                result.emplace_back(MakeWalletTx(*m_wallet, entry.second));
            }
            return result;
        }
        bool tryGetTxStatus(const TxId &txid,
                            interfaces::WalletTxStatus &tx_status,
                            int &num_blocks, int64_t &block_time) override {
            auto locked_chain = m_wallet->chain().lock(true /* try_lock */);
            if (!locked_chain) {
                return false;
            }
            TRY_LOCK(m_wallet->cs_wallet, locked_wallet);
            if (!locked_wallet) {
                return false;
            }
            auto mi = m_wallet->mapWallet.find(txid);
            if (mi == m_wallet->mapWallet.end()) {
                return false;
            }
            if (Optional<int> height = locked_chain->getHeight()) {
                num_blocks = *height;
                block_time = locked_chain->getBlockTime(*height);
            } else {
                num_blocks = -1;
                block_time = -1;
            }
            tx_status = MakeWalletTxStatus(*locked_chain, mi->second);
            return true;
        }
        WalletTx getWalletTxDetails(const TxId &txid, WalletTxStatus &tx_status,
                                    WalletOrderForm &order_form,
                                    bool &in_mempool,
                                    int &num_blocks) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            auto mi = m_wallet->mapWallet.find(txid);
            if (mi != m_wallet->mapWallet.end()) {
                num_blocks = locked_chain->getHeight().value_or(-1);
                in_mempool = mi->second.InMempool();
                order_form = mi->second.vOrderForm;
                tx_status = MakeWalletTxStatus(*locked_chain, mi->second);
                return MakeWalletTx(*m_wallet, mi->second);
            }
            return {};
        }
        TransactionError
        fillPSBT(PartiallySignedTransaction &psbtx, bool &complete,
                 SigHashType sighash_type =
                     SigHashType().withForkId() /* SIGHASH_ALL|FORKID */,
                 bool sign = true, bool bip32derivs = false) override {
            return FillPSBT(m_wallet.get(), psbtx, complete, sighash_type, sign,
                            bip32derivs);
        }
        WalletBalances getBalances() override {
            const auto bal = m_wallet->GetBalance();
            WalletBalances result;
            result.balance = bal.m_mine_trusted;
            result.unconfirmed_balance = bal.m_mine_untrusted_pending;
            result.immature_balance = bal.m_mine_immature;
            result.have_watch_only = haveWatchOnly();
            if (result.have_watch_only) {
                result.watch_only_balance = bal.m_watchonly_trusted;
                result.unconfirmed_watch_only_balance =
                    bal.m_watchonly_untrusted_pending;
                result.immature_watch_only_balance = bal.m_watchonly_immature;
            }
            return result;
        }
        bool tryGetBalances(WalletBalances &balances,
                            int &num_blocks) override {
            auto locked_chain = m_wallet->chain().lock(true /* try_lock */);
            if (!locked_chain) {
                return false;
            }
            TRY_LOCK(m_wallet->cs_wallet, locked_wallet);
            if (!locked_wallet) {
                return false;
            }
            balances = getBalances();
            num_blocks = locked_chain->getHeight().value_or(-1);
            return true;
        }
        Amount getBalance() override {
            return m_wallet->GetBalance().m_mine_trusted;
        }
        Amount getAvailableBalance(const CCoinControl &coin_control) override {
            return m_wallet->GetAvailableBalance(&coin_control);
        }
        isminetype txinIsMine(const CTxIn &txin) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->IsMine(txin);
        }
        isminetype txoutIsMine(const CTxOut &txout) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->IsMine(txout);
        }
        Amount getDebit(const CTxIn &txin, isminefilter filter) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->GetDebit(txin, filter);
        }
        Amount getCredit(const CTxOut &txout, isminefilter filter) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            return m_wallet->GetCredit(txout, filter);
        }
        CoinsList listCoins() override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            CoinsList result;
            for (const auto &entry : m_wallet->ListCoins(*locked_chain)) {
                auto &group = result[entry.first];
                for (const auto &coin : entry.second) {
                    group.emplace_back(COutPoint(coin.tx->GetId(), coin.i),
                                       MakeWalletTxOut(*m_wallet, *coin.tx,
                                                       coin.i, coin.nDepth));
                }
            }
            return result;
        }
        std::vector<WalletTxOut>
        getCoins(const std::vector<COutPoint> &outputs) override {
            auto locked_chain = m_wallet->chain().lock();
            LOCK(m_wallet->cs_wallet);
            std::vector<WalletTxOut> result;
            result.reserve(outputs.size());
            for (const auto &output : outputs) {
                result.emplace_back();
                auto it = m_wallet->mapWallet.find(output.GetTxId());
                if (it != m_wallet->mapWallet.end()) {
                    int depth = it->second.GetDepthInMainChain();
                    if (depth >= 0) {
                        result.back() = MakeWalletTxOut(*m_wallet, it->second,
                                                        output.GetN(), depth);
                    }
                }
            }
            return result;
        }
        bool hdEnabled() override { return m_wallet->IsHDEnabled(); }
        OutputType getDefaultAddressType() override {
            return m_wallet->m_default_address_type;
        }
        bool canGetAddresses() override { return m_wallet->CanGetAddresses(); }
        bool IsWalletFlagSet(uint64_t flag) override {
            return m_wallet->IsWalletFlagSet(flag);
        }
        OutputType getDefaultChangeType() override {
            return m_wallet->m_default_change_type;
        }
        Amount getDefaultMaxTxFee() override {
            return m_wallet->m_default_max_tx_fee;
        }
        void remove() override { RemoveWallet(m_wallet); }
        std::unique_ptr<Handler> handleUnload(UnloadFn fn) override {
            return MakeHandler(m_wallet->NotifyUnload.connect(fn));
        }
        std::unique_ptr<Handler>
        handleShowProgress(ShowProgressFn fn) override {
            return MakeHandler(m_wallet->ShowProgress.connect(fn));
        }
        std::unique_ptr<Handler>
        handleStatusChanged(StatusChangedFn fn) override {
            return MakeHandler(m_wallet->NotifyStatusChanged.connect(
                [fn](CWallet *) { fn(); }));
        }
        std::unique_ptr<Handler>
        handleAddressBookChanged(AddressBookChangedFn fn) override {
            return MakeHandler(m_wallet->NotifyAddressBookChanged.connect(
                [fn](CWallet *, const CTxDestination &address,
                     const std::string &label, bool is_mine,
                     const std::string &purpose, ChangeType status) {
                    fn(address, label, is_mine, purpose, status);
                }));
        }
        std::unique_ptr<Handler>
        handleTransactionChanged(TransactionChangedFn fn) override {
            return MakeHandler(m_wallet->NotifyTransactionChanged.connect(
                [fn](CWallet *, const TxId &txid, ChangeType status) {
                    fn(txid, status);
                }));
        }
        std::unique_ptr<Handler>
        handleWatchOnlyChanged(WatchOnlyChangedFn fn) override {
            return MakeHandler(m_wallet->NotifyWatchonlyChanged.connect(fn));
        }
        std::unique_ptr<Handler>
        handleCanGetAddressesChanged(CanGetAddressesChangedFn fn) override {
            return MakeHandler(
                m_wallet->NotifyCanGetAddressesChanged.connect(fn));
        }
        Amount getRequiredFee(unsigned int tx_bytes) override {
            return GetRequiredFee(*m_wallet, tx_bytes);
        }
        Amount getMinimumFee(unsigned int tx_bytes,
                             const CCoinControl &coin_control) override {
            return GetMinimumFee(*m_wallet, tx_bytes, coin_control);
        }

        std::shared_ptr<CWallet> m_wallet;
    };

    class WalletClientImpl : public ChainClient {
    public:
        WalletClientImpl(Chain &chain,
                         std::vector<std::string> wallet_filenames)
            : m_chain(chain), m_wallet_filenames(std::move(wallet_filenames)) {}

        void registerRpcs() override {
            g_rpc_chain = &m_chain;
            RegisterWalletRPCCommands(m_chain, m_rpc_handlers);
            RegisterDumpRPCCommands(m_chain, m_rpc_handlers);
        }
        bool verify(const CChainParams &chainParams) override {
            return VerifyWallets(chainParams, m_chain, m_wallet_filenames);
        }
        bool load(const CChainParams &chainParams) override {
            return LoadWallets(chainParams, m_chain, m_wallet_filenames);
        }
        void start(CScheduler &scheduler) override {
            return StartWallets(scheduler);
        }
        void flush() override { return FlushWallets(); }
        void stop() override { return StopWallets(); }
        ~WalletClientImpl() override { UnloadWallets(); }

        Chain &m_chain;
        std::vector<std::string> m_wallet_filenames;
        std::vector<std::unique_ptr<Handler>> m_rpc_handlers;
    };

} // namespace

std::unique_ptr<Wallet> MakeWallet(const std::shared_ptr<CWallet> &wallet) {
    return wallet ? std::make_unique<WalletImpl>(wallet) : nullptr;
}

std::unique_ptr<ChainClient>
MakeWalletClient(Chain &chain, std::vector<std::string> wallet_filenames) {
    return std::make_unique<WalletClientImpl>(chain,
                                              std::move(wallet_filenames));
}

} // namespace interfaces
