// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017 The Bitcoin developers
// Copyright (c) 2019 The Freecash First Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <amount.h>
#include <blockfileinfo.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <diskblockpos.h>
#include <fs.h>
#include <protocol.h> // For CMessageHeader::MessageMagic
#include <script/script_error.h>
#include <sync.h>
#include <versionbits.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class arith_uint256;

class CBlockIndex;
class CBlockTreeDB;
class CChainParams;
class CChain;
class CCoinsViewDB;
class CConnman;
class CInv;
class Config;
class CScriptCheck;
class CTxMemPool;
class CTxUndo;
class CValidationState;

struct CDiskBlockPos;
struct ChainTxData;
struct PrecomputedTransactionData;
struct LockPoints;

#define MIN_TRANSACTION_SIZE                                                   \
    (::GetSerializeSize(CTransaction(), SER_NETWORK, PROTOCOL_VERSION))

/** Default for DEFAULT_WHITELISTRELAY. */
static const bool DEFAULT_WHITELISTRELAY = true;
/** Default for DEFAULT_WHITELISTFORCERELAY. */
static const bool DEFAULT_WHITELISTFORCERELAY = true;
/** Default for -minrelaytxfee, minimum relay fee for transactions */
static const Amount DEFAULT_MIN_RELAY_TX_FEE_PER_KB(1000 * SATOSHI);
/** Default for -excessutxocharge for transactions transactions */
static const Amount DEFAULT_UTXO_FEE = Amount::zero();
//! -maxtxfee default
static const Amount DEFAULT_TRANSACTION_MAXFEE(COIN / 10);
//! Discourage users to set fees higher than this amount (in satoshis) per kB
static const Amount HIGH_TX_FEE_PER_KB(COIN / 100);
/**
 * -maxtxfee will warn if called with a higher fee than this amount (in satoshis
 */
static const Amount HIGH_MAX_TX_FEE(100 * HIGH_TX_FEE_PER_KB);
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 25;
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool
 * ancestors */
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 101;
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 25;
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool
 * descendants */
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 101;
/** Default for -mempoolexpiry, expiration time for mempool transactions in
 * hours */
static const unsigned int DEFAULT_MEMPOOL_EXPIRY = 336;
/** The maximum size of a blk?????.dat file (since 0.8) */
static const unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** The pre-allocation chunk size for blk?????.dat files (since 0.8) */
static const unsigned int BLOCKFILE_CHUNK_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for rev?????.dat files (since 0.8) */
static const unsigned int UNDOFILE_CHUNK_SIZE = 0x100000; // 1 MiB

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
/**
 * Number of blocks that can be requested at any given time from a single peer.
 */
static const int MAX_BLOCKS_IN_TRANSIT_PER_PEER = 16;
/**
 * Timeout in seconds during which a peer must stall block download progress
 * before being disconnected.
 */
static const unsigned int BLOCK_STALLING_TIMEOUT = 2;
/**
 * Number of headers sent in one getheaders result. We rely on the assumption
 * that if a peer sends less than this number, we reached its tip. Changing this
 * value is a protocol upgrade.
 */
static const unsigned int MAX_HEADERS_RESULTS = 2000;
/**
 * Maximum depth of blocks we're willing to serve as compact blocks to peers
 * when requested. For older blocks, a regular BLOCK response will be sent.
 */
static const int MAX_CMPCTBLOCK_DEPTH = 5;
/**
 * Maximum depth of blocks we're willing to respond to GETBLOCKTXN requests for.
 */
static const int MAX_BLOCKTXN_DEPTH = 10;
/**
 * Size of the "block download window": how far ahead of our current height do
 * we fetch ? Larger windows tolerate larger download speed differences between
 * peer, but increase the potential degree of disordering of blocks on disk
 * (which make reindexing and in the future perhaps pruning harder). We'll
 * probably want to make this a per-peer adaptive value at some point.
 */
