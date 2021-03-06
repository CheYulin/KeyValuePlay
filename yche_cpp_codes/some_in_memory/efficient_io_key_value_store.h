//
// Created by cheyulin on 8/20/16.
//

#ifndef KEYVALUESTORE_EFFICIENT_IO_KEY_VALUE_STORE_H
#define KEYVALUESTORE_EFFICIENT_IO_KEY_VALUE_STORE_H

#include <unordered_map>
#include <algorithm>
#include <string>
#include <fstream>

#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

#define INDEX_NAME "index.meta"
#define DB_NAME "value.db"

using namespace std;

inline size_t get_file_size(int file_descriptor) {
    struct stat st;
    fstat(file_descriptor, &st);
    return st.st_size;
}

hash<string> hash_func;

template<typename T, size_t slot_num = 900000>
class yche_map {
private:
    vector<pair<string, T>> hash_table_;
    size_t current_size_{0};
    size_t max_slot_size_{slot_num};

public:
    inline yche_map() : hash_table_(slot_num) {}

    inline T *get(const string &key) {
        auto index = hash_func(key) % max_slot_size_;
        for (; hash_table_[index].first.size() != 0; index = (index + 1) % max_slot_size_) {
            if (hash_table_[index].first == key) {
                return &hash_table_[index].second;
            }
        }
        return nullptr;
    }

    inline void insert_or_replace(const string &key, const T &value) {
        auto index = hash_func(key) % max_slot_size_;
        for (; hash_table_[index].first.size() != 0; index = (index + 1) % max_slot_size_) {
            if (hash_table_[index].first == key) {
                hash_table_[index].second = value;
                return;
            }
        }
        hash_table_[index].first = key;
        hash_table_[index].second = value;
    }
};

class Answer {
private:
    yche_map<pair<int, int>> map_;
    fstream index_stream_;
    fstream val_stream_;
    int db_file_descriptor_;
    char *db_mmap_;
    char *db_read_mmap_;
    int val_index_{0};
    int val_len_{0};
    char *value_buffer;

    bool is_initialized_{false};

    inline void read_index_info() {
        string key_str;
        string prefix_sum_index_str;
        string length_str;
        for (; index_stream_.good();) {
            getline(index_stream_, key_str);
            if (index_stream_.good()) {
                getline(index_stream_, prefix_sum_index_str);
                getline(index_stream_, length_str);
                val_index_ = stoi(prefix_sum_index_str);
                val_len_ = stoi(length_str);
                map_.insert_or_replace(key_str, make_pair(val_index_, val_len_));
            }
        }
        val_index_ = val_index_ + val_len_;
        index_stream_.clear();
    }

public:
    Answer() {
        value_buffer = new char[1024 * 32];
        index_stream_.open(INDEX_NAME, ios::in | ios::out | ios::app | ios::binary);
        db_file_descriptor_ = open(DB_NAME, O_RDWR | O_CREAT, 0600);
        val_stream_.open(DB_NAME, ios::in | ios::binary);
        read_index_info();
    }

    virtual ~Answer() {
        delete[] value_buffer;
    }

    inline string get(string key) {
        if (map_.get(key) == nullptr) {
            return "NULL";
        }
        else {
            auto *index_pair = map_.get(key);
            val_stream_.seekg(index_pair->first, ios::beg);
            val_stream_.read(value_buffer, index_pair->second);
            return string(value_buffer, 0, index_pair->second);
        }
    }

    inline void put(string key, string value) {
        val_len_ = value.size();
        if (!is_initialized_) {
            unsigned long file_size = get_file_size(db_file_descriptor_);
            if (file_size < 10) {
                if (val_len_ < 500) {
                    ftruncate(db_file_descriptor_, 10000000);
                    file_size = 10000000;
                } else if (val_len_ < 5000) {
                    ftruncate(db_file_descriptor_, 1600000000);
                    file_size = 1600000000;
                } else {
                    ftruncate(db_file_descriptor_, 2900000000);
                    file_size = 2900000000;
                }
            }
            db_mmap_ = (char *) mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, db_file_descriptor_, 0);
            db_read_mmap_ = (char *) mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, db_file_descriptor_, 0);
            is_initialized_ = true;
        }
        index_stream_ << key << "\n";
        index_stream_ << val_index_ << "\n";
        index_stream_ << val_len_ << "\n" << flush;

        memcpy(db_mmap_ + val_index_, value.c_str(), val_len_);

        map_.insert_or_replace(key, make_pair(val_index_, val_len_));
        val_index_ += val_len_;
    }
};

#endif //KEYVALUESTORE_EFFICIENT_IO_KEY_VALUE_STORE_H