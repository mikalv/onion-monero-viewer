//
// Created by mwo on 5/11/15.
//

#include "MicroCore.h"

namespace xmreg
{
    /**
     * The constructor is interesting, as
     * m_mempool and m_blockchain_storage depend
     * on each other.
     *
     * So basically m_mempool is initialized with
     * reference to Blockchain (i.e., Blockchain&)
     * and m_blockchain_storage is initialized with
     * reference to m_mempool (i.e., tx_memory_pool&)
     *
     * The same is done in cryptonode::core.
     */
    MicroCore::MicroCore():
            m_mempool(m_blockchain_storage),
            m_blockchain_storage(m_mempool)
    {}


    /**
     * Initialized the MicroCore object.
     *
     * Create BlockchainLMDB on the heap.
     * Open database files located in blockchain_path.
     * Initialize m_blockchain_storage with the BlockchainLMDB object.
     */
    bool
    MicroCore::init(const string& _blockchain_path)
    {
        int db_flags = 0;

        blockchain_path = _blockchain_path;

        //db_flags |= MDB_RDONLY;
        db_flags |= MDB_NOLOCK;
        //db_flags |= MDB_SYNC;

        //uint64_t DEFAULT_FLAGS = MDB_NOMETASYNC | MDB_NORDAHEAD;

        //db_flags = DEFAULT_FLAGS;

        BlockchainDB* db = nullptr;
        db = new BlockchainLMDB();                        

        try
        {
            // try opening lmdb database files
            db->open(blockchain_path, db_flags);
        }
        catch (const std::exception& e)
        {
            cerr << "Error opening database: " << e.what();
            return false;
        }

        // check if the blockchain database
        // is successful opened
        if(!db->is_open())
        {
            return false;
        }

        // initialize Blockchain object to manage
        // the database.
        return m_blockchain_storage.init(db, false);
    }

    /**
    * Get m_blockchain_storage.
    * Initialize m_blockchain_storage with the BlockchainLMDB object.
    */
    Blockchain&
    MicroCore::get_core()
    {
        return m_blockchain_storage;
    }

    /**
     * Get block by its height
     *
     * returns true if success
     */
    bool
    MicroCore::get_block_by_height(const uint64_t& height, block& blk)
    {

        crypto::hash block_id;

        try
        {
            block_id = m_blockchain_storage.get_block_id_by_height(height);
        }
        catch (const exception& e)
        {
            cerr << e.what() << endl;
            return false;
        }


        if (!m_blockchain_storage.get_block_by_hash(block_id, blk))
        {
            cerr << "Block with hash " << block_id
                 << "and height " << height << " not found!"
                 << endl;
            return false;
        }

        return true;
    }

    /**
     * Finds the first block created in a given day, e.g., 2015-05-22
     *
     *
     */
    bool
    MicroCore::get_block_by_date(const string& date, /* searched date */
                                 block& blk, /* block to be returned */
                                 uint64_t init_height, /* start looking from this height */
                                 const char* format)
    {

        // get the current blockchain height.
        uint64_t max_height = m_blockchain_storage.get_current_blockchain_height();

        if (init_height > max_height)
        {
            cerr << "Initial height higher than current blockchain height!" << endl;
            return false;
        }


        // first parse the string date into boost's ptime object
        dateparser parser {format};

        if (!parser(date))
        {
            throw runtime_error(string("Date format is incorrect: ") + date);
        }

        // change the requested date ptime into timestamp
        uint64_t searched_timestamp = static_cast<uint64_t>(xmreg::to_time_t(parser.pt));

        //cout << "searched_timestamp: " << searched_timestamp << endl;

        // get block at initial guest height
        block tmp_blk;

        if (!get_block_by_height(init_height, tmp_blk))
        {
            cerr << "Cant find block of height " << init_height << endl;
            return false;
        }

        // assume the initall block is correct
        blk = tmp_blk;

        // get timestamp of the initial block
        //cout << tmp_blk.timestamp  << ", " << searched_timestamp << endl;

        // if init block and time do not match, do iterative search for the correct block

        // if found init block was earlier than the search time, iterate increasing block heigths
        if (tmp_blk.timestamp < searched_timestamp)
        {
            for (uint64_t i = init_height + 1; i < max_height; ++i)
            {
                block tmp_blk2;
                if (!get_block_by_height(i, tmp_blk2))
                {
                    cerr << "Cant find block of height " << i << endl;
                    return false;
                }

                //cout << tmp_blk2.timestamp - searched_timestamp << endl;

                if (tmp_blk2.timestamp >= searched_timestamp)
                {

                    // take one before this one:

                    if (!get_block_by_height(--i, tmp_blk2))
                    {
                        cerr << "Cant find block of height " << i << endl;
                        return false;
                    }

                    blk = tmp_blk2;
                    break;
                }

            }
        }
        else
        {
            for (uint64_t i = init_height - 1; i >= 0; --i)
            {
                block tmp_blk2;
                if (!get_block_by_height(i, tmp_blk2))
                {
                    cerr << "Cant find block of height " << i << endl;
                    return false;
                }

                //cout << tmp_blk2.timestamp - searched_timestamp << endl;

                if (tmp_blk2.timestamp <= searched_timestamp)
                {
                    blk = tmp_blk2;
                    break;
                }
            }
        }

        return true;
    }