static const unsigned int BLOCK_DOWNLOAD_WINDOW = 1024;
/** Time to wait (in seconds) between writing blocks/block index to disk. */
static const unsigned int DATABASE_WRITE_INTERVAL = 60 * 60;
/** Time to wait (in seconds) between flushing chainstate to disk. */
static const unsigned int DATABASE_FLUSH_INTERVAL = 24 * 60 * 60;
/** Maximum length of reject messages. */
static const unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/** Average delay between local address broadcasts in seconds. */
static const unsigned int AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL = 24 * 60 * 60;
/** Average delay between peer address broadcasts in seconds. */
static const unsigned int AVG_ADDRESS_BROADCAST_INTERVAL = 30;
/**
 * Average delay between trickled inventory transmissions in seconds.
 * Blocks and whitelisted receivers bypass this, outbound peers get half this
 * delay.
 */
static const unsigned int INVENTORY_BROADCAST_INTERVAL = 5;
/**
 * Maximum number of inventory items to send per transmission.
 * Limits the impact of low-fee transaction floods.
 */
static const unsigned int INVENTORY_BROADCAST_MAX_PER_MB =
    7 * INVENTORY_BROADCAST_INTERVAL;
/** Average delay between feefilter broadcasts in seconds. */
static const unsigned int AVG_FEEFILTER_BROADCAST_INTERVAL = 10 * 60;
/** Maximum feefilter broadcast delay after significant change. */
static const unsigned int MAX_FEEFILTER_CHANGE_DELAY = 5 * 60;
/** Block download timeout base, expressed in millionths of the block interval
 * (i.e. 10 min) */
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_BASE = 1000000;
/**
 * Additional block download timeout per parallel downloading peer (i.e. 5 min)
 */
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_PER_PEER = 500000;

static const unsigned int DEFAULT_LIMITFREERELAY = 0;
static const bool DEFAULT_RELAYPRIORITY = true;
static const int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;
/**
 * Maximum age of our tip in seconds for us to be considered current for fee
 * estimation.
 */
static const int64_t MAX_FEE_ESTIMATION_TIP_AGE = 3 * 60 * 60;

/** Default for -permitbaremultisig */
static const bool DEFAULT_PERMIT_BAREMULTISIG = true;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_TXINDEX = false;
static const unsigned int DEFAULT_BANSCORE_THRESHOLD = 100;

/** Default for -persistmempool */
static const bool DEFAULT_PERSIST_MEMPOOL = true;
/** Default for using fee filter */
static const bool DEFAULT_FEEFILTER = true;

/**
 * Maximum number of headers to announce when relaying blocks with headers
 * message.
 */
static const unsigned int MAX_BLOCKS_TO_ANNOUNCE = 8;

/** Maximum number of unconnecting headers announcements before DoS score */
static const int MAX_UNCONNECTING_HEADERS = 10;

static const bool DEFAULT_PEERBLOOMFILTERS = true;

/** Default for -stopatheight */
static const int DEFAULT_STOPATHEIGHT = 0;
/** Default for -maxreorgdepth */
static const int DEFAULT_MAX_REORG_DEPTH = 30;
/**
 * Default for -finalizationdelay
 * This is the minimum time between a block header reception and the block
 * finalization.
 * This value should be >> block propagation and validation time
 */
static const int64_t DEFAULT_MIN_FINALIZATION_DELAY = 12 * 60;

extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern CTxMemPool g_mempool;
extern std::atomic_bool g_is_mempool_loaded;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;
extern const std::string strMessageMagic;
extern Mutex g_best_block_mutex;
extern std::condition_variable g_best_block_cv;
extern uint256 g_best_block;
extern std::atomic_bool fImporting;
extern std::atomic_bool fReindex;
extern int nScriptCheckThreads;
extern bool fIsBareMultisigStd;
extern bool fRequireStandard;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
extern size_t nCoinCacheUsage;
extern const int LAST_HALVING;
extern const Amount LAST_DEV_REWARD;

/**
 * A fee rate smaller than this is considered zero fee (for relaying, mining and
 * transaction creation)
 */
extern CFeeRate minRelayTxFee;
/**
 * Absolute maximum transaction fee (in satoshis) used by wallet and mempool
 * (rejects high fee in sendrawtransaction)
 */
