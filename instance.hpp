#include "structs/mutator.pb.h"
#include "helpers/client_ws.hpp"

#include <filesystem>
#include <fstream>

namespace pzm {
    enum class EMsgType
    {
        kAuth = 0,
        kInit = 1,
        kCreateInstance = 2,
        kMutate = 3,
    };

    enum class EOption
    {
        kShuffle = 0,
        kBlockAsObject,
        kBlockShuffle,
        kObfuscateRTTI,
        kPartition,
        kSectionRandomization,
        kMinMutationLength,
        kMaxMutationLength,
        kVM,
    };

    using ws_client_t = SimpleWeb::SocketClient<SimpleWeb::WS>;

    ws_client_t client("ws.pzm322.com/mutator/");
    std::shared_ptr<ws_client_t::Connection> m_connection;

    std::mutex main_lock = {};
    std::thread client_thread;

    bool authorized = false;
    uint32_t last_status = 0;

    MUTATOR::MutatorSettings* settings;

    std::function<void(uint32_t session, std::optional<MUTATOR::MapperData> data)> on_session_created = {};
    std::function<void(uint32_t session, std::vector<std::vector<uint8_t>> binaries,
        std::optional<MUTATOR::LaunchData>)> on_mutated = {};

    namespace path {
        std::string binary;
        std::string symbols;
        std::string protected_binary;
    }

    void on_open(const std::shared_ptr<ws_client_t::Connection>& connection) {
        m_connection = connection;

        if (!authorized)
            main_lock.unlock();
    }

    void on_message(const std::shared_ptr<ws_client_t::Connection>& connection,
        const std::shared_ptr<ws_client_t::InMessage>& message) {
        MUTATOR::ServerResponse response;
        if (!response.ParseFromString(message->string())) {
            std::cout << "[error] failed to parse message!" << std::endl;
            return;
        }

        switch (static_cast<EMsgType>(response.type())) {
            case EMsgType::kAuth: {
                authorized = (response.status() == 0);
                main_lock.unlock();
                break;
            }
            case EMsgType::kInit: {
                last_status = response.status();
                main_lock.unlock();
                break;
            }
            case EMsgType::kCreateInstance: {
                on_session_created(response.session_id(), response.mapperdata());
                break;
            }
            case EMsgType::kMutate: {
                std::vector<std::vector<uint8_t>> binaries = {};
                for (const auto& binary : response.binaries())
                    binaries.emplace_back(binary.begin(), binary.end());

                on_mutated(response.session_id(), binaries, response.launchdata());
                break;
            }
            default: {
                printf("unknown packet...\n");
            }
        }
    }

    void setup() {
        settings = new MUTATOR::MutatorSettings();

        main_lock.lock();

        client.on_open = on_open;
        client.on_message = on_message;
    }

    void uninstall() {
        client.stop();
        client_thread.join();
    }

    bool auth(std::string_view token) {
        client.config.header.emplace("auth-token", token);
        client_thread = std::thread([]() -> void {
            client.start();
        });

        main_lock.lock();

        MUTATOR::AuthRequest request;
        request.set_auth_token(token.data());

        m_connection->send(std::to_string((uint8_t)EMsgType::kAuth) + request.SerializeAsString());
        main_lock.lock();

        return authorized;
    }

    void set_directory(std::string_view dir_path) {
        size_t largest_file = 0;
        for (const auto& file : std::filesystem::directory_iterator(dir_path)) {
            if (file.path().extension().string() == ".map") {
                path::symbols = file.path().string();
                continue;
            }

            if (largest_file > 0) {
                if (file.file_size() > largest_file) {
                    path::protected_binary = file.path().string();
                    continue;
                } else {
                    path::protected_binary = path::binary;
                }
            }

            path::binary = file.path().string();
            largest_file = file.file_size();
        }
    }

    template<typename T>
    void set_option(EOption option, T value) {
        switch (option) {
            case EOption::kShuffle:
                settings->set_shuffle(value);
                break;
            case EOption::kPartition:
                settings->set_partition(value);
                break;
            case EOption::kBlockShuffle:
                settings->set_block_shuffle(value);
                break;
            case EOption::kBlockAsObject:
                settings->set_block_as_object(value);
                break;
            case EOption::kObfuscateRTTI:
                settings->set_obfuscate_rtti(value);
                break;
            case EOption::kSectionRandomization:
                settings->set_section_randomization(value);
                break;
            case EOption::kMinMutationLength:
                settings->set_min_mutation_length(value);
                break;
            case EOption::kMaxMutationLength:
                settings->set_max_mutation_length(value);
                break;
            case EOption::kVM:
                settings->set_vm_type(static_cast<MUTATOR::VM>(value));
                break;
        }
    }

    bool initialize() {
        static auto open_file = [](std::string_view path) -> std::string {
            std::ifstream file(path);
            std::stringstream map_stream;
            map_stream << file.rdbuf();
            auto result = map_stream.str();
            file.close();
            return result;
        };

        MUTATOR::InitializationRequest init;
        init.set_allocated_settings(settings);
        init.set_binary(open_file(path::binary));
        init.set_symbols(open_file(path::symbols));

        if (!path::protected_binary.empty())
            init.set_protected_binary(open_file(path::protected_binary));

        m_connection->send(std::to_string((uint8_t)EMsgType::kInit) + init.SerializeAsString());

        main_lock.lock();
        return (last_status == 0);
    }

    uint32_t get_last_status() {
        return last_status;
    }

    void create_instance(uint32_t id, bool mapper) {
        MUTATOR::InstanceRequest instance;
        instance.set_unique_key(id);
        instance.set_mapper(mapper);

        m_connection->send(std::to_string((uint8_t)EMsgType::kCreateInstance) + instance.SerializeAsString());
    }

    void create_mapper(uint32_t id) {
        create_instance(id, true);
    }

    void create_builder(uint32_t id) {
        create_instance(id, false);
    }

    void mutate(uint32_t id, std::optional<MUTATOR::MutatorRequest> data) {
        MUTATOR::MutatorRequest request;
        request.set_unique_key(id);

        if (data) {
            *request.mutable_bases() = data->bases();
            *request.mutable_imports() = data->imports();
        }

        m_connection->send(std::to_string((uint8_t)EMsgType::kMutate) + request.SerializeAsString());
    }
}