    /**
     * Get transaction tx from the blockchain using it hash
     */
    bool
    MicroCore::get_tx(const crypto::hash& tx_hash, transaction& tx)
    {
        try
        {
            // get transaction with given hash
            tx = m_blockchain_storage.get_db().get_tx(tx_hash);
        }
        catch (const exception& e)
        {
            cerr << e.what() << endl;
            return false;
        }

        return true;
    }

    bool
    MicroCore::get_tx(const string& tx_hash_str, transaction& tx)
    {

        // parse tx hash string to hash object
        crypto::hash tx_hash;

        if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
        {
            cerr << "Cant parse tx hash: " << tx_hash_str << endl;
            return false;
        }


        if (!get_tx(tx_hash, tx))
        {
            return false;
        }


        return true;
    }




    /**
     * Find output with given public key in a given transaction
     */
    bool
    MicroCore::find_output_in_tx(const transaction& tx,
                                 const public_key& output_pubkey,
                                 tx_out& out,
                                 size_t& output_index)
    {

        size_t idx {0};


        // search in the ouputs for an output which
        // public key matches to what we want
        auto it = std::find_if(tx.vout.begin(), tx.vout.end(),
                               [&](const tx_out& o)
                               {
                                   const txout_to_key& tx_in_to_key
                                           = boost::get<txout_to_key>(o.target);

                                   ++idx;

                                   return tx_in_to_key.key == output_pubkey;
                               });

        if (it != tx.vout.end())
        {
            // we found the desired public key
            out = *it;
            output_index = idx > 0 ? idx - 1 : idx;

            //cout << idx << " " << output_index << endl;

            return true;
        }

        return false;
    }


    /**
     * Returns tx hash in a given block which
     * contains given output's public key
     */
    bool
    MicroCore::get_tx_hash_from_output_pubkey(const public_key& output_pubkey,
                                              const uint64_t& block_height,
                                              crypto::hash& tx_hash,
                                              cryptonote::transaction& tx_found)
    {

        tx_hash = null_hash;

        // get block of given height
        block blk;
        if (!get_block_by_height(block_height, blk))
        {
            cerr << "Cant get block of height: " << block_height << endl;
            return false;
        }


        // get all transactions in the block found
        // initialize the first list with transaction for solving
        // the block i.e. coinbase.
        list<transaction> txs {blk.miner_tx};
        list<crypto::hash> missed_txs;

        if (!m_blockchain_storage.get_transactions(blk.tx_hashes, txs, missed_txs))
        {
            cerr << "Cant find transcations in block: " << block_height << endl;
            return false;
        }

        if (!missed_txs.empty())
        {
            cerr << "Transactions not found in blk: " << block_height << endl;

            for (const crypto::hash& h : missed_txs)
            {
                cerr << " - tx hash: " << h << endl;
            }

            return false;
        }


        // search outputs in each transactions
        // until output with pubkey of interest is found
        for (const transaction& tx : txs)
        {

            tx_out found_out;

            // we dont need here output_index
            size_t output_index;

            if (find_output_in_tx(tx, output_pubkey, found_out, output_index))
            {
                // we found the desired public key
                tx_hash = get_transaction_hash(tx);
                tx_found = tx;

                return true;
            }

        }

        return false;
    }


    uint64_t
    MicroCore::get_blk_timestamp(uint64_t blk_height)
    {
        cryptonote::block blk;

        if (!get_block_by_height(blk_height, blk))
        {
            cerr << "Cant get block by height: " << blk_height << endl;
            return 0;
        }

        return blk.timestamp;
    }


    /**
     * De-initialized Blockchain.
     *
     * since blockchain is opened as MDB_RDONLY
     * need to manually free memory taken on heap
     * by BlockchainLMDB
     */
    MicroCore::~MicroCore()
    {
        delete &m_blockchain_storage.get_db();
    }


    bool
    init_blockchain(const string& path,
                    MicroCore& mcore,
                    Blockchain*& core_storage)
    {

        // initialize the core using the blockchain path
        if (!mcore.init(path))
        {
            cerr << "Error accessing blockchain." << endl;
            return false;
        }

        // get the high level Blockchain object to interact
        // with the blockchain lmdb database
        core_storage = &(mcore.get_core());

        return true;
    }

    string
    MicroCore::get_blkchain_path()
    {
        return blockchain_path;
    }

}