extern Amount maxTxFee;
/**
 * If the tip is older than this (in seconds), the node is considered to be in
 * initial block download.
 */
extern int64_t nMaxTipAge;

/**
 * Block hash whose ancestors we will assume to have valid scripts without
 * checking them.
 */
extern uint256 hashAssumeValid;

/**
 * Minimum work we will assume exists on some valid chain.
 */
extern arith_uint256 nMinimumChainWork;

/**
 * Best header we've seen so far (used for getheaders queries' starting points).
 */
extern CBlockIndex *pindexBestHeader;

/** Minimum disk space required - used in CheckDiskSpace() */
static const uint64_t nMinDiskSpace = 52428800;

/** Pruning-related variables and constants */
/** True if any block files have ever been pruned. */
extern bool fHavePruned;
/** True if we're running in -prune mode. */
extern bool fPruneMode;
/** Number of MiB of block files that we're trying to stay below. */
extern uint64_t nPruneTarget;
/** Block files containing a block-height within MIN_BLOCKS_TO_KEEP of
 * chainActive.Tip() will not be pruned. */
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;
/** Minimum blocks required to signal NODE_NETWORK_LIMITED */
static const unsigned int NODE_NETWORK_LIMITED_MIN_BLOCKS = 288;

static const signed int DEFAULT_CHECKBLOCKS = 6;
static const unsigned int DEFAULT_CHECKLEVEL = 3;

/**
 * Require that user allocate at least 550MB for block & undo files (blk???.dat
 * and rev???.dat)
 * At 1MB per block, 288 blocks = 288MB.
 * Add 15% for Undo data = 331MB
 * Add 20% for Orphan block rate = 397MB
 * We want the low water mark after pruning to be at least 397 MB and since we
 * prune in full block file chunks, we need the high water mark which triggers
 * the prune to be one 128MB block file + added 15% undo data = 147MB greater
 * for a total of 545MB. Setting the target to > than 550MB will make it likely
 * we can respect the target.
 */
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

class BlockValidationOptions {
private:
    bool checkPoW : 1;
    bool checkMerkleRoot : 1;

public:
    // Do full validation by default
    BlockValidationOptions() : checkPoW(true), checkMerkleRoot(true) {}
    BlockValidationOptions(bool checkPoWIn, bool checkMerkleRootIn)
        : checkPoW(checkPoWIn), checkMerkleRoot(checkMerkleRootIn) {}

    bool shouldValidatePoW() const { return checkPoW; }
    bool shouldValidateMerkleRoot() const { return checkMerkleRoot; }
};

/**
 * Process an incoming block. This only returns after the best known valid
 * block is made active. Note that it does not, however, guarantee that the
 * specific block passed to it has been checked for validity!
 *
 * If you want to *possibly* get feedback on whether pblock is valid, you must
 * install a CValidationInterface (see validationinterface.h) - this will have
 * its BlockChecked method called whenever *any* block completes validation.
 *
 * Note that we guarantee that either the proof-of-work is valid on pblock, or
 * (and possibly also) BlockChecked will have been called.
 *
 * Call without cs_main held.
 *
 * @param[in]   config  The global config.
 * @param[in]   pblock  The block we want to process.
 * @param[in]   fForceProcessing Process this block even if unrequested; used
 * for non-network block sources and whitelisted peers.
 * @param[out]  fNewBlock A boolean which is set to indicate if the block was
 *                        first received via this call.
 * @return True if the block is accepted as a valid block.
 */
bool ProcessNewBlock(const Config &config,
                     const std::shared_ptr<const CBlock> pblock,
                     bool fForceProcessing, bool *fNewBlock);

/**
 * Process incoming block headers.
 *
 * Call without cs_main held.
 *
 * @param[in]  config        The config.
 * @param[in]  block         The block headers themselves.
 * @param[out] state         This may be set to an Error state if any error
 *                           occurred processing them.
 * @param[out] ppindex       If set, the pointer will be set to point to the
 *                           last new block index object for the given headers.
 * @param[out] first_invalid First header that fails validation, if one exists.
 * @return True if block headers were accepted as valid.
 */
