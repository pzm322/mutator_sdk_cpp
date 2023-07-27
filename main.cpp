#include "instance.hpp"

void on_session_created(uint32_t session, std::optional<MUTATOR::MapperData> data) {
    // once the session is created, all necessary data must be provided
    // for mapper at least import addresses and allocation bases must be provided
    // for builder nothing is required
    // if you have any callbacks set up, you will receive them after sending the data

    // note: on production, you must use "session" argument to identify which client's session is being handled
    // note 2: it's advised to check if the binary is being mapped or built; if it's being built, you don't need to provide any data

    MUTATOR::MutatorRequest resolved_data;
    resolved_data.add_bases(0x10000000); // example of the base allocated on a client

    // iterate over all imports to provide addresses for them
    for (const auto& imported_module : data->imports()) {
        MUTATOR::ImportedModule current_module;

        // in this example, each imported function has the same address : 0x12345678
        for (const auto& imported_function : imported_module.second.functions())
            current_module.mutable_functions()->insert({ imported_function.first, 0x12345678 });

        resolved_data.mutable_imports()->insert({ imported_module.first, current_module });
    }

    pzm::mutate(session, resolved_data);
}

void on_mutated(uint32_t session_id, const std::vector<std::vector<uint8_t>>& binaries,
                std::optional<MUTATOR::LaunchData>) {
    std::cout << "Session #" << session_id << " has been mutated. Binaries: " << binaries.size() << std::endl;

    // once the binary is mutated, you can send it to the client
    // in this example, we will save binary to a file

    if (binaries.empty())
        return;

    std::ofstream bin_file("test.bin", std::ios::binary);
    bin_file.write((const char*)binaries.at(0).data(), (long)binaries.at(0).size());
    bin_file.close();
}

namespace callbacks {
    void on_export_init(void* data) {
        auto export_info = (MUTATOR::ExportCallback*)data;

        // in this example, each export in the binary is a pointer (and the binary is x86), so the size is always 4
        // each pointer will be equal to 0x13371337 after initialization, and will be mapped recursively

        std::vector<uint8_t> pointer_value = {};
        pointer_value.resize(4);
        *reinterpret_cast<uint32_t*>(pointer_value.data()) = 0x13371337;

        export_info->set_size(pointer_value.size());
        export_info->set_value(std::string(pointer_value.begin(), pointer_value.end()));
        export_info->set_is_const(true);
    }

    void on_export_map(void* data) {
        auto export_info = (MUTATOR::ExportCallback*)data;

        // in this example, the value of each export will be increased by 0x10000000 when generating a unique mutation

        std::vector<uint8_t> updated_value = {};
        updated_value.resize(4);
        memcpy(updated_value.data(), export_info->value().data(), updated_value.size());
        *reinterpret_cast<uint32_t*>(updated_value.data()) += 0x10000000;

        export_info->set_value(std::string(updated_value.begin(), updated_value.end()));
    }

    void on_expiration(void* data) {
        printf("subscription is expiring in %llu minutes!!!\n", ((MUTATOR::ExpireCallback*)data)->time_left() / 60);

        // it's advised to send a message to the social media or messenger once the callback is received
        // thus, there would be enough time to renew the subscription
        // first expiration callback arrives 24 hours before the subscription expires
        // the last expiration callback arrives 3 minutes before the subscription expires
    }
}

int main() {
    std::cout << "Example usage of Mutator SDK" << std::endl;

    std::cout << "Initializing the connection..." << std::endl;

    // initialize the connection and environment
    pzm::setup();

    // setup callbacks
    pzm::on_session_created = on_session_created;
    pzm::on_mutated = on_mutated;

    // a token issued on the profile page on the website,
    // the account which this token has been issued on must have an active subscription
    std::string auth_token = "sampleToken123";

    // authenticate the token in mutator
    if (!pzm::auth(auth_token))
        throw std::runtime_error("Invalid or inactive token");

    std::cout << "Successfully logged in" << std::endl << std::endl;

    // provide the directory which contains both binary and symbols files
    // optionally, there could be a protected file of the same binary
    pzm::set_directory("sample_directory");

    // setup options for the mutator
    pzm::set_option<bool>(pzm::EOption::kShuffle, true);
    pzm::set_option<bool>(pzm::EOption::kBlockAsObject, true);
    pzm::set_option<bool>(pzm::EOption::kBlockShuffle, true);
    pzm::set_option<uint16_t>(pzm::EOption::kMinMutationLength, 30);
    pzm::set_option<uint16_t>(pzm::EOption::kMaxMutationLength, 50);

    // setup callbacks
    pzm::set_callback(MUTATOR::Callback::CALLBACK_EXPORT_INIT, callbacks::on_export_init);
    pzm::set_callback(MUTATOR::Callback::CALLBACK_EXPORT_MMAP, callbacks::on_export_map);
    pzm::set_callback(MUTATOR::Callback::CALLBACK_SUBSCRIPTION_EXPIRE, callbacks::on_expiration);

    std::cout << "Initializing..." << std::endl;

    // initialize the mutator for the provided binary
    if (!pzm::initialize())
        throw std::runtime_error("Init failed with status " + std::to_string(pzm::get_last_status()));

    std::cout << "A project has been initialized" << std::endl << std::endl;

    // there are two ways of handling a binary in mutator:
    // 1. builder - this approach will build a mutated PE with headers and descriptors
    // 2. mapper - this approach will create a dump of the mutated binary to be mapped only once
    // in this example, we will use the mapper approach

    // create a mapper instance
    // when creating either mapper or builder instance, you need to provide unique id for the session
    // if the ID provided is not unique in your system, mutator could not be able to create a session
    uint32_t unique_id = 1;
    pzm::create_mapper(unique_id);

    std::cout << "A session with unique id #" << unique_id << " has been created" << std::endl;

    // close the connection and stop the threads
    pzm::uninstall();

	return EXIT_SUCCESS;
}