//
// Created by mwo on 8/04/16.
//

#ifndef CROWXMR_PAGE_H
#define CROWXMR_PAGE_H



#include "mstch/mstch.hpp"
#include "rapidjson/document.h"
#include "../ext/format.h"
#include "../ext/member_checker.h"


#include "monero_headers.h"

#include "MicroCore.h"
#include "tools.h"
#include "rpccalls.h"
#include "mylmdb.h"

#include <algorithm>
#include <limits>
#include <ctime>
#include <thread>

#define TMPL_DIR             "./templates"
#define TMPL_PARIALS_DIR     TMPL_DIR "/partials"
#define TMPL_INDEX           TMPL_DIR "/index.html"
#define TMPL_INDEX2          TMPL_DIR "/index2.html"
#define TMPL_MEMPOOL         TMPL_DIR "/mempool.html"
#define TMPL_HEADER          TMPL_DIR "/header.html"
#define TMPL_FOOTER          TMPL_DIR "/footer.html"
#define TMPL_BLOCK           TMPL_DIR "/block.html"
#define TMPL_TX              TMPL_DIR "/tx.html"
#define TMPL_MY_OUTPUTS      TMPL_DIR "/my_outputs.html"
#define TMPL_SEARCH_RESULTS  TMPL_DIR "/search_results.html"

namespace xmreg {


    using namespace cryptonote;
    using namespace crypto;
    using namespace std;

    // define a checker to test if a structure has "tx_blob"
    // member variable. I use modified daemon with few extra
    // bits and pieces here and there. One of them is
    // tx_blob in cryptonote::tx_info structure
    // thus I check if I run my version, or just
    // generic one
    DEFINE_MEMBER_CHECKER(tx_blob)

    // define getter to get tx_blob, i.e., get_tx_blob function
    // as string if exists. the getter return empty string if
    // tx_blob does not exist
    DEFINE_MEMBER_GETTER(tx_blob, string)

    /**
     * @brief The tx_details struct
     *
     * Basic information about tx
     *
     */
    struct tx_details
    {
        crypto::hash hash;
        crypto::public_key pk;
        uint64_t xmr_inputs;
        uint64_t xmr_outputs;
        uint64_t fee;
        uint64_t mixin_no;
        uint64_t size;
        size_t   version;
        uint64_t unlock_time;
        vector<uint8_t> extra;

        crypto::hash  payment_id  = null_hash; // normal
        crypto::hash8 payment_id8 = null_hash8; // encrypted

        std::vector<std::vector<crypto::signature> > signatures;

        // key images of inputs
        vector<txin_to_key> input_key_imgs;

        // public keys and xmr amount of outputs
        vector<pair<txout_to_key, uint64_t>> output_pub_keys;

        mstch::map
        get_mstch_map()
        {
            // remove "<" and ">" from the hash string
            string tx_hash_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}", hash));

            string tx_pk_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}", pk));

            //cout << "payment_id: " << payment_id << endl;

            string pid_str   = REMOVE_HASH_BRAKETS(fmt::format("{:s}", payment_id));
            string pid8_str  = REMOVE_HASH_BRAKETS(fmt::format("{:s}", payment_id8));

            string mixin_str {"N/A"};
            string fee_str {"N/A"};
            string fee_short_str {"N/A"};

            if (!input_key_imgs.empty())
            {
                mixin_str     = std::to_string(mixin_no - 1);
                fee_str       = fmt::format("{:0.6f}", XMR_AMOUNT(fee));
                fee_short_str = fmt::format("{:0.3f}", XMR_AMOUNT(fee));
            }


            //cout << "extra: " << extra_str << endl;

            mstch::map txd_map {
                {"hash"              , tx_hash_str},
                {"pub_key"           , tx_pk_str},
                {"tx_fee"            , fee_str},
                {"tx_fee_short"      , fee_short_str},
                {"sum_inputs"        , fmt::format("{:0.6f}", XMR_AMOUNT(xmr_inputs))},
                {"sum_outputs"       , fmt::format("{:0.6f}", XMR_AMOUNT(xmr_outputs))},
                {"sum_inputs_short"  , fmt::format("{:0.3f}", XMR_AMOUNT(xmr_inputs))},
                {"sum_outputs_short" , fmt::format("{:0.3f}", XMR_AMOUNT(xmr_outputs))},
                {"no_inputs"         , input_key_imgs.size()},
                {"no_outputs"        , output_pub_keys.size()},
                {"mixin"             , mixin_str},
                {"version"           , std::to_string(version)},
                {"has_payment_id"    , payment_id  != null_hash},
                {"has_payment_id8"   , payment_id8 != null_hash8},
                {"payment_id"        , pid_str},
                {"extra"             , get_extra_str()},
                {"payment_id8"       , pid8_str},
                {"unlock_time"       , std::to_string(unlock_time)},
                {"tx_size"           , fmt::format("{:0.4f}",
                                                   static_cast<double>(size)/1024.0)},
                {"tx_size_short"     , fmt::format("{:0.2f}",
                                                   static_cast<double>(size)/1024.0)}
            };