bool ProcessNewBlockHeaders(const Config &config,
                            const std::vector<CBlockHeader> &block,
                            CValidationState &state,
                            const CBlockIndex **ppindex = nullptr,
                            CBlockHeader *first_invalid = nullptr);

/**
 * Check whether enough disk space is available for an incoming block.
 */
bool CheckDiskSpace(uint64_t nAdditionalBytes = 0, bool blocks_dir = false);

/**
 * Open a block file (blk?????.dat).
 */
FILE *OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly = false);

/**
 * Translation to a filesystem path.
 */
fs::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix);

/**
 * Import blocks from an external file.
 */
bool LoadExternalBlockFile(const Config &config, FILE *fileIn,
                           CDiskBlockPos *dbp = nullptr);

/**
 * Ensures we have a genesis block in the block tree, possibly writing one to
 * disk.
 */
bool LoadGenesisBlock(const CChainParams &chainparams);

/**
 * Load the block tree and coins database from disk, initializing state if we're
 * running with -reindex.
 */
bool LoadBlockIndex(const Config &config);

/**
 * Update the chain tip based on database information.
 */
bool LoadChainTip(const Config &config);

/**
 * Unload database information.
 */
void UnloadBlockIndex();

/**
 * Run an instance of the script checking thread.
 */
void ThreadScriptCheck();

/**
 * Check whether we are doing an initial block download (synchronizing from disk
 * or network)
 */
bool IsInitialBlockDownload();

/**
 * Retrieve a transaction (from memory pool, or from disk, if possible).
 */
bool GetTransaction(const Config &config, const TxId &txid, CTransactionRef &tx,
                    uint256 &hashBlock, bool fAllowSlow = false,
                    CBlockIndex *blockIndex = nullptr);

/**
 * Find the best known block, and make it the active tip of the block chain.
 * If it fails, the tip is not updated.
 *
 * pblock is either nullptr or a pointer to a block that is already loaded
 * in memory (to avoid loading it from disk again).
 *
 * Returns true if a new chain tip was set.
 */
bool ActivateBestChain(
    const Config &config, CValidationState &state,
    std::shared_ptr<const CBlock> pblock = std::shared_ptr<const CBlock>());
Amount GetBlockSubsidy(int nHeight, const Consensus::Params &consensusParams);
Amount GetBlockRewardSubsidy(int nHeight, const Consensus::Params &consensusParams);

/**
 * Guess verification progress (as a fraction between 0.0=genesis and
 * 1.0=current tip).
 */
double GuessVerificationProgress(const ChainTxData &data,
                                 const CBlockIndex *pindex);

/**
 * Calculate the amount of disk space the block & undo files currently use.
 */
uint64_t CalculateCurrentUsage();

/**
 * Mark one block file as pruned.
 */
void PruneOneBlockFile(const int fileNumber);

/**
 * Actually unlink the specified files
 */
void UnlinkPrunedFiles(const std::set<int> &setFilesToPrune);

/** Flush all state, indexes and buffers to disk. */
void FlushStateToDisk();
/** Prune block files and flush state to disk. */
void PruneAndFlush();
/** Prune block files up to a given height */
void PruneBlockFilesManual(int nManualPruneHeight);

/**
 * (try to) add transaction to memory pool
 */
bool AcceptToMemoryPool(const Config &config, CTxMemPool &pool,
                        CValidationState &state, const CTransactionRef &tx,
                        bool fLimitFree, bool *pfMissingInputs,
                        bool fOverrideMempoolLimit = false,
                        const Amount nAbsurdFee = Amount::zero(),
                        bool test_accept = false);

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState &state);

/**
 * Check whether all inputs of this transaction are valid (no double spends,
 * scripts & sigs, amounts). This does not modify the UTXO set.
 *
 * If pvChecks is not nullptr, script checks are pushed onto it instead of being
 * performed inline. Any script checks which are not necessary (eg due to script
 * execution cache hits) are, obviously, not pushed onto pvChecks/run.
 *
 * Setting sigCacheStore/scriptCacheStore to false will remove elements from the
 * corresponding cache which are matched. This is useful for checking blocks
 * where we will likely never need the cache entry again.
 */
