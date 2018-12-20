#ifndef _MAIN_ZEROCOIN_V3_H__
#define _MAIN_ZEROCOIN_V3_H__

#include "amount.h"
#include "chain.h"
#include "libzerocoin/sigma/Coin.h"
#include "consensus/validation.h"
#include <secp256k1/include/Scalar.h>
#include <secp256k1/include/GroupElement.h>
#include "libzerocoin/sigma/Params.h"
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include "hash_functions.h"

using namespace sigma;

// zerocoin parameters
extern sigma::ParamsV3 *ZCParamsV3;

// Test for zerocoin transaction version 3, TODO(martun): change the denominations in enum CoinDenominationV3 and here.
inline bool IsZerocoinTxV3(sigma::CoinDenominationV3 denomination, int coinId) {
  bool fTestNet = Params().NetworkIDString() == CBaseChainParams::TESTNET;

  if (fTestNet) {
    return ((denomination == CoinDenominationV3::ZQ_LOVELACE) && (coinId >= ZC_V2_TESTNET_SWITCH_ID_1))
      || ((denomination == CoinDenominationV3::ZQ_GOLDWASSER) && (coinId >= ZC_V2_TESTNET_SWITCH_ID_10))
      || ((denomination == CoinDenominationV3::ZQ_RACKOFF) && (coinId >= ZC_V2_TESTNET_SWITCH_ID_25))
      || ((denomination == CoinDenominationV3::ZQ_PEDERSEN) && (coinId >= ZC_V2_TESTNET_SWITCH_ID_50))
      || ((denomination == CoinDenominationV3::ZQ_WILLIAMSON) && (coinId >= ZC_V2_TESTNET_SWITCH_ID_100));
  }
  else {
    return ((denomination == CoinDenominationV3::ZQ_LOVELACE) && (coinId >= ZC_V2_SWITCH_ID_1))
      || ((denomination == CoinDenominationV3::ZQ_GOLDWASSER) && (coinId >= ZC_V2_SWITCH_ID_10))
      || ((denomination == CoinDenominationV3::ZQ_RACKOFF) && (coinId >= ZC_V2_SWITCH_ID_25))
      || ((denomination == CoinDenominationV3::ZQ_PEDERSEN) && (coinId >= ZC_V2_SWITCH_ID_50))
      || ((denomination == CoinDenominationV3::ZQ_WILLIAMSON) && (coinId >= ZC_V2_SWITCH_ID_100));
  }
}

// Zerocoin transaction info, added to the CBlock to ensure zerocoin mint/spend transactions got their info stored into
// index
class CZerocoinTxInfoV3 {
public:
    // all the zerocoin transactions encountered so far
    std::set<uint256> zcTransactions;

    // <denomination, pubCoin> for all the mints
    // TODO(martun): remove denomination from here, it's already present in the pubCoin itself.
    std::vector<pair<int, PublicCoinV3> > mints;
  
    // serial for every spend (map from serial to denomination)
    std::unordered_map<Scalar, int, sigma::CScalarHash> spentSerials;

    // are there v1 spends in the block?
    bool fHasSpendV1;

    // are there v2 spends in the block?
    bool fHasSpendV2;

    // information about transactions in the block is complete
    bool fInfoIsComplete;

    CZerocoinTxInfoV3(): fInfoIsComplete(false) {}

    // finalize everything
    void Complete();
};

bool CheckZerocoinTransactionV3(
  const CTransaction &tx,
	CValidationState &state,
	uint256 hashTx,
	bool isVerifyDB,
	int nHeight,
  bool isCheckWallet,
  CZerocoinTxInfoV3 *zerocoinTxInfo);

void DisconnectTipZCV3(CBlock &block, CBlockIndex *pindexDelete);

bool ConnectBlockZCV3(
  CValidationState& state, 
  const CChainParams& chainparams, 
  CBlockIndex* pindexNew, 
  const CBlock *pblock, 
  bool fJustCheck=false);

bool ZerocoinBuildStateFromIndexV3(CChain *chain);

Scalar ZerocoinGetSpendSerialNumberV3(const CTransaction &tx);

/*
 * State of minted/spent coins as extracted from the index
 */
