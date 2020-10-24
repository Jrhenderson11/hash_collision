#include <map>
#include <set>
#include <iostream>
#include <list>
#include <string>
//#include "cpp-fnv/include/fnv/fnv.h"
//#include "PicoSHA2/picosha2.h"
#include <openssl/sha.h>
//#include "parallel_hashmap/parallel_hashmap/phmap.h"
#include <string>

using std::string;
using std::cout;


#ifdef PARALLEL
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <pthread.h>

#define MAPARGS string, string, \
          phmap::priv::hash_default_hash<string>,\
          phmap::priv::hash_default_eq<string>, \
          phmap::priv::Allocator<std::pair<const string, string>>, \
          4,                 \
          boost::mutex

phmap::parallel_flat_hash_map<MAPARGS> hashed = {};

#define NUM_THREADS 62

pthread_attr_t attr;
void* status;

struct thread_data {
   string input;
};

#else

string GetHexRepresentation(const unsigned char *Bytes, size_t Length) {
    string ret(Length*2, '\0');
    const char *digits = "0123456789abcdef";
    for(size_t i = 0; i < Length; ++i) {
        ret[i*2]   = digits[(Bytes[i]>>4) & 0xf];
        ret[i*2+1] = digits[ Bytes[i]     & 0xf];
    }
    return ret;
}


string sha_hash(string input) {
    
    unsigned char obuf[20];

    SHA1((unsigned char*)input.c_str(), input.size(), obuf);

    return GetHexRepresentation(obuf, 20).substr(0,15);
}


string sha_hash_full(string input) {
    
    unsigned char obuf[20];

    SHA1((unsigned char*)input.c_str(), input.size(), obuf);

    return GetHexRepresentation(obuf, 20);
}

class CustomStr
{
public:
    string s;
    
    CustomStr(string i) {
        this->s = i;
    }

    bool operator==(const CustomStr &o) const
    { 
        return s == o.s;
    }

    bool operator<(const CustomStr &o) const
    {
        if (s.size() == o.s.size()) {
            return s < o.s;
        } else if (s.size() < o.s.size()) {
            return true;
        } else {
            return false;
        }
        
    }

    bool operator>(const CustomStr &o) const
    { 
        if (s.size() == o.s.size()) {
            return s > o.s;
        } else if (s.size() > o.s.size()) {
            return true;
        } else if (s.size() < o.s.size()) {
            return false;
        }
    }

    friend size_t hash_value(const CustomStr &s)
    {
        return std::hash<string>{}(sha_hash(s.s));
    }

};

//phmap::flat_hash_map<CustomStr, string> hashed = {};
std::map<CustomStr, string> hashed = {};

#endif

static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";


// string sha_hash_full(string input) {
//     SHA1 checksum;
//     checksum.update(input);
//     return checksum.final();
// }

#ifdef PARALLEL
void *hash_final(void *threadarg) {
    if (threadarg == NULL) {
     cout << "NULL" << "\n";
    }
    string input = ((struct thread_data *)threadarg)->input;
#else
void hash_final(string input) {

#endif
    CustomStr custom_input = CustomStr(input);
    auto found = hashed.find(custom_input);
    if (found != hashed.end()) {
        cout << "success!" << "\n";
        cout << "INPUT 1: " << input << "\n";
        cout << sha_hash_full(input) << "\n";
        cout << "INPUT 2: " << found->second << "\n";
        cout << sha_hash_full(found->second) << "\n";
        exit(0);
    }
    auto [iterator, success] = hashed.insert({custom_input, input});
    #ifdef PARALLEL
    pthread_exit(NULL);
    #endif
}



#define WINDOW_DIFF_SIZE 100000

long counter = 0;
long high_mark_counter = 0;
string last_mark = "0";
string tmp_last_mark = last_mark;
std::list<string> markers = {};
long upper_bound = 100000000; // 100 MILLION


void inc_and_hash(string base, long depth) {

    if (depth == 0) {
        #ifdef PARALLEL
            pthread_t threads[NUM_THREADS];
            struct thread_data td[NUM_THREADS];

            for (int i =0; i< 62; i++) {
                td[i].input = base + alphanum[i];
                
                int rc = pthread_create(&threads[i], &attr, hash_final, (void *)&td[i]);
                if (rc) {
                    cout << "Error:unable to create thread," << rc << "\n";
                    exit(-1);
                }
            }
            for(int i =0; i< 62; i++) {
                int rc = pthread_join(threads[i], &status);
                if (rc) {
                    cout << "Error:unable to join thread," << rc << "\n";
                    exit(-1);
                }
            }

        #else
            for (int i =0; i< 62; i++) {
               hash_final(base + alphanum[i]);
            }
            counter += 62;
            if (counter - high_mark_counter >= WINDOW_DIFF_SIZE) {
                markers.push_front(base + alphanum[61]);
                high_mark_counter = counter;
                //cout <<"size:" <<  markers.size() << "\n";
            }

            if (counter >= upper_bound) {

                upper_bound += WINDOW_DIFF_SIZE;
                
                // ERASE between last mark and next mark
                
                tmp_last_mark = markers.back();

                hashed.erase(hashed.find(CustomStr(last_mark)), hashed.find(CustomStr(tmp_last_mark)));
                cout << "Counter:" << counter << "\n";
                last_mark = tmp_last_mark;

                markers.pop_back();

                // cout << "Window Shift = " << hashed.size() << "\n";
            }

        #endif
            
    } else {
        for (int i =0; i< 62; i++) {
            inc_and_hash(base + alphanum[i], depth-1);
        }
    }
}

int main()
{
    #ifdef PARALLEL
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    #endif

    string base = "";
    string val;

    long startlen = 0;
    long maxlen = 9;

    for (long length =startlen; length< maxlen; length++) {
        cout << length << "\n";
        inc_and_hash(base, length);
    }
    #ifdef PARALLEL
    pthread_exit(NULL);
    #endif

    return 0;
}