bool CheckInputs(const CTransaction &tx, CValidationState &state,
                 const CCoinsViewCache &view, bool fScriptChecks,
                 const uint32_t flags, bool sigCacheStore,
                 bool scriptCacheStore,
                 const PrecomputedTransactionData &txdata,
                 std::vector<CScriptCheck> *pvChecks = nullptr);

/**
 * Mark all the coins corresponding to a given transaction inputs as spent.
 */
void SpendCoins(CCoinsViewCache &view, const CTransaction &tx, CTxUndo &txundo,
                int nHeight);

/**
 * Apply the effects of this transaction on the UTXO set represented by view.
 */
void UpdateCoins(CCoinsViewCache &view, const CTransaction &tx, int nHeight);
void UpdateCoins(CCoinsViewCache &view, const CTransaction &tx, CTxUndo &txundo,
                 int nHeight);

/**
 * Test whether the LockPoints height and time are still valid on the current
 * chain.
 */
bool TestLockPointValidity(const LockPoints *lp);

/**
 * Check if transaction will be BIP 68 final in the next block to be created.
 *
 * Simulates calling SequenceLocks() with data from the tip of the current
 * active chain. Optionally stores in LockPoints the resulting height and time
 * calculated and the hash of the block needed for calculation or skips the
 * calculation and uses the LockPoints passed in for evaluation. The LockPoints
 * should not be considered valid if CheckSequenceLocks returns false.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckSequenceLocks(const CTransaction &tx, int flags,
                        LockPoints *lp = nullptr,
                        bool useExistingLockPoints = false);

/**
 * Closure representing one script verification.
 * Note that this stores references to the spending transaction.
 */
class CScriptCheck {
private:
    CScript scriptPubKey;
    Amount amount;
    const CTransaction *ptxTo;
    unsigned int nIn;
    uint32_t nFlags;
    bool cacheStore;
    ScriptError error;
    PrecomputedTransactionData txdata;

public:
    CScriptCheck()
        : amount(), ptxTo(nullptr), nIn(0), nFlags(0), cacheStore(false),
          error(SCRIPT_ERR_UNKNOWN_ERROR), txdata() {}

    CScriptCheck(const CScript &scriptPubKeyIn, const Amount amountIn,
                 const CTransaction &txToIn, unsigned int nInIn,
                 uint32_t nFlagsIn, bool cacheIn,
                 const PrecomputedTransactionData &txdataIn)
        : scriptPubKey(scriptPubKeyIn), amount(amountIn), ptxTo(&txToIn),
          nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn),
          error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) {}

    bool operator()();

    void swap(CScriptCheck &check) {
        scriptPubKey.swap(check.scriptPubKey);
        std::swap(ptxTo, check.ptxTo);
        std::swap(amount, check.amount);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};

/** Functions for disk access for blocks */
bool ReadBlockFromDisk(CBlock &block, const CDiskBlockPos &pos,
                       const Config &config);
bool ReadBlockFromDisk(CBlock &block, const CBlockIndex *pindex,
                       const Config &config);

/** Functions for validating blocks and updating the block tree */

/**
 * Context-independent validity checks.
 *
 * Returns true if the provided block is valid (has valid header,
 * transactions are valid, block is a valid size, etc.)
 */
bool CheckBlock(
    const Config &Config, const CBlock &block, CValidationState &state,
    BlockValidationOptions validationOptions = BlockValidationOptions());

/**
 * This is a variant of ContextualCheckTransaction which computes the contextual
 * check for a transaction based on the chain tip.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool ContextualCheckTransactionForCurrentBlock(const Config &config,
                                               const CTransaction &tx,
                                               CValidationState &state,
                                               int flags = -1);

/**
 * Check a block is completely valid from start to finish (only works on top of
 * our current best block, with cs_main held)
 */
