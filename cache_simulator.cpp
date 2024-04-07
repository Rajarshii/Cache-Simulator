#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdint>
#include <vector>
#include <tuple>
#include <bitset>


typedef uint64_t cycles_t;
enum access_e {read,write};

// Class for the Cache
class CACHE {
    protected: 
    uint32_t _size;
    uint32_t _assoc;
    int _blk_size;
    uint32_t _policy;

    int _num_sets;
    int set_bits;
    int block_bits;
    int tag_offset;

    std::ifstream infile;

    int cache_level;

    struct SET
    {
        uint32_t _assoc;    // associativity
        uint32_t next_hipri_idx; // next high-priority index
        bool wb_evt;        // a write-back event is triggered

        // Block
        struct BLOCK
        {
            std::uint32_t _tag;
            bool _valid;
            bool _dirty;
            int _priority;
            //std::uint32_t _data; // Not required

            BLOCK(int& policy_b) {
                _tag = 0;
                _valid = false;
                _dirty = false;
                _priority = policy_b;
                //_data = 0; // Not required   
            }

        };

        std::vector<BLOCK> block;

        // constructor
        SET(uint32_t assoc,int policy_b) {
            _assoc = assoc;
            next_hipri_idx = 0;
            wb_evt = false;
            for(int i=0; i<assoc; ++i) {
                block.push_back(BLOCK(policy_b));
            }
        }

        // function to update invalid block
        void block_add(access_e type,int invalid_index, uint32_t tag) {
            // waait for data to come back - cache miss
            block[invalid_index]._valid = true;
            block[invalid_index]._tag = tag;
            block[invalid_index]._dirty = type;
        }

        // choose which block to evict - replacement policy dependent- for now LRU is directly implemented here
        void block_replace(access_e type,int index, uint32_t up_tag) {
            block[index]._tag = up_tag;
            block[index]._valid = true;
            block[index]._dirty = type;
        }

        // evict a block - - replacement policy dependent- for now LRU is directly implemented here
        int block_evict(access_e type,uint32_t up_tag) {
            // evict here
            int priority = -1;
            int index = -1;
            for(int i=0; i<_assoc; ++i) {
                if(block[i]._priority > priority) {
                    priority = block[i]._priority;
                    index = i;
                }
            }

            wb_evt = block[index]._dirty;
            block[index]._valid = false;
            // replace block
            block_add(type,index,up_tag);
            return index;
        }

        void handle_miss(const access_e type, const int add_at_idx, const uint32_t up_tag) {
            // depending on write / read and write allocate policies some differences
            // can occur here. Take care of them
            //int index = -1;
            //std::cout << "Cache miss handler called...\n" ;

            if(add_at_idx >= 0) {
                block_add(type,add_at_idx,up_tag);
                next_hipri_idx = add_at_idx;
            }
            // evict the block
            else {
                next_hipri_idx = block_evict(type,up_tag);
            }
            //update_priority(index);
        }

        void update_priority() {
            // hipri_index is going to have the highest priority now = 0
            for(int i=0; i<_assoc; ++i) {
                if(i == next_hipri_idx) block[next_hipri_idx]._priority = 0;
                else if(block[i]._priority < _assoc-1) { // If multiple block with pri == assoc - 1, handle that case
                    block[i]._priority += 1;
                }
            }
        }

        void wb_complete() {
            wb_evt = false;
        }
    };

    std::vector<SET> _sets;

    public:

    uint64_t accesses;          // memory accesses
    uint64_t misses;            // # of misses
    uint64_t hits;              // # of hits
    cycles_t num_cycles;        // TODO : unused
    uint64_t write_accesses;    // write accesses
    uint64_t illegal_access;    // illegal (non-write/ non-read) accesses - unexpected unless some issue in trace
    uint32_t miss_penalty;      // Constant miss penalty model
    uint32_t _dirty_wbpenalty;  // Constant WB penalty model

    uint64_t instructions_;     // Total number of instructions executed between memory accesses
    uint64_t _dirty_wb;         // Total number of writebacks

    CACHE(int level) {
        _size = 1 << 16;
        _assoc = 2;
        _blk_size = 1 << 6;
        _policy = 1; // LRU
        _num_sets = 1 << 9;

        accesses = 0;
        misses = 0;
        write_accesses = 0;
        illegal_access = 0;
        instructions_ = 0;
        _dirty_wb = 0;
        hits = 0;
        miss_penalty = 0;
        _dirty_wbpenalty = 0;
    }

    void start_phase(const uint32_t& size, const uint32_t& assoc, const uint32_t& blk_size, const uint32_t& policy,
                            const uint32_t& wb_pen, const uint32_t& miss_pen) 
    {
        std::cout << "Start Phase called...\n" ;
        _size = size;
        _assoc = assoc;
        _blk_size = blk_size;
        _policy = policy;
        _dirty_wbpenalty = wb_pen;
        miss_penalty = miss_pen;

        // compute sets
        _num_sets = _size/(_assoc*_blk_size);
        // create sets
        for(int i=0; i<_num_sets; ++i) {
            _sets.push_back(SET(_assoc,_policy));
        }
        //std::cout << "Created sets...\n" ;

        // Number of bits required for block offset and set
        std::bitset<32> ns_bits(_num_sets-1);
        std::bitset<32> blk_bits(_blk_size-1);

        set_bits = ns_bits.count();
        block_bits = blk_bits.count();
        tag_offset = set_bits + block_bits;

        // check for stuff befor emoving to 
    } 