            return txd_map;
        }


        string
        get_extra_str()
        {

            string extra_str = epee::string_tools::buff_to_hex_nodelimer(
                        string{reinterpret_cast<const char*>(extra.data()), extra.size()});

            return extra_str;
        }


        mstch::array
        get_ring_sig_for_input(uint64_t in_i)
        {
             mstch::array ring_sigs {};

             if (in_i >= signatures.size())
             {
                 return ring_sigs;
             }

             for (const crypto::signature &sig: signatures.at(in_i))
             {
                 ring_sigs.push_back(mstch::map{
                     {"ring_sig", print_signature(sig)}
                 });
             }

             return ring_sigs;
        }

        string
        print_signature(const signature& sig)
        {
             stringstream ss;

             ss << epee::string_tools::pod_to_hex(sig.c)
                << epee::string_tools::pod_to_hex(sig.r);

             return ss.str();
        }
    };


    struct search_class_test
    {
        static xmreg::MyLMDB mylmdb ;

        MicroCore* mcore;
        Blockchain* core_storage;

        string xmr_address_str ;
        string viewkey_str;
        uint64_t since_when;

        uint64_t current_blockchain_height;

        uint64_t block_id;

        bool user_left;

        string timestamp_str;

        mstch::array outputs;



        cryptonote::account_public_address address;
        crypto::secret_key prv_view_key;

        search_class_test(MicroCore* _mcore,
                          Blockchain* _core_storage,
                          string _xmr_address,
                          string _viewkey,
                          uint64_t _since_when,
                          uint64_t _height
        )
                :
                  mcore {_mcore},
                  core_storage {_core_storage},
                  xmr_address_str {_xmr_address},
                  viewkey_str {_viewkey},
                  since_when {_since_when},
                  current_blockchain_height {_height},
                  block_id{0},
                  user_left{false}
        {
            const set<uint64_t> possible_since_when_values {1, 7, 14, 28, 56};

            // remove white characters
            boost::trim(xmr_address_str);
            boost::trim(viewkey_str);

            if (xmr_address_str.empty())
            {
                return;
            }

            if (viewkey_str.empty())
            {
                return;
            }

            // check if since_when is correct value
            if (!possible_since_when_values.count(since_when))
            {
                string err_msg = fmt::format(
                        "Since when value {:d} is incorrect", since_when);
                cerr << err_msg << endl;
                return;
            }

            // parse string representing given monero address
            if (!xmreg::parse_str_address(xmr_address_str,  address, 0))
            {
                cerr << "Cant parse string address: " << xmr_address_str << endl;
                return;
            }

            // parse string representing given private viewkey


            if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
            {
                cerr << "Cant parse view key: " << viewkey_str << endl;
                return;
            }

        };

        void
        search()
        {

            // rough estimate of number of recent blocks to search
            // from the current block. Monero blocks now are, on average,
            // every 120 seconds, so we use this to get the estimate
            uint64_t no_of_blocks_to_search = since_when*24*3600 / 120;

            uint64_t tx_blk_height {current_blockchain_height - no_of_blocks_to_search};

            uint64_t out_idx = {0};

            uint64_t sum_xmr {0};


            for (uint64_t i = tx_blk_height; i <= current_blockchain_height; ++i)
            {

                if (user_left)
                    return;

                // get block at the given height i
                block blk;

                if (!mcore->get_block_by_height(i, blk))
                {
                    cerr << "Cant get block: " << i << endl;
                    continue;
                }

                crypto::hash block_id;

                vector<output_info> outputs_info;

                mylmdb.get_output_info(blk.timestamp, outputs_info);


                this->block_id = i;
                this->timestamp_str = xmreg::timestamp_to_str(blk.timestamp);

                cout << "blk: << " << i << " output no.: " << outputs_info.size() << endl;

                //std::this_thread::sleep_for(std::chrono::seconds(1));

                crypto::hash previous_tx_hash {null_hash};

                // go through all outputs in each block, based on timestamp,
                // and search for our outputs
                for (const xmreg::output_info out_info : outputs_info)
                {
                    // public transaction key is combined with our viewkey
                    // to create, so called, derived key.
                    crypto::key_derivation derivation;

                    bool r = generate_key_derivation(out_info.tx_pub_key,
                                                     prv_view_key,
                                                     derivation);

                    if (!r)
                    {
                        cerr << "cant derive key for output: "
                             << out_info.out_pub_key << endl;
                        continue;
                    }

                    // get the tx output public key
                    // that normally would be generated for us,
                    // if someone had sent us some xmr.
                    crypto::public_key generated_pubkey;

                    derive_public_key(derivation,
                                      out_info.index_in_tx,
                                      address.m_spend_public_key,
                                      generated_pubkey);

                    bool same_tx {false};

                    if (out_info.out_pub_key == generated_pubkey)
                    {

                        cout << "found output " << endl;

                        sum_xmr += out_info.amount;

                        string timestamp_str = xmreg::timestamp_to_str(blk.timestamp);


                        same_tx = (previous_tx_hash == out_info.tx_hash);

                        string out_pub_key_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}",
                                                                     out_info.out_pub_key));

                        string tx_hash_str      = REMOVE_HASH_BRAKETS(fmt::format("{:s}",
                                                                      out_info.tx_hash));

                        outputs.push_back(mstch::map {
                                {"out_pub_key"  , out_pub_key_str},
                                {"amount"       , fmt::format("{:0.12f}", XMR_AMOUNT(out_info.amount))},
                                {"output_idx"   , fmt::format("{:04d}", ++out_idx)},
                                {"tx_hash"      , tx_hash_str},
                                {"blk_timestamp", timestamp_str},
                                {"same_tx"      , !same_tx}
                        });

                        previous_tx_hash = out_info.tx_hash;

                    }
                } // for (const xmreg::output_info out_info : outputs_info)
            } // for (uint64_t i = tx_blk_height;
        }

        ~search_class_test()
        {
            cout << "destroy search_class_test" << endl;
        }
    };

    xmreg::MyLMDB search_class_test::mylmdb
            = xmreg::MyLMDB{"/home/mwo/.bitmonero/lmdb2"};


    class page {

        // check if we have tx_blob member in tx_info structure
        static const bool HAVE_TX_BLOB {
            HAS_MEMBER(cryptonote::tx_info, tx_blob)
        };

        static const bool FULL_AGE_FORMAT {true};

        MicroCore* mcore;
        Blockchain* core_storage;
        rpccalls rpc;
        time_t server_timestamp;

        string lmdb2_path;


        map<string, shared_ptr<xmreg::search_class_test>> searching_threads;


    public:

        page(MicroCore* _mcore, Blockchain* _core_storage,
             string _deamon_url, string _lmdb2_path)
                : mcore {_mcore},
                  core_storage {_core_storage},
                  rpc {_deamon_url},
                  server_timestamp {std::time(nullptr)},
                  lmdb2_path {_lmdb2_path}
        {

        }

        void add_searching_thread(string uuid, shared_ptr<xmreg::search_class_test>& search_cls)
        {
            searching_threads[uuid] = search_cls;
        }

        /**
         * @brief show recent transactions and mempool
         * @param page_no block page to show
         * @param refresh_page enable autorefresh
         * @return rendered index page
         */
        string
        index2(uint64_t page_no = 0, bool refresh_page = false)
        {

            //get current server timestamp
            server_timestamp = std::time(nullptr);

            // get the current blockchain height. Just to check
            uint64_t height =
                    xmreg::MyLMDB::get_blockchain_height(mcore->get_blkchain_path()) - 1;

            // initalise page tempate map with basic info about blockchain
            mstch::map context {
                    {"refresh"         , refresh_page},
                    {"height"          , std::to_string(height)},
                    {"server_timestamp", xmreg::timestamp_to_str(server_timestamp)}
            };

            // read index.html
            string index2_html = xmreg::read(TMPL_INDEX2);

            // add header and footer
            string full_page = get_full_page(index2_html);

            // render the page
            return mstch::render(full_page, context);
        }


        /**
         * Render mempool data
         */
        string
        mempool()
        {
            std::vector<tx_info> mempool_txs;

            if (!rpc.get_mempool(mempool_txs))
            {
              return "Getting mempool failed";
            }

            // initalise page tempate map with basic info about mempool
            mstch::map context {
                    {"mempool_size",  std::to_string(mempool_txs.size())},
                    {"mempooltxs" ,   mstch::array()}
            };

            // get reference to blocks template map to be field below
            mstch::array& txs = boost::get<mstch::array>(context["mempooltxs"]);

            // for each transaction in the memory pool
            for (size_t i = 0; i < mempool_txs.size(); ++i)
            {
                // get transaction info of the tx in the mempool
                tx_info _tx_info = mempool_txs.at(i);

                // calculate difference between tx in mempool and server timestamps
                array<size_t, 5> delta_time = timestamp_difference(
                                              server_timestamp,
                                              _tx_info.receive_time);

                // use only hours, so if we have days, add
                // it to hours
                uint64_t delta_hours {delta_time[1]*24 + delta_time[2]};

                string age_str = fmt::format("{:02d}:{:02d}:{:02d}",
                                             delta_hours,
                                             delta_time[3], delta_time[4]);

                // if more than 99 hourse, change formating
                // for the template
                if (delta_hours > 99)
                {
                    age_str = fmt::format("{:03d}:{:02d}:{:02d}",
                                             delta_hours,
                                             delta_time[3], delta_time[4]);
                }

                // sum xmr in inputs and ouputs in the given tx
                pair<uint64_t, uint64_t> sum_inputs  = sum_xmr_inputs(_tx_info.tx_json);
                pair<uint64_t, uint64_t> sum_outputs = sum_xmr_outputs(_tx_info.tx_json);

                // get mixin number in each transaction
                vector<uint64_t> mixin_numbers = get_mixin_no_in_txs(_tx_info.tx_json);

                // set output page template map
                txs.push_back(mstch::map {
                        {"timestamp"     , xmreg::timestamp_to_str(_tx_info.receive_time)},
                        {"age"           , age_str},
                        {"hash"          , fmt::format("{:s}", _tx_info.id_hash)},
                        {"fee"           , fmt::format("{:0.3f}", XMR_AMOUNT(_tx_info.fee))},
                        {"xmr_inputs"    , fmt::format("{:0.2f}", XMR_AMOUNT(sum_inputs.first))},
                        {"xmr_outputs"   , fmt::format("{:0.2f}", XMR_AMOUNT(sum_outputs.first))},
                        {"no_inputs"     , sum_inputs.second},
                        {"no_outputs"    , sum_outputs.second},
                        {"mixin"         , fmt::format("{:d}", mixin_numbers.at(0) - 1)},
                        {"txsize"        , fmt::format("{:0.2f}", static_cast<double>(_tx_info.blob_size)/1024.0)}
                });
            }

            // read index.html
            string mempool_html = xmreg::read(TMPL_MEMPOOL);

            // render the page
            return mstch::render(mempool_html, context);
        }


        string
        show_block(uint64_t _blk_height)
        {
            // get block at the given height i
            block blk;

            if (!mcore->get_block_by_height(_blk_height, blk))
            {
                cerr << "Cant get block: " << _blk_height << endl;
                return fmt::format("Cant get block {:d}!", _blk_height);
            }

            // get block's hash
            crypto::hash blk_hash = core_storage->get_block_id_by_height(_blk_height);

            crypto::hash prev_hash = blk.prev_id;
            crypto::hash next_hash = core_storage->get_block_id_by_height(_blk_height + 1);

            bool have_next_hash = (next_hash == null_hash ? false : true);
            bool have_prev_hash = (prev_hash == null_hash ? false : true);

            // remove "<" and ">" from the hash string
            string prev_hash_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}", prev_hash));
            string next_hash_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}", next_hash));

            // remove "<" and ">" from the hash string
            string blk_hash_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}", blk_hash));

            // get block timestamp in user friendly format
            string blk_timestamp = xmreg::timestamp_to_str(blk.timestamp);

            // get age of the block relative to the server time
            pair<string, string> age = get_age(server_timestamp, blk.timestamp);

            // get time from the last block
            string delta_time {"N/A"};

            if (have_prev_hash)
            {
                block prev_blk = core_storage->get_db().get_block(prev_hash);

                pair<string, string> delta_diff = get_age(blk.timestamp, prev_blk.timestamp);

                delta_time = delta_diff.first;
            }

            // get block size in bytes
            uint64_t blk_size = get_object_blobsize(blk);

            // miner reward tx
            transaction coinbase_tx = blk.miner_tx;

            // transcation in the block
            vector<crypto::hash> tx_hashes = blk.tx_hashes;


            bool have_txs = !blk.tx_hashes.empty();

            // sum of all transactions in the block
            uint64_t sum_fees = 0;

            // get tx details for the coinbase tx, i.e., miners reward
            tx_details txd_coinbase = get_tx_details(blk.miner_tx, true);

            // initalise page tempate map with basic info about blockchain
            mstch::map context {
                    {"blk_hash"       , blk_hash_str},
                    {"blk_height"     , _blk_height},
                    {"blk_timestamp"  , blk_timestamp},
                    {"prev_hash"      , prev_hash_str},
                    {"next_hash"      , next_hash_str},
                    {"have_next_hash" , have_next_hash},
                    {"have_prev_hash" , have_prev_hash},
                    {"have_txs"       , have_txs},
                    {"no_txs"         , std::to_string(blk.tx_hashes.size())},
                    {"blk_age"        , age.first},
                    {"delta_time"     , delta_time},
                    {"blk_nonce"      , blk.nonce},
                    {"age_format"     , age.second},
                    {"major_ver"      , std::to_string(blk.major_version)},
                    {"minor_ver"      , std::to_string(blk.minor_version)},
                    {"blk_size"       , fmt::format("{:0.4f}",
                                                    static_cast<double>(blk_size) / 1024.0)},
                    {"coinbase_txs"   , mstch::array{{txd_coinbase.get_mstch_map()}}},
                    {"blk_txs"        , mstch::array()}
            };

            // .push_back(txd_coinbase.get_mstch_map()

           // boost::get<mstch::array>(context["blk_txs"]).push_back(txd_coinbase.get_mstch_map());

            // now process nomral transactions
            // get reference to blocks template map to be field below
            mstch::array& txs = boost::get<mstch::array>(context["blk_txs"]);

            // timescale representation for each tx in the block
            vector<string> mixin_timescales_str;

            // for each transaction in the block
            for (size_t i = 0; i < blk.tx_hashes.size(); ++i)
            {
                // get transaction info of the tx in the mempool
                const crypto::hash& tx_hash = blk.tx_hashes.at(i);

                // remove "<" and ">" from the hash string
                string tx_hash_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}", tx_hash));


                // get transaction
                transaction tx;

                if (!mcore->get_tx(tx_hash, tx))
                {
                    cerr << "Cant get tx: " << tx_hash << endl;
                    continue;
                }

                tx_details txd = get_tx_details(tx);

                // add fee to the rest
                sum_fees += txd.fee;


                // get mixins in time scale for visual representation
                //string mixin_times_scale = xmreg::timestamps_time_scale(mixin_timestamps,
                //                                                        server_timestamp);


                // add tx details mstch map to context
                txs.push_back(txd.get_mstch_map());
            }


            // add total fees in the block to the context
            context["sum_fees"]   = fmt::format("{:0.6f}",
                                         XMR_AMOUNT(sum_fees));

            // get xmr in the block reward
            context["blk_reward"] = fmt::format("{:0.6f}",
                                         XMR_AMOUNT(txd_coinbase.xmr_outputs - sum_fees));

            // read block.html
            string block_html = xmreg::read(TMPL_BLOCK);

            // add header and footer
            string full_page = get_full_page(block_html);

            // render the page
            return mstch::render(full_page, context);
        }


        string
        show_block(string _blk_hash)
        {
            crypto::hash blk_hash;

            if (!xmreg::parse_str_secret_key(_blk_hash, blk_hash))
            {
                cerr << "Cant parse blk hash: " << blk_hash << endl;
                return fmt::format("Cant get block {:s} due to block hash parse error!", blk_hash);
            }

            uint64_t blk_height;

            try
            {
                blk_height = core_storage->get_db().get_block_height(blk_hash);
            }
            catch (const BLOCK_DNE& e)
            {
                cerr << "Block does not exist: " << blk_hash << endl;
                return fmt::format("Cant get block {:s} because it does not exist!", blk_hash);
            }
            catch (const std::exception& e)
            {
                cerr << "Cant get block: " << blk_hash << endl;
                return fmt::format("Cant get block {:s} for some uknown reason", blk_hash);
            }

            return show_block(blk_height);
        }

        string
        show_tx(string tx_hash_str, uint with_ring_signatures = 0)
        {

            // parse tx hash string to hash object
            crypto::hash tx_hash;

            if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
            {
                cerr << "Cant parse tx hash: " << tx_hash_str << endl;
                return string("Cant get tx hash due to parse error: " + tx_hash_str);
            }

            // tx age
            pair<string, string> age;

            string blk_timestamp {"N/A"};

            // get transaction
            transaction tx;

            if (!mcore->get_tx(tx_hash, tx))
            {
                cerr << "Cant get tx in blockchain: " << tx_hash
                     << ". \n Check mempool now" << endl;

                vector<pair<tx_info, transaction>> found_txs
                        = search_mempool(tx_hash);

                if (!found_txs.empty())
                {
                    // there should be only one tx found
                    tx = found_txs.at(0).second;

                    // since its tx in mempool, it has no blk yet
                    // so use its recive_time as timestamp to show

                    uint64_t tx_recieve_timestamp
                            = found_txs.at(0).first.receive_time;

                    blk_timestamp = xmreg::timestamp_to_str(tx_recieve_timestamp);

                    age = get_age(server_timestamp, tx_recieve_timestamp,
                                  FULL_AGE_FORMAT);
                }
                else
                {
                    // tx is nowhere to be found :-(
                    return string("Cant get tx: " + tx_hash_str);
                }
            }

            tx_details txd = get_tx_details(tx);

            uint64_t tx_blk_height {0};

            bool tx_blk_found {false};

            try
            {
                tx_blk_height = core_storage->get_db().get_tx_block_height(tx_hash);
                tx_blk_found = true;
            }
            catch (exception& e)
            {
                cerr << "Cant get block height: " << tx_hash
                     << e.what() << endl;
            }


            // get block cointaining this tx
            block blk;

            if (tx_blk_found && !mcore->get_block_by_height(tx_blk_height, blk))
            {
                cerr << "Cant get block: " << tx_blk_height << endl;
            }

            string tx_blk_height_str {"N/A"};

            if (tx_blk_found)
            {
                // calculate difference between tx and server timestamps
                age = get_age(server_timestamp, blk.timestamp, FULL_AGE_FORMAT);

                blk_timestamp = xmreg::timestamp_to_str(blk.timestamp);

                tx_blk_height_str = std::to_string(tx_blk_height);
            }

            // payments id. both normal and encrypted (payment_id8)
            string pid_str   = REMOVE_HASH_BRAKETS(fmt::format("{:s}", txd.payment_id));
            string pid8_str  = REMOVE_HASH_BRAKETS(fmt::format("{:s}", txd.payment_id8));


            // initalise page tempate map with basic info about blockchain
            mstch::map context {
                    {"tx_hash"              , tx_hash_str},
                    {"tx_pub_key"           , REMOVE_HASH_BRAKETS(fmt::format("{:s}", txd.pk))},
                    {"blk_height"           , tx_blk_height_str},
                    {"tx_size"              , fmt::format("{:0.4f}",
                                                   static_cast<double>(txd.size) / 1024.0)},
                    {"tx_fee"               , fmt::format("{:0.12f}", XMR_AMOUNT(txd.fee))},
                    {"blk_timestamp"        , blk_timestamp},
                    {"blk_timestamp_uint"   , blk.timestamp},
                    {"delta_time"           , age.first},
                    {"inputs_no"            , txd.input_key_imgs.size()},
                    {"has_inputs"           , !txd.input_key_imgs.empty()},
                    {"outputs_no"           , txd.output_pub_keys.size()},
                    {"has_payment_id"       , txd.payment_id  != null_hash},
                    {"has_payment_id8"      , txd.payment_id8 != null_hash8},
                    {"payment_id"           , pid_str},
                    {"payment_id8"          , pid8_str},
                    {"extra"                , txd.get_extra_str()},
                    {"with_ring_signatures" , static_cast<bool>(with_ring_signatures)}
            };

            string server_time_str = xmreg::timestamp_to_str(server_timestamp, "%F");

            mstch::array inputs = mstch::array{};

            mstch::array mixins_timescales;

            double timescale_scale {0.0}; // size of one '_' in days

            uint64_t input_idx {0};

            // make timescale maps for mixins in input
            for (const txin_to_key& in_key: txd.input_key_imgs)
            {
                // get absolute offsets of mixins
                std::vector<uint64_t> absolute_offsets
                        = cryptonote::relative_output_offsets_to_absolute(
                                in_key.key_offsets);

                // get public keys of outputs used in the mixins that match to the offests
                std::vector<cryptonote::output_data_t> outputs;
                core_storage->get_db().get_output_key(in_key.amount,
                                                      absolute_offsets,
                                                      outputs);

                vector<uint64_t> mixin_timestamps;

                inputs.push_back(mstch::map {
                    {"in_key_img", REMOVE_HASH_BRAKETS(fmt::format("{:s}", in_key.k_image))},
                    {"amount"    , fmt::format("{:0.12f}", XMR_AMOUNT(in_key.amount))},
                    {"input_idx" , fmt::format("{:02d}", input_idx)},
                    {"mixins"    , mstch::array{}},
                    {"ring_sigs" , txd.get_ring_sig_for_input(input_idx)}
                });


                // get reference to mixins array created above
                mstch::array& mixins = boost::get<mstch::array>(
                            boost::get<mstch::map>(inputs.back())["mixins"]);

                // mixin counter
                size_t count = 0;

                // for each found output public key find its block to get timestamp
                for (const uint64_t &i: absolute_offsets)
                {
                    // get basic information about mixn's output
                    cryptonote::output_data_t output_data = outputs.at(count);

                    // get pair pair<crypto::hash, uint64_t> where first is tx hash
                    // and second is local index of the output i in that tx
                    tx_out_index tx_out_idx =
                            core_storage->get_db().get_output_tx_and_index(in_key.amount, i);

                    // get block of given height, as we want to get its timestamp
                    cryptonote::block blk;

                    if (!mcore->get_block_by_height(output_data.height, blk))
                    {
                        cerr << "- cant get block of height: " << output_data.height << endl;
                        return fmt::format("- cant get block of height: {}", output_data.height);
                    }

                    // get age of mixin relative to server time
                    pair<string, string> mixin_age = get_age(server_timestamp,
                                                             blk.timestamp,
                                                             FULL_AGE_FORMAT);
                    // get mixin transaction
                    transaction mixin_tx;

                    if (!mcore->get_tx(tx_out_idx.first, mixin_tx))
                    {
                        cerr << "Cant get tx: " << tx_out_idx.first << endl;
                        return fmt::format("Cant get tx: {:s}", tx_out_idx.first);
                    }

                     // mixin tx details
                    tx_details mixin_txd = get_tx_details(mixin_tx, true);

                    mixins.push_back(mstch::map {
                            {"mix_blk"        , fmt::format("{:08d}", output_data.height)},
                            {"mix_pub_key"    , REMOVE_HASH_BRAKETS(fmt::format("{:s}",
                                                    output_data.pubkey))},
                            {"mix_tx_hash"    , REMOVE_HASH_BRAKETS(fmt::format("{:s}",
                                                    tx_out_idx.first))},
                            {"mix_out_indx"   , fmt::format("{:d}", tx_out_idx.second)},
                            {"mix_timestamp"  , xmreg::timestamp_to_str(blk.timestamp)},
                            {"mix_age"        , mixin_age.first},
                            {"mix_mixin_no"   , mixin_txd.mixin_no},
                            {"mix_inputs_no"  , mixin_txd.input_key_imgs.size()},
                            {"mix_outputs_no" , mixin_txd.output_pub_keys.size()},
                            {"mix_age_format" , mixin_age.second},
                            {"mix_idx"        , fmt::format("{:02d}", count)},
                    });

                    // get mixin timestamp from its orginal block
                    mixin_timestamps.push_back(blk.timestamp);

                    ++count;

                } // for (const uint64_t &i: absolute_offsets)

                // get mixins in time scale for visual representation
                pair<string, double> mixin_times_scale = xmreg::timestamps_time_scale(
                                                            mixin_timestamps,
                                                            server_timestamp, 170);

                // save resolution of mixin timescales
                timescale_scale = mixin_times_scale.second;

                // save the string timescales for later to show
                mixins_timescales.push_back(mstch::map {
                    {"timescale", mixin_times_scale.first}});

                input_idx++;
            } // for (const txin_to_key& in_key: txd.input_key_imgs)

            context["server_time"]      = server_time_str;
            context["inputs"]           = inputs;
            context["timescales"]       = mixins_timescales;
            context["timescales_scale"] = fmt::format("{:0.2f}",
                                                timescale_scale / 3600.0 / 24.0); // in days

            // get indices of outputs in amounts tables
            vector<uint64_t> out_amount_indices;

            try
            {

                uint64_t tx_index;

                if (core_storage->get_db().tx_exists(txd.hash, tx_index))
                {
                    out_amount_indices = core_storage->get_db()
                            .get_tx_amount_output_indices(tx_index);
                }
                else
                {
                    cerr << "get_tx_outputs_gindexs failed to find transaction with id = " << txd.hash;
                }

            }
            catch(const exception& e)
            {
                cerr << e.what() << endl;
            }

            uint64_t output_idx {0};

            mstch::array outputs;

            for (pair<txout_to_key, uint64_t>& outp: txd.output_pub_keys)
            {

                // total number of ouputs in the blockchain for this amount
                uint64_t num_outputs_amount = core_storage->get_db()
                                                .get_num_outputs(outp.second);

                string out_amount_index_str {"N/A"};

                // outputs in tx in them mempool dont have yet global indices
                // thus for them, we print N/A
                if (!out_amount_indices.empty())
                {
                    out_amount_index_str = fmt::format("{:d}",
                                            out_amount_indices.at(output_idx));
                }

                outputs.push_back(mstch::map {
                      {"out_pub_key"   , REMOVE_HASH_BRAKETS(fmt::format("{:s}", outp.first.key))},
                      {"amount"        , fmt::format("{:0.12f}", XMR_AMOUNT(outp.second))},
                      {"amount_idx"    , out_amount_index_str},
                      {"num_outputs"   , fmt::format("{:d}", num_outputs_amount)},
                      {"output_idx"    , fmt::format("{:02d}", output_idx++)}
                });
            }

            context["outputs"] = outputs;

            // read tx.html
            string tx_html = xmreg::read(TMPL_TX);

            // add header and footer
            string full_page = get_full_page(tx_html);

            // render the page
            return mstch::render(full_page, context);
        }


        string
        get_search_status(string uuid)
        {
            uint64_t block_id    = searching_threads[uuid]->block_id;
            string timestamp_str =  searching_threads[uuid]->timestamp_str;
            //cout << uuid << ": " << searching_threads[uuid]->block_id << endl;

            stringstream ss;

            ss << block_id << ": " << timestamp_str
               << " found " <<  searching_threads[uuid]->outputs.size()
               << " outputs";

            return ss.str();
        };

        string
        fire_finish_search( string uuid)
        {
            searching_threads[uuid]->user_left = true;
            searching_threads.erase(uuid);
            cout <<  "User left" << endl;
            return {};
        };


        string
        show_my_outputs(string xmr_address_str,
                        string viewkey_str,
                        string uuid,
                        uint64_t since_when = 1)
        {


            // remove white characters
            boost::trim(xmr_address_str);
            boost::trim(viewkey_str);

            if (xmr_address_str.empty())
            {
                return string("Monero address not provided!");
            }

            if (viewkey_str.empty())
            {
                return string("Viewkey not provided!");
            }

            // parse string representing given monero address
            cryptonote::account_public_address address;

            if (!xmreg::parse_str_address(xmr_address_str,  address, 0))
            {
                cerr << "Cant parse string address: " << xmr_address_str << endl;
                return string("Cant parse xmr address: " + xmr_address_str);
            }

            // parse string representing given private viewkey
            crypto::secret_key prv_view_key;

            if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
            {
                cerr << "Cant parse view key: " << viewkey_str << endl;
                return string("Cant parse view key: " + viewkey_str);
            }

            // get the current blockchain height.
            uint64_t height =
                    xmreg::MyLMDB::get_blockchain_height(
                            mcore->get_blkchain_path()) - 1;

            uint64_t out_idx = {0};

            mstch::map context {
                {"xmr_address"  , xmr_address_str},
                {"viewkey"      , viewkey_str},
                {"uuid"         , uuid}
            };

            mstch::array outputs;

            //std::thread t1 {*searching_threads[uuid]};
            std::thread t1 {&search_class_test::search, searching_threads[uuid].get()};
            t1.detach();


            // xmreg::MyLMDB mylmdb {lmdb2_path};

            // for (uint64_t i = tx_blk_height; i <= height; ++i)
            // {
            //     // get block at the given height i
            //     block blk;

            //     if (!mcore->get_block_by_height(i, blk))
            //     {
            //         cerr << "Cant get block: " << i << endl;
            //         continue;
            //     }

            //     vector<output_info> outputs_info;

            //     mylmdb.get_output_info(blk.timestamp, outputs_info);

            //     //cout << "blk: << " << i << " output no.: " << outputs_info.size() << endl;

            //     crypto::hash previous_tx_hash {null_hash};

            //     // go through all outputs in each block, based on timestamp,
            //     // and search for our outputs
            //     for (const xmreg::output_info out_info : outputs_info)
            //     {
            //         // public transaction key is combined with our viewkey
            //         // to create, so called, derived key.
            //         crypto::key_derivation derivation;

            //         bool r = generate_key_derivation(out_info.tx_pub_key,
            //                                          prv_view_key,
            //                                          derivation);

            //         if (!r)
            //         {
            //             cerr << "cant derive key for output: "
            //                  << out_info.out_pub_key << endl;
            //             continue;
            //         }

            //         // get the tx output public key
            //         // that normally would be generated for us,
            //         // if someone had sent us some xmr.
            //         crypto::public_key generated_pubkey;

            //         derive_public_key(derivation,
            //                           out_info.index_in_tx,
            //                           address.m_spend_public_key,
            //                           generated_pubkey);

            //         bool same_tx {false};

            //         if (out_info.out_pub_key == generated_pubkey)
            //         {

            //             sum_xmr += out_info.amount;

            //             string timestamp_str = xmreg::timestamp_to_str(blk.timestamp);


            //             same_tx = (previous_tx_hash == out_info.tx_hash);

            //             string out_pub_key_str = REMOVE_HASH_BRAKETS(fmt::format("{:s}",
            //                                                          out_info.out_pub_key));

            //             string tx_hash_str      = REMOVE_HASH_BRAKETS(fmt::format("{:s}",
            //                                                          out_info.tx_hash));

            //             outputs.push_back(mstch::map {
            //                     {"out_pub_key"  , out_pub_key_str},
            //                     {"amount"       , fmt::format("{:0.12f}", XMR_AMOUNT(out_info.amount))},
            //                     {"output_idx"   , fmt::format("{:04d}", ++out_idx)},
            //                     {"tx_hash"      , tx_hash_str},
            //                     {"blk_timestamp", timestamp_str},
            //                     {"same_tx"      , !same_tx}
            //             });

            //             previous_tx_hash = out_info.tx_hash;

            //         }
            //     } // for (const xmreg::output_info out_info : outputs_info)
            // } // for (uint64_t i = tx_blk_height;

            // context["outputs_no"] = outputs.size();
            // context["outputs"]    = outputs;
            // context["sum_xmr"]    = fmt::format("{:0.12f}", XMR_AMOUNT(sum_xmr));

            // read my_outputs.html
            string my_outputs_html = xmreg::read(TMPL_MY_OUTPUTS);

            // add header and footer
            string full_page = get_full_page(my_outputs_html);

            // render the page
            return mstch::render(full_page, context);
        }

        string
        search(string search_text)
        {

            // remove white characters
            boost::trim(search_text);

            string default_txt {"No such thing found: " + search_text};

            string result_html {default_txt};

            // check first if we look for output with given global index
            // such search start with "goi_", e.g., "goi_543"
            bool search_for_global_output_idx = (search_text.substr(0, 4) == "goi_");

            // check if we look for output with amout index and amount
            // such search start with "aoi_", e.g., "aoi_444-23.00"
            bool search_for_amount_output_idx = (search_text.substr(0, 4) == "aoi_");

            // first check if searching for block of given height
            if (search_text.size() < 12 &&
                                (search_for_global_output_idx == false
                                 ||search_for_amount_output_idx == false))
            {
                uint64_t blk_height;

                try
                {
                    blk_height = boost::lexical_cast<uint64_t>(search_text);

                    result_html = show_block(blk_height);

                    // nasty check if output is "Cant get" as a sign of
                    // a not found tx. Later need to think of something better.
                    if (result_html.find("Cant get") == string::npos)
                    {
                         return result_html;
                    }

                }
                catch(boost::bad_lexical_cast &e)
                {
                    return result_html;
                }

            }

            // second let try searching for tx
            result_html = show_tx(search_text);

            // nasty check if output is "Cant get" as a sign of
            // a not found tx. Later need to think of something better.
            if (result_html.find("Cant get") == string::npos)
            {
                 return result_html;
            }

            // if tx search not successful, check if we are looking
            // for a block with given hash
            result_html = show_block(search_text);

            if (result_html.find("Cant get") == string::npos)
            {
                 return result_html;
            }

            result_html = default_txt;

            // get mempool transaction so that what we search,
            // might be there. Note: show_tx above already searches it
            // but only looks for tx hash. Now want to check
            // for key_images, public_keys, payments_id, etc.
            vector<transaction> mempool_txs = get_mempool_txs();

            // key is string indicating where search_text was found.
            map<string, vector<string>> tx_search_results
                                = search_txs(mempool_txs, search_text);

            // now search my own custom lmdb database
            // with key_images, public_keys, payments_id etc.

            vector<pair<string, vector<string>>> all_possible_tx_hashes;




            try
            {
                unique_ptr<xmreg::MyLMDB> mylmdb;

                if (!bf::is_directory(lmdb2_path))
                {
                    throw std::runtime_error(lmdb2_path + " does not exist");
                }

                cout << "Custom lmdb database seem to exist at: " << lmdb2_path << endl;
                cout << "So lets try to search there for what we are after." << endl;

                mylmdb = make_unique<xmreg::MyLMDB>(lmdb2_path);


                mylmdb->search(search_text,
                               tx_search_results["key_images"],
                               "key_images");

                cout << "size: " << tx_search_results["key_images"].size() << endl;

                all_possible_tx_hashes.push_back(
                        make_pair("key_images",
                                  tx_search_results["key_images"]));


                // search the custum lmdb for tx_public_keys and append the result
                // to those from the mempool search if found

                mylmdb->search(search_text,
                               tx_search_results["tx_public_keys"],
                               "tx_public_keys");

                all_possible_tx_hashes.push_back(
                        make_pair("tx_public_keys",
                                  tx_search_results["tx_public_keys"]));

                // search the custum lmdb for payments_id and append the result
                // to those from the mempool search if found

                mylmdb->search(search_text,
                               tx_search_results["payments_id"],
                               "payments_id");

                all_possible_tx_hashes.push_back(
                        make_pair("payments_id",
                                  tx_search_results["payments_id"]));

                // search the custum lmdb for encrypted_payments_id and append the result
                // to those from the mempool search if found

                mylmdb->search(search_text,
                               tx_search_results["encrypted_payments_id"],
                               "encrypted_payments_id");

                all_possible_tx_hashes.push_back(
                        make_pair("encrypted_payments_id",
                                  tx_search_results["encrypted_payments_id"]));

                // search the custum lmdb for output_public_keys and append the result
                // to those from the mempool search if found

                mylmdb->search(search_text,
                               tx_search_results["output_public_keys"],
                               "output_public_keys");

                all_possible_tx_hashes.push_back(
                        make_pair("output_public_keys",
                                  tx_search_results["output_public_keys"]));


                // seach for output using output global index

                if (search_for_global_output_idx)
                {
                    try
                    {
                        uint64_t global_idx = boost::lexical_cast<uint64_t>(
                                search_text.substr(4));


                        //cout << "global_idx: " << global_idx << endl;

                        // get info about output of a given global index
                        output_data_t output_data = core_storage->get_db()
                                .get_output_key(global_idx);

                        //tx_out_index tx_out = core_storage->get_db()
                        //                    .get_output_tx_and_index_from_global(global_idx);

                        //cout << "tx_out.first: " << tx_out.first << endl;
                        //cout << "tx_out.second: " << tx_out.second << endl;

                        string output_pub_key = pod_to_hex(output_data.pubkey);

                        //cout << "output_pub_key: " << output_pub_key << endl;

                        vector<string> found_outputs;

                        mylmdb->search(output_pub_key,
                                       found_outputs,
                                       "output_public_keys");

                        //cout << "found_outputs.size(): " << found_outputs.size() << endl;

                        all_possible_tx_hashes.push_back(
                                make_pair("output_public_keys_based_on_global_idx",
                                          found_outputs));

                    }
                    catch(boost::bad_lexical_cast &e)
                    {
                        cerr << "Cant cast global_idx string: "
                        << search_text.substr(4) << endl;
                    }
                } //  if (search_for_global_output_idx)

                // seach for output using output amount index and amount

                if (search_for_amount_output_idx)
                {
                    try
                    {

                        string str_to_split = search_text.substr(4);

                        vector<string> string_parts;

                        boost::split(string_parts, str_to_split,
                                     boost::is_any_of("-"));

                        if (string_parts.size() != 2)
                        {
                            throw;
                        }

                        uint64_t amount_idx = boost::lexical_cast<uint64_t>(
                                string_parts[0]);

                        uint64_t amount = static_cast<uint64_t>
                        (boost::lexical_cast<double>(
                                        string_parts[1]) * 1e12);


                        //cout << "amount_idx: " << amount_idx << endl;
                        //cout << "amount: "     << amount << endl;

                        // get info about output of a given global index
                        output_data_t output_data = core_storage->get_db()
                                .get_output_key(
                                        amount, amount_idx);

                        string output_pub_key = pod_to_hex(output_data.pubkey);

                        //cout << "output_pub_key: " << output_pub_key << endl;

                        vector<string> found_outputs;

                        mylmdb->search(output_pub_key,
                                       found_outputs,
                                       "output_public_keys");

                        //cout << "found_outputs.size(): " << found_outputs.size() << endl;

                        all_possible_tx_hashes.push_back(
                                make_pair("output_public_keys_based_on_amount_idx",
                                          found_outputs));

                    }
                    catch(boost::bad_lexical_cast& e)
                    {
                        cerr << "Cant parse amout index and amout string: "
                             << search_text.substr(4) << endl;
                    }
                    catch(OUTPUT_DNE& e)
                    {
                        cerr << "Output not found in the blockchain: "
                             << search_text.substr(4) << endl;

                        return(string("Output not found in the blockchain: ")
                               + search_text.substr(4));
                    }
                } // if (search_for_amount_output_idx)
            }
            catch (const lmdb::runtime_error& e)
            {
                cerr << "Error opening/accessing custom lmdb database: "
                     << e.what() << endl;
            }
            catch (...)
            {
                std::exception_ptr p = std::current_exception();
                cerr << "Error opening/accessing custom lmdb database: "
                     << p.__cxa_exception_type()->name() << endl;
            }



            result_html = show_search_results(search_text, all_possible_tx_hashes);

            return result_html;
        }


        map<string, vector<string>>
        search_txs(vector<transaction> txs, const string& search_text)
        {
            map<string, vector<string>> tx_hashes;

            // initlizte the map with empty results
            tx_hashes["key_images"]                             = {};
            tx_hashes["tx_public_keys"]                         = {};
            tx_hashes["payments_id"]                            = {};
            tx_hashes["encrypted_payments_id"]                  = {};
            tx_hashes["output_public_keys"]                     = {};

            for (const transaction& tx: txs)
            {

                tx_details txd = get_tx_details(tx);

                string tx_hash_str = pod_to_hex(txd.hash);

                // check if any key_image matches the search_text

                vector<txin_to_key>::iterator it1 =
                    find_if(begin(txd.input_key_imgs), end(txd.input_key_imgs),
                        [&](const txin_to_key& key_img)
                        {
                            return pod_to_hex(key_img.k_image) == search_text;
                        });

                if (it1 != txd.input_key_imgs.end())
                {
                    tx_hashes["key_images"].push_back(tx_hash_str);
                }

                // check if  tx_public_key matches the search_text

                if (pod_to_hex(txd.pk) == search_text)
                {
                    tx_hashes["tx_public_keys"].push_back(tx_hash_str);
                }

                // check if  payments_id matches the search_text

                if (pod_to_hex(txd.payment_id) == search_text)
                {
                    tx_hashes["payment_id"].push_back(tx_hash_str);
                }

                // check if  encrypted_payments_id matches the search_text

                if (pod_to_hex(txd.payment_id8) == search_text)
                {
                    tx_hashes["encrypted_payments_id"].push_back(tx_hash_str);
                }

                // check if output_public_keys matche the search_text

                vector<pair<txout_to_key, uint64_t>>::iterator it2 =
                    find_if(begin(txd.output_pub_keys), end(txd.output_pub_keys),
                        [&](const pair<txout_to_key, uint64_t>& tx_out_pk)
                        {
                            return pod_to_hex(tx_out_pk.first.key) == search_text;
                        });

                if (it2 != txd.output_pub_keys.end())
                {
                    tx_hashes["output_public_keys"].push_back(tx_hash_str);
                }

            }

            return  tx_hashes;

        }

        vector<transaction>
        get_mempool_txs()
        {
            // get mempool data using rpc call
            vector<pair<tx_info, transaction>> mempool_data = search_mempool();

            // output only transactions
            vector<transaction> mempool_txs;

            mempool_txs.reserve(mempool_data.size());

            for (const auto& a_pair: mempool_data)
            {
                mempool_txs.push_back(a_pair.second);
            }

            return mempool_txs;
        }

        string
        show_search_results(const string& search_text,
            const vector<pair<string, vector<string>>>& all_possible_tx_hashes)
        {

            // initalise page tempate map with basic info about blockchain
            mstch::map context {
                    {"search_text"    , search_text},
                    {"no_results"     , true},
                    {"to_many_results", false}
            };

            for (const pair<string, vector<string>>& found_txs: all_possible_tx_hashes)
            {
                // define flag, e.g., has_key_images denoting that
                // tx hashes for key_image searched were found
                context.insert({"has_" + found_txs.first, !found_txs.second.empty()});

                cout << "found_txs.first: " << found_txs.first << endl;

                // insert new array based on what we found to context if not exist
                pair< mstch::map::iterator, bool> res
                        = context.insert({found_txs.first, mstch::array{}});

                if (!found_txs.second.empty())
                {

                    uint64_t tx_i {0};

                    // for each found tx_hash, get the corresponding tx
                    // and its details, and put into mstch for rendering
                    for (const string& tx_hash: found_txs.second)
                    {

                        crypto::hash tx_hash_pod;

                        epee::string_tools::hex_to_pod(tx_hash, tx_hash_pod);

                        transaction tx;

                        uint64_t blk_height {0};

                        int64_t blk_timestamp;

                        // first check in the blockchain
                        if (mcore->get_tx(tx_hash, tx))
                        {

                            // get timestamp of the tx's block
                            blk_height    = core_storage
                                    ->get_db().get_tx_block_height(tx_hash_pod);

                            blk_timestamp = core_storage
                                    ->get_db().get_block_timestamp(blk_height);

                        }
                        else
                        {
                            // check in mempool if tx_hash not found in the
                            // blockchain
                            vector<pair<tx_info, transaction>> found_txs
                                    = search_mempool(tx_hash_pod);

                            if (!found_txs.empty())
                            {
                                // there should be only one tx found
                                tx = found_txs.at(0).second;
                            }
                            else
                            {
                                return string("Cant get tx of hash (show_search_results): " + tx_hash);
                            }

                            // tx in mempool have no blk_timestamp
                            // but can use their recive time
                             blk_timestamp = found_txs.at(0).first.receive_time;

                        }

                        tx_details txd = get_tx_details(tx);

                        mstch::map txd_map = txd.get_mstch_map();


                        // add the timestamp to tx mstch map
                        txd_map.insert({"timestamp", xmreg::timestamp_to_str(blk_timestamp)});

                        boost::get<mstch::array>((res.first)->second).push_back(txd_map);

                        // dont show more than 500 results
                        if (tx_i > 500)
                        {
                            context["to_many_results"] = true;
                            break;
                        }

                        ++tx_i;
                    }

                    // if found something, set this flag to indicate this fact
                    context["no_results"] = false;
                }
            }

            // read search_results.html
            string search_results_html = xmreg::read(TMPL_SEARCH_RESULTS);

            // add header and footer
            string full_page = get_full_page(search_results_html);

            // read partial for showing details of tx(s) found
            map<string, string> partials {
                {"tx_table_head", xmreg::read(string(TMPL_PARIALS_DIR) + "/tx_table_header.html")},
                {"tx_table_row" , xmreg::read(string(TMPL_PARIALS_DIR) + "/tx_table_row.html")}
            };

            // render the page
            return  mstch::render(full_page, context, partials);
        }



    private:

        tx_details
        get_tx_details(const transaction& tx, bool coinbase = false)
        {
            tx_details txd;

            // get tx hash
            txd.hash = get_transaction_hash(tx);

            // get tx public key
            txd.pk = get_tx_pub_key_from_extra(tx);

            // sum xmr in inputs and ouputs in the given tx
            txd.xmr_inputs  = sum_money_in_inputs(tx);
            txd.xmr_outputs = sum_money_in_outputs(tx);

            // get mixin number
            txd.mixin_no    = get_mixin_no(tx);

            txd.fee = 0;

            if (!coinbase &&  tx.vin.size() > 0)
            {
                // check if not miner tx
                // i.e., for blocks without any user transactions
                if (tx.vin.at(0).type() != typeid(txin_gen))
                {
                    // get tx fee
                    txd.fee = get_tx_fee(tx);
                }

            }

            get_payment_id(tx, txd.payment_id, txd.payment_id8);

            // get tx size in bytes
            txd.size = get_object_blobsize(tx);

            txd.input_key_imgs  = get_key_images(tx);
            txd.output_pub_keys = get_ouputs(tx);

            txd.extra = tx.extra;

            // get tx signatures for each input
            txd.signatures = tx.signatures;

            // get tx version
            txd.version = tx.version;

            // get unlock time
            txd.unlock_time = tx.unlock_time;

            return txd;
        }

        vector<pair<tx_info, transaction>>
        search_mempool(crypto::hash tx_hash = null_hash)
        {

            vector<pair<tx_info, transaction>> found_txs;

            // get txs in the mempool
            std::vector<tx_info> mempool_txs;

            if (!rpc.get_mempool(mempool_txs))
            {
              cerr << "Getting mempool failed " << endl;
              return found_txs;
            }

            // if we have tx blob disply more.
            // this info can also be obtained from json that is
            // normally returned by the RCP call (see below in detailed view)
            if (HAVE_TX_BLOB)
            {
                // get tx_blob if exists
                //string tx_blob = get_tx_blob(_tx_info);

                for (size_t i = 0; i < mempool_txs.size(); ++i)
                {
                    // get transaction info of the tx in the mempool
                    tx_info _tx_info = mempool_txs.at(i);

                    // get tx_blob if exists
                    string tx_blob = get_tx_blob(_tx_info);

                    if (tx_blob.empty())
                    {
                        cerr << "tx_blob is empty. Probably its not a custom deamon." << endl;
                        continue;
                    }

                    // pare tx_blob into tx class
                    transaction tx;

                    if (!parse_and_validate_tx_from_blob(
                            tx_blob, tx))
                    {
                        cerr << "Cant get tx from blob" << endl;
                        continue;
                    }


                    // if we dont provide tx_hash, just get all txs in
                    // the mempool
                    if (tx_hash != null_hash)
                    {
                        // check if tx hash matches, and if yes, save it for return
                        if (tx_hash == get_transaction_hash(tx))
                        {
                            found_txs.push_back(make_pair(_tx_info, tx));
                            break;
                        }
                    }
                    else
                    {
                       found_txs.push_back(make_pair(_tx_info, tx));
                    }

                }
            }
            else
            {
                // @TODO make tx_info from json
                // if dont have tx_blob member, construct tx_info
                // from json obtained from the rpc call
            }

            return found_txs;
        }

        pair<string, string>
        get_age(uint64_t timestamp1, uint64_t timestamp2, bool full_format = 0)
        {

            pair<string, string> age_pair;

            // calculate difference between server and block timestamps
            array<size_t, 5> delta_time = timestamp_difference(
                    timestamp1, timestamp2);

            // default format for age
            string age_str = fmt::format("{:02d}:{:02d}:{:02d}",
                                         delta_time[2], delta_time[3],
                                         delta_time[4]);

            string age_format {"[h:m:s]"};

            // if have days or years, change age format
            if (delta_time[0] > 0 || full_format == true)
            {
                age_str = fmt::format("{:02d}:{:03d}:{:02d}:{:02d}:{:02d}",
                                      delta_time[0], delta_time[1], delta_time[2],
                                      delta_time[3], delta_time[4]);

                age_format = string("[y:d:h:m:s]");
            }
            else if (delta_time[1] > 0)
            {
                age_str = fmt::format("{:02d}:{:02d}:{:02d}:{:02d}",
                                      delta_time[1], delta_time[2],
                                      delta_time[3], delta_time[4]);

                age_format = string("[d:h:m:s]");
            }

            age_pair.first  = age_str;
            age_pair.second = age_format;

            return age_pair;
        }

        pair<uint64_t, uint64_t>
        sum_xmr_outputs(const string& json_str)
        {
            pair<uint64_t, uint64_t> sum_xmr {0, 0};

            rapidjson::Document json;

            if (json.Parse(json_str.c_str()).HasParseError())
            {
                cerr << "Failed to parse JSON" << endl;
                return sum_xmr;
            }

            // get information about outputs
            const rapidjson::Value& vout = json["vout"];

            if (vout.IsArray())
            {

                for (rapidjson::SizeType i = 0; i < vout.Size(); ++i)
                {
                    //print(" - {:s}, {:0.8f} xmr\n",
                    //    vout[i]["target"]["key"].GetString(),
                    //    XMR_AMOUNT(vout[i]["amount"].GetUint64()));

                    sum_xmr.first += vout[i]["amount"].GetUint64();
                }

                  sum_xmr.second = vout.Size();
            }

            return sum_xmr;
        }

        pair<uint64_t, uint64_t>
        sum_xmr_inputs(const string& json_str)
        {
            pair<uint64_t, uint64_t> sum_xmr {0, 0};

            rapidjson::Document json;

            if (json.Parse(json_str.c_str()).HasParseError())
            {
                cerr << "Failed to parse JSON" << endl;
                return sum_xmr;
            }

            // get information about inputs
            const rapidjson::Value& vin = json["vin"];

            if (vin.IsArray())
            {
                // print("Input key images:\n");

                for (rapidjson::SizeType i = 0; i < vin.Size(); ++i)
                {
                    if (vin[i].HasMember("key"))
                    {
                        const rapidjson::Value& key_img = vin[i]["key"];

                        // print(" - {:s}, {:0.8f} xmr\n",
                        //       key_img["k_image"].GetString(),
                        //       XMR_AMOUNT(key_img["amount"].GetUint64()));

                        sum_xmr.first += key_img["amount"].GetUint64();
                    }
                }

                sum_xmr.second = vin.Size();
            }

            return sum_xmr;
        }


        vector<uint64_t>
        get_mixin_no_in_txs(const string& json_str)
        {
            vector<uint64_t> mixin_no;

            rapidjson::Document json;

            if (json.Parse(json_str.c_str()).HasParseError())
            {
                cerr << "Failed to parse JSON" << endl;
                return mixin_no;
            }

            // get information about inputs
            const rapidjson::Value& vin = json["vin"];

            if (vin.IsArray())
            {
               // print("Input key images:\n");

                for (rapidjson::SizeType i = 0; i < vin.Size(); ++i)
                {
                    if (vin[i].HasMember("key"))
                    {
                        const rapidjson::Value& key_img = vin[i]["key"];

                        // print(" - {:s}, {:0.8f} xmr\n",
                        //       key_img["k_image"].GetString(),
                        //       XMR_AMOUNT(key_img["amount"].GetUint64()));


                        mixin_no.push_back(key_img["key_offsets"].Size());
                    }
                }
            }

            return mixin_no;
        }

        string
        get_full_page(string& middle)
        {
            return xmreg::read(TMPL_HEADER)
                   + middle
                   + xmreg::read(TMPL_FOOTER);
        }

    };
}


#endif //CROWXMR_PAGE_H