bool TestBlockValidity(
    const Config &config, CValidationState &state, const CBlock &block,
    CBlockIndex *pindexPrev,
    BlockValidationOptions validationOptions = BlockValidationOptions());

/**
 * When there are blocks in the active chain with missing data, rewind the
 * chainstate and remove them from the block index.
 */
bool RewindBlockIndex(const Config &config);

/**
 * RAII wrapper for VerifyDB: Verify consistency of the block and coin
 * databases.
 */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const Config &config, CCoinsView *coinsview, int nCheckLevel,
                  int nCheckDepth);
};

/** Replay blocks that aren't fully applied to the database. */
bool ReplayBlocks(const Config &config, CCoinsView *view);

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex *FindForkInGlobalIndex(const CChain &chain,
                                   const CBlockLocator &locator);

/**
 * Treats a block as if it were received before others with the same work,
 * making it the active chain tip if applicable. Successive calls to
 * PreciousBlock() will override the effects of earlier calls. The effects of
 * calls to PreciousBlock() are not retained across restarts.
 *
 * Returns true if the provided block index successfully became the chain tip.
 */
bool PreciousBlock(const Config &config, CValidationState &state,
                   CBlockIndex *pindex);

/**
 * Mark a block as finalized.
 * A finalized block can not be reorged in any way.
 */
bool FinalizeBlockAndInvalidate(const Config &config, CValidationState &state,
                                CBlockIndex *pindex);

/** Mark a block as invalid. */
bool InvalidateBlock(const Config &config, CValidationState &state,
                     CBlockIndex *pindex);

/** Park a block. */
bool ParkBlock(const Config &config, CValidationState &state,
               CBlockIndex *pindex);

/** Remove invalidity status from a block and its descendants. */
bool ResetBlockFailureFlags(CBlockIndex *pindex);

/** Remove parked status from a block and its descendants. */
bool UnparkBlockAndChildren(CBlockIndex *pindex);

/** Remove parked status from a block. */
bool UnparkBlock(CBlockIndex *pindex);

/**
 * Retrieve the topmost finalized block.
 */
const CBlockIndex *GetFinalizedBlock();

/**
 * Checks if a block is finalized.
 */
bool IsBlockFinalized(const CBlockIndex *pindex);

/** The currently-connected chain of blocks (protected by cs_main). */
extern CChain &chainActive;

/**
 * Global variable that points to the coins database (protected by cs_main)
 */
extern std::unique_ptr<CCoinsViewDB> pcoinsdbview;

/**
 * Global variable that points to the active CCoinsView (protected by cs_main)
 */
extern std::unique_ptr<CCoinsViewCache> pcoinsTip;

/**
 * Global variable that points to the active block tree (protected by cs_main)
 */
extern std::unique_ptr<CBlockTreeDB> pblocktree;

/**
 * Return the spend height, which is one more than the inputs.GetBestBlock().
 * While checking, GetBestBlock() refers to the parent block. (protected by
 * cs_main)
 * This is also true for mempool checks.
 */
int GetSpendHeight(const CCoinsViewCache &inputs);

/**
 * Determine what nVersion a new block should use.
 */
int32_t ComputeBlockVersion(const CBlockIndex *pindexPrev,
                            const Consensus::Params &params);

/**
 * Reject codes greater or equal to this can be returned by AcceptToMemPool or
 * AcceptBlock for blocks/transactions, to signal internal conditions. They
 * cannot and should not be sent over the P2P network.
 */
static const unsigned int REJECT_INTERNAL = 0x100;
/** Too high fee. Can not be triggered by P2P transactions */
static const unsigned int REJECT_HIGHFEE = 0x100;
/** Block conflicts with a transaction already known */
static const unsigned int REJECT_AGAINST_FINALIZED = 0x103;

/** Get block file info entry for one block file */
CBlockFileInfo *GetBlockFileInfo(size_t n);

/** Dump the mempool to disk. */
bool DumpMempool();

/** Load the mempool from disk. */
bool LoadMempool(const Config &config);

#endif // BITCOIN_VALIDATION_H