    void run_phase(const std::string& fname) {
        infile.open(fname);
        std::cout << "Run Phase called...\n" ;

        std::string line;
        while(std::getline (infile, line)) {
            // read the line
            auto [type,address,instructions] = parse_line(line);
            //std::cout << "Read Line - " << type << ' ' << address << ' '  << instructions << '\n';

            // cache operation on processor access
            access(type,address);

            //update ;
            instructions_ += instructions;
        }

    }

    /*void CACHE::cache_stat_collector() {

    }*/

    void end_phase() {
        dump_stats();
    }

    ~CACHE() {
        infile.close();
    }

    void access(access_e type, uint64_t address) {
        auto set_id = (address >> block_bits) & (_num_sets-1);
        auto search_tag = address >> tag_offset;
        auto hit = false;
        uint32_t hit_index = 0;
        uint32_t invalid_index = -1;
        //std::cout << "Cache Access called...\n" ;

        SET *acc_set = &_sets[set_id];
        for(std::size_t it=0; it < acc_set->_assoc; ++it) {

            if(acc_set->block[it]._valid==false) {
                invalid_index = it;
                continue;
            }

            if(acc_set->block[it]._tag!=search_tag) continue;

            hit = true;
            hit_index = it; 

            acc_set->block[it]._dirty |= (type);
            break;
        }
        //std::cout << "Cache hit/ miss handler called...\n" ;

        if(!hit) {
            acc_set->handle_miss(type,invalid_index,search_tag);
        }
        else {
            acc_set->next_hipri_idx = hit_index;
        }

        acc_set->update_priority();

        // Update stats
        CACHE::accesses += 1;
        CACHE::hits += hit;
        CACHE::write_accesses += (type);
        CACHE::misses += !hit;
        CACHE::_dirty_wb += acc_set->wb_evt;
        acc_set->wb_complete();
    }

    // The following prints are from CoffeBeforeArch's blog
    void dump_stats() {
    // Print the cache settings
    std::cout << 'L' << cache_level << " CACHE CONFIGURATION\n";
    std::cout << "       Cache Size (Bytes): " << _size << '\n';
    std::cout << "           Associativity : " << _assoc << '\n';
    std::cout << "       Block Size (Bytes): " << _blk_size << '\n';
    std::cout << "    Miss Penalty (Cycles): " << miss_penalty << '\n';
    std::cout << "Dirty WB Penalty (Cycles): " << _dirty_wbpenalty << '\n';
    std::cout << '\n';

    // Print the access breakdown
    std::cout << "CACHE ACCESS STATS\n";
    std::cout << "TOTAL CACHE ACCESSES: " << accesses << '\n';
    std::cout << "               READS: " << accesses - write_accesses << '\n';
    std::cout << "              WRITES: " << write_accesses << '\n';
    std::cout << '\n';

    // Print the miss-rate breakdown
    std::cout << "CACHE MISS-RATE STATS\n";
    double miss_rate = (double)misses / (double)accesses * 100.0;
    auto hits = accesses - misses - illegal_access;
    std::cout << "      MISS-RATE: " << miss_rate << '\n';
    std::cout << "         MISSES: " << misses << '\n';
    std::cout << "           HITS: " << hits << '\n';
    std::cout << '\n';

    // Print the instruction breakdown
    std::cout << "CACHE IPC STATS\n";
    auto cycles = miss_penalty * misses;
    cycles += _dirty_wbpenalty * _dirty_wb;
    cycles += instructions_;
    double ipc = (double)instructions_ / (double)cycles;
    std::cout << "         OVERALL IPC: " << ipc << '\n';
    std::cout << "  TOTAL INSTRUCTIONS: " << instructions_ << '\n';
    std::cout << "        TOTAL CYCLES: " << cycles << '\n';
    std::cout << "            DIRTY WB: " << _dirty_wb << '\n';
  }

  // Get memory access from the trace file
  std::tuple<access_e, uint64_t, int> parse_line(std::string access) {
    // What we want to parse
    int type;
    uint64_t address;
    int instructions;

    // Parse from the string we read from the file
    sscanf(access.c_str(), "# %d %lx %d", &type, &address, &instructions);

    return {access_e(type), address, instructions};
  }
};


int main(int argc, char* argv[]) {
    std::cout << "Cache Sim:\n" ;
    assert(argc >= 7);
    /*if(argc!=2) {
        std::cout << "improper usage\n" ;
        std::cout << "expectd usage: ... \n"; 
        //return SYSERR
    }*/

    //std::cout << "Init Parameters:\n" ;

    std::string trace_file(argv[1]); // Get the trace file
    //std::cout << "Init Parameters2:\n" ;

    uint32_t cache_size(atoi(argv[2]));     // Cache Size
    uint32_t cache_assoc(atoi(argv[3]));    // Associativity
    uint32_t blk_size(atoi(argv[4]));       // Block Size
    uint32_t wb_pen(atoi(argv[5]));
    uint32_t miss_pen(atoi(argv[6]));
    //std::cout << "Init Parameters3:\n" ;

    //uint32_t num_proc(atoi(argv[7]));       // TODO: Number of processors - here we only account for 1
    //uint32_t replacement_policy(atoi(argv[8])); // TODO: Coherence Protocol

    //uint32_t protocol(atoi(argv[9])); // TODO: Coherence Protocol

    // TODO: For now using only one instance of cache and only one level(L1) 
    CACHE cache_inst(1); // L1 cache

    cache_inst.start_phase(cache_size,cache_assoc,blk_size,0,wb_pen,miss_pen);
    cache_inst.run_phase(trace_file);
    cache_inst.end_phase();


    return 0;
}