class CZerocoinStateV3 {
friend bool ZerocoinBuildStateFromIndexV3(CChain *, set<CBlockIndex *> &);
public:
    // First and last block where mint with given denomination and id was seen
    struct CoinGroupInfoV3 {
        CoinGroupInfoV3() : firstBlock(NULL), lastBlock(NULL), nCoins(0) {}

        // first and last blocks having coins with given denomination and id minted
        CBlockIndex *firstBlock;
        CBlockIndex *lastBlock;
        // total number of minted coins with such parameters
        int nCoins;
    };

// private: TODO(martun): change back to public later on.
    struct CMintedCoinInfo {
        int         denomination;
        int         id;
        int         nHeight;
    };

    struct pairhash {
      public:
        template <typename T, typename U>
          std::size_t operator()(const std::pair<T, U> &x) const
          {
            return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
          }
    };

    // Collection of coin groups. Map from <denomination,id> to CoinGroupInfoV3 structure
    std::unordered_map<pair<int, int>, CoinGroupInfoV3, pairhash> coinGroups;

    // Map from <denomination, coin set id> to a vector of public coins minted for each coin set.
//    std::unordered_map<pair<int, int>, std::vector<PublicCoinV3>, pairhash> all_minted_coins;
    
    // Set of all minted pubCoin values, keyed by the public coin. 
    // Used for checking if the given coin already exists.
    unordered_map<PublicCoinV3, CMintedCoinInfo, sigma::CPublicCoinHash> mintedPubCoins;

    // Latest IDs of coins by denomination
    std::unordered_map<int, int> latestCoinIds;

    // Set of all used coin serials.
    std::unordered_set<Scalar, sigma::CScalarHash> usedCoinSerials;

    // serials of spends currently in the mempool mapped to tx hashes
    std::unordered_map<Scalar, uint256, sigma::CScalarHash> mempoolCoinSerials;

public:
    CZerocoinStateV3();

    // Add mint, automatically assigning id to it. Returns id and previous accumulator value (if any)
    int AddMint(
        CBlockIndex *index, 
        const PublicCoinV3& pubCoin);

    // Add serial to the list of used ones
    void AddSpend(const Scalar& serial);

    // Add everything from the block to the state
    void AddBlock(CBlockIndex *index);

    // Disconnect block from the chain rolling back mints and spends
    void RemoveBlock(CBlockIndex *index);

    // Query coin group with given denomination and id
    bool GetCoinGroupInfo(int denomination, int id, CoinGroupInfoV3 &result);

    // Query if the coin serial was previously used
    bool IsUsedCoinSerial(const Scalar& coinSerial);

    // Query if there is a coin with given pubCoin value
    bool HasCoin(const PublicCoinV3& pubCoin);

    // Given denomination and id returns latest accumulator value and corresponding block hash
    // Do not take into account coins with height more than maxHeight
    // Returns number of coins satisfying conditions
    int GetCoinSetForSpend(
        CChain *chain, 
        int maxHeight, 
        int denomination, 
        int id, 
        uint256& blockHash_out, 
        std::vector<PublicCoinV3>& coins_out);

    // Get witness
    //libzerocoin::AccumulatorWitness GetWitnessForSpend(
    //    CChain *chain, 
    //    int maxHeight, 
    //    int denomination, 
    //    int id, 
    //    const PublicCoinV3& pubCoin);

    // Return height of mint transaction and id of minted coin
    std::pair<int, int> GetMintedCoinHeightAndId(
        const PublicCoinV3& pubCoin,
        int denomination);

    // Reset to initial values
    void Reset();

    // Check if there is a conflicting tx in the blockchain or mempool
    bool CanAddSpendToMempool(const Scalar& coinSerial);

    // Add spend into the mempool.
    // Check if there is a coin with such serial in either blockchain or mempool
    bool AddSpendToMempool(const Scalar &coinSerial, uint256 txHash);

    // Get conflicting tx hash by coin serial number
    uint256 GetMempoolConflictingTxHash(const Scalar& coinSerial);

    // Remove spend from the mempool (usually as the result of adding tx to the block)
    void RemoveSpendFromMempool(const Scalar& coinSerial);

    static CZerocoinStateV3* GetZerocoinState();
};

#endif // _MAIN_ZEROCOIN_V3_H__
