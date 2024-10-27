#include <unordered_map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cassert>
#include <filesystem>
#include <GL/glew.h>
#include <chrono>
#include <string>
#include <algorithm>
#include <lmdb.h>
#include <classes/Profiler.hpp>

/* *************** ProfilerObject class **************** */

ProfilerObject::ProfilerObject(const char *name) : name(name) {}

void ProfilerObject::StartTracking() {
    if (!isTracking) {
        startTime = std::chrono::high_resolution_clock::now();
        isTracking = true;
    }
}

void ProfilerObject::StopTracking() {
    if (isTracking) {
        data.push_back(std::chrono::high_resolution_clock::now() - startTime);
        isTracking = false;
    }
}

void ProfilerObject::LogData() {
    if (data.empty()) return;

    std::chrono::nanoseconds average{0};
    std::chrono::nanoseconds median, worst, worst5percent, best, best5percent;
    const char *unit;
    double divider;
    char *value;
    const char *sign;
    std::chrono::nanoseconds dbValue;
    double resultValue;

    // Calculate stats
    std::sort(data.begin(), data.end());
    size_t dataSize = data.size();
    median = (dataSize % 2 == 0) ? (data[dataSize / 2 - 1] + data[dataSize / 2]) / 2 : data[dataSize / 2];
    worst = data.back();
    best = data.front();
    worst5percent = data[(size_t)(0.95 * dataSize)];
    best5percent = data[(size_t)(0.05 * dataSize)];
    
    for (const auto& value : data) {
        average += value;
    }
    average /= dataSize;

    // Determine time unit
    if (worst.count() < 1'000) {
        unit = " ns";
        divider = 1;
    } else if (worst.count() < 1'000'000) {
        unit = " μs";
        divider = 1'000;
    } else if (worst.count() < 1'000'000'000) {
        unit = " ms";
        divider = 1'000'000;
    } else {
        unit = " s";
        divider = 1'000'000'000;
    }

    std::cout << std::showpoint << std::setprecision(4) << "---------- " << name << " ----------" << std::endl;

    // Display results with comparison to previous data
    auto printWithComparison = [&](const std::string& label, const std::chrono::nanoseconds& current) {
        if ((value = Profiler::RetrieveData(name, label.c_str()))) {
            dbValue = std::chrono::nanoseconds(atol(value));
            resultValue = (current < dbValue) ? (1 - static_cast<double>(current.count()) / dbValue.count()) * 100
                                              : (static_cast<double>(current.count()) / dbValue.count() - 1) * 100;
            sign = (current < dbValue) ? " -" : " +";
            std::cout << label << ": " << current.count() / divider << unit << sign << resultValue << "%" << std::endl;
        } else {
            std::cout << label << ": " << current.count() / divider << unit << std::endl;
        }
        Profiler::InsertData(name, label.c_str(), std::to_string(current.count()).c_str());
    };

    printWithComparison("Average", average);
    printWithComparison("Median", median);
    printWithComparison("Worst", worst);
    printWithComparison("Best", best);

    if (dataSize >= 40) {
        printWithComparison("Worst5%", worst5percent);
        printWithComparison("Best5%", best5percent);
    }
}

/* *************** Profiler class **************** */

std::unordered_map<const char *, ProfilerObject *> Profiler::data;
bool Profiler::isSaveOn = false;
MDB_env *Profiler::env;
MDB_dbi Profiler::dbi;

void Profiler::StartTracking(const char *name) {
    if (data.find(name) == data.end()) {
        data[name] = new ProfilerObject(name);
    }
    data[name]->StartTracking();
}

void Profiler::StopTracking(const char *name) {
    if (data.find(name) != data.end()) {
        data[name]->StopTracking();
    }
}

void Profiler::LogData() {
    std::ofstream out("Profiler.log");
    std::streambuf *coutbuf = std::cout.rdbuf();
    std::cout.rdbuf(out.rdbuf());
    MDB_txn *txn;

    int rc = mdb_env_create(&env);
    if (rc != 0) assert(!"LMDB env create didn't work");
    rc = mdb_env_open(env, DB_PATH, 0, 0664);
    if (rc != 0) assert(!"LMDB env open didn't work");
    rc = mdb_txn_begin(env, nullptr, 0, &txn);
    if (rc != 0) assert(!"LMDB transaction begin didn't work");
    rc = mdb_dbi_open(txn, nullptr, 0, &dbi);
    if (rc != 0) assert(!"LMDB database open didn't work");
    mdb_txn_abort(txn);

    for (const auto &keyValue : data) {
        keyValue.second->LogData();
    }

    mdb_dbi_close(env, dbi);
    mdb_env_close(env);

    std::cout.rdbuf(coutbuf);
}

void Profiler::InsertData(const char *keyName, const char *keyType, const char *value) {
    if (!isSaveOn) return;

    char key[256];
    snprintf(key, sizeof(key), "%s%s", keyName, keyType);
    MDB_txn *txn;
    int rc = mdb_txn_begin(env, nullptr, 0, &txn);
    if (rc != 0) assert(!"LMDB insert transaction begin didn't work");

    MDB_val mdbKey = { strlen(key), (void *)key };
    MDB_val mdbValue = { strlen(value), (void *)value };

    rc = mdb_put(txn, dbi, &mdbKey, &mdbValue, 0);
    if (rc != 0) {
        mdb_txn_abort(txn);
        assert(!"LMDB put didn't work");
    }

    rc = mdb_txn_commit(txn);
    if (rc != 0) assert(!"LMDB insert transaction commit didn't work");
}

char *Profiler::RetrieveData(const char *keyName, const char *keyType) {
    char key[256];
    snprintf(key, sizeof(key), "%s%s", keyName, keyType);
    MDB_txn *txn;

    int rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
    if (rc != 0) assert(!"LMDB retrieve transaction begin didn't work");

    MDB_val mdbKey = { strlen(key), (void *)key };
    MDB_val mdbValue;

    rc = mdb_get(txn, dbi, &mdbKey, &mdbValue);
    char *ret = nullptr;
    if (rc == 0) {
        ret = strndup((char*)mdbValue.mv_data, mdbValue.mv_size);
    } else if (rc == MDB_NOTFOUND) {
        printf("[%s] Key not found in LMDB\n", key);
    } else {
        fprintf(stderr, "Error retrieving data from LMDB: %s\n", mdb_strerror(rc));
    }

    mdb_txn_abort(txn);
    return ret;
}

void Profiler::SetSaveOn() {
    isSaveOn = true;
}
