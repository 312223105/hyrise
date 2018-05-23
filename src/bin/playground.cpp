#include <chrono>
#include <iostream>
#include <unordered_map>
#include <random>
#include <optional>
#include <thread>
#include <uWS/uWS.h>

#include "json.hpp"

#include "queries.cpp"

#include "sql/gds_cache.hpp"
#include "sql/gdfs_cache.hpp"
#include "sql/lru_cache.hpp"
#include "sql/lru_k_cache.hpp"
#include "sql/random_cache.hpp"
#include "sql/sql_query_cache.hpp"
#include "sql/sql_query_plan.hpp"
#include "sql/sql_translator.hpp"
#include "utils/thread_pool.h"


using namespace std::chrono_literals;
using hsql::SQLStatement;
using hsql::PrepareStatement;
using hsql::ExecuteStatement;
using hsql::kStmtPrepare;
using hsql::kStmtExecute;
using hsql::SQLParser;
using hsql::SQLParserResult;

void evaluate_query(uWS::WebSocket<uWS::SERVER> *ws, Query& query, std::map<std::string, std::shared_ptr<opossum::SQLQueryCache<std::string>>>& caches, size_t execution, size_t query_id) {
    return;
}

using CacheKeyType = std::string;
using CacheValueType = std::string;

int main() {
    size_t current_cache_size = 30;
    size_t lru_k_value = 2;
    size_t execution_id = 1;

    std::map<std::string, std::shared_ptr<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>> caches;
    caches.emplace("GDS", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["GDS"]->replace_cache_impl<opossum::GDSCache<CacheKeyType, CacheValueType>>(current_cache_size);

    caches.emplace("GDFS", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["GDFS"]->replace_cache_impl<opossum::GDFSCache<CacheKeyType, CacheValueType>>(current_cache_size);

    caches.emplace("LRU", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU"]->replace_cache_impl<opossum::LRUCache<CacheKeyType, CacheValueType>>(current_cache_size);

    caches.emplace("LRU_2", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_2"]->replace_cache_impl<opossum::LRUKCache<2, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_3", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_3"]->replace_cache_impl<opossum::LRUKCache<3, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_4", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_4"]->replace_cache_impl<opossum::LRUKCache<4, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_5", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_5"]->replace_cache_impl<opossum::LRUKCache<5, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_6", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_6"]->replace_cache_impl<opossum::LRUKCache<6, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_7", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_7"]->replace_cache_impl<opossum::LRUKCache<7, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_8", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_8"]->replace_cache_impl<opossum::LRUKCache<8, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_9", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_9"]->replace_cache_impl<opossum::LRUKCache<9, CacheKeyType, CacheValueType>>(current_cache_size);
    caches.emplace("LRU_10", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["LRU_10"]->replace_cache_impl<opossum::LRUKCache<10, CacheKeyType, CacheValueType>>(current_cache_size);

    caches.emplace("RANDOM", std::make_shared<opossum::SQLQueryCache<CacheValueType, CacheKeyType>>(current_cache_size));
    caches["RANDOM"]->replace_cache_impl<opossum::RandomCache<CacheKeyType, CacheValueType>>(current_cache_size);

    auto workloads = initialize_workloads();

    uWS::Hub h;

    h.onConnection([&workloads, &execution_id](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        std::cout << "Connected!" << std::endl;

        execution_id = 1;

        nlohmann::json initial_data;
        initial_data["message"]  = "startup";
        initial_data["data"]     =  {};

        nlohmann::json tpch;
        tpch["name"] = "TPC-H";
        tpch["id"] = "tpch";
        tpch["queryCount"] = workloads["tpch"].size();

        nlohmann::json tpcc;
        tpcc["name"] = "TPC-C";
        tpcc["id"] = "tpcc";
        tpcc["queryCount"] = workloads["tpcc"].size();

        nlohmann::json join_order;
        join_order["name"] = "Join Order Benchmark";
        join_order["id"] = "join_order";
        join_order["queryCount"] = workloads["join_order"].size();

        initial_data["data"]["workloads"] = {tpch, tpcc, join_order};

        auto initial_data_dump = initial_data.dump();
        ws->send(initial_data_dump.c_str());

    });

    h.onMessage([&caches, &workloads, &execution_id, &lru_k_value, &current_cache_size](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        auto message_json = nlohmann::json::parse(std::string(message, length));
        if (message_json["message"] == "execute_query") {
            std::string workload_id = message_json["data"]["workload"];
            size_t query_id = message_json["data"]["query"];
            std::string query_key = workload_id + std::string("__") + std::to_string(query_id);

            // std::cout << "Execute query: " << query_id << " from workload: " << workload_id << std::endl;

            auto& query = workloads[workload_id][query_id];

            nlohmann::json results;

            results["executionId"] = execution_id++;
            results["workload"] = workload_id;
            results["query"] = query_id;
            results["cacheHits"] = {};
            results["planningTimePostgres"] = {};
            results["planningTimeHyrise"] = {};
            results["planningTimeMysql"] = {};
            results["cacheContents"] = {};

            for (auto &[strategy, cache] : caches) {
                std::optional<CacheValueType> cached_plan = cache->try_get(query_key);
                std::optional<CacheKeyType> evicted;
                bool hit;
                float postgres_planning_time, hyrise_planning_time, mysql_planning_time;
                if (cached_plan) {
                    // std::cout << "Cache Hit: " << query_key << std::endl;
                    hit = true;
                    postgres_planning_time = 0.0f;
                    hyrise_planning_time = 0.0f;
                    mysql_planning_time = 0.0f;
                } else {
                    // std::cout << "Cache Miss: " << query_key << std::endl;
                    hit = false;
                    postgres_planning_time = query.postgres_planning_time;
                    hyrise_planning_time = query.hyrise_planning_time;
                    mysql_planning_time = query.mysql_planning_time;
                    evicted = cache->set(query_key, query.sql_string, query.postgres_planning_time, query.num_tokens);
                }
                if (strategy.find("LRU_") == std::string::npos || strategy.find(std::to_string(lru_k_value)) != std::string::npos) {
                    auto current_strategy = strategy;
                    if (strategy.find("LRU_") != std::string::npos) {
                        current_strategy = "LRU_K";
                    }

                    results["cacheHits"][current_strategy] = hit;
                    results["planningTimePostgres"][current_strategy] = postgres_planning_time;
                    results["planningTimeHyrise"][current_strategy] = hyrise_planning_time;
                    results["planningTimeMysql"][current_strategy] = mysql_planning_time;
                    results["evictedQuery"][current_strategy] = evicted ? *evicted : "-1";

                    auto cache_content = cache->dump_cache();
                    if (cache_content.size() > 0) {
                        results["cacheContents"][current_strategy] = cache_content;
                    }
                }
            }
            nlohmann::json package;
            package["message"] = "query_execution";
            package["data"] = results;


            auto package_dump = package.dump();
            ws->send(package_dump.c_str());
        } else if (message_json["message"] == "update_config") {
            size_t cache_size = message_json["data"]["cacheSize"];

            if (cache_size != current_cache_size) {
                current_cache_size = cache_size;
                for (auto &[strategy, cache] : caches) {
                    cache->resize(cache_size);
                }
                std::cout << "Cache size set to " << cache_size << std::endl;
            }

            size_t new_k_value = message_json["data"]["lruKValue"];
            if (new_k_value != lru_k_value) {
                lru_k_value = new_k_value;

                std::cout << "LRU's K value set to: " << message_json["data"]["lruKValue"] << std::endl;
            }
        } else if (message_json["message"] == "stop_benchmark") {

            for (auto &[strategy, cache] : caches) {
                cache->clear();
            }

            execution_id = 1;

            std::cout << "Benchmark stopped. Caches are cleared." << std::endl;
        }

    });

    if (h.listen(4000)) {
        h.run();
    }

    std::cout << "out" << std::endl;
}
