// src/async.cpp
#include "async/async.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <sstream>
#include <stack>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <shared_mutex>
#include <iomanip>

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

struct CommandBlock {
    std::shared_ptr<std::vector<std::string>> commands;
    TimePoint firstCommandTime;
    std::string context_id;
    int block_number;
};

class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;
    virtual void handle(const CommandBlock& block) = 0;
};

class ConsoleOutputHandler : public ICommandHandler {
public:
    void handle(const CommandBlock& block) override {
        if (!block.commands || block.commands->empty()) return;

        std::cout << "bulk: ";
        for (size_t i = 0; i < block.commands->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (*block.commands)[i];
        }
        std::cout << std::endl;
    }
};

class FileOutputHandler : public ICommandHandler {
    int thread_index;
    std::atomic<int> file_counter{0};
    
public:
    explicit FileOutputHandler(int index) : thread_index(index) {}
    
    void handle(const CommandBlock& block) override {
        if (!block.commands || block.commands->empty()) return;

        auto timestamp_sec = std::chrono::duration_cast<std::chrono::seconds>(
            block.firstCommandTime.time_since_epoch()).count();
        auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            block.firstCommandTime.time_since_epoch()).count() % 1000000;
        
        int file_num = file_counter.fetch_add(1, std::memory_order_relaxed) + 1;
        std::ostringstream filename;
        filename << "bulk" 
                << timestamp_sec << "_"
                << std::setfill('0') << std::setw(6) << timestamp_us << "_"
                << block.context_id << "_"
                << block.block_number << "_"
                << thread_index << "_"
                << std::setfill('0') << std::setw(6) << file_num
                << ".log";

        std::ofstream file(filename.str());
        if (!file) {
            std::cerr << "Failed to create file: " << filename.str() << std::endl;
            return;
        }

        file << "bulk: " << (*block.commands)[0];
        for (size_t i = 1; i < block.commands->size(); ++i) {
            file << ", " << (*block.commands)[i];
        }
        file << std::endl;
    }
};

class ThreadManager {
private:
    std::unique_ptr<ConsoleOutputHandler> console_handler;
    std::unique_ptr<FileOutputHandler> file_handler1;
    std::unique_ptr<FileOutputHandler> file_handler2;
    
    std::queue<std::shared_ptr<CommandBlock>> console_queue;
    std::mutex console_mutex;
    std::condition_variable console_cv;
    std::atomic<bool> console_stop{false};
    std::thread console_thread;
    
    std::queue<std::shared_ptr<CommandBlock>> file_queue;
    std::mutex file_mutex;
    std::condition_variable file_cv;
    std::atomic<bool> file_stop{false};
    std::thread file_thread1;
    std::thread file_thread2;
    
    void console_worker() {
        while (true) {
            std::shared_ptr<CommandBlock> block;
            {
                std::unique_lock lock(console_mutex);
                console_cv.wait(lock, [this]() {
                    return !console_queue.empty() || console_stop;
                });
                
                if (console_stop && console_queue.empty()) break;
                
                if (!console_queue.empty()) {
                    block = std::move(console_queue.front());
                    console_queue.pop();
                }
            }
            
            if (block) {
                console_handler->handle(*block);
            }
        }
    }
    
    void file_worker(FileOutputHandler* handler) {
        while (true) {
            std::shared_ptr<CommandBlock> block;
            {
                std::unique_lock lock(file_mutex);
                file_cv.wait(lock, [this]() {
                    return !file_queue.empty() || file_stop;
                });
                
                if (file_stop && file_queue.empty()) break;
                
                if (!file_queue.empty()) {
                    block = std::move(file_queue.front());
                    file_queue.pop();
                }
            }
            
            if (block) {
                handler->handle(*block);
            }
        }
    }
    
public:
    ThreadManager() {
        console_handler = std::make_unique<ConsoleOutputHandler>();
        file_handler1 = std::make_unique<FileOutputHandler>(1);
        file_handler2 = std::make_unique<FileOutputHandler>(2);
        
        console_thread = std::thread(&ThreadManager::console_worker, this);
        file_thread1 = std::thread(&ThreadManager::file_worker, this, file_handler1.get());
        file_thread2 = std::thread(&ThreadManager::file_worker, this, file_handler2.get());
    }
    
    ~ThreadManager() {
        stop();
    }
    
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;
    
    void submitBlock(std::shared_ptr<CommandBlock> block) {
        if (!block) return;
        
        {
            std::lock_guard lock(console_mutex);
            console_queue.push(block);
        }
        console_cv.notify_one();
        
        {
            std::lock_guard lock(file_mutex);
            file_queue.push(block);
        }
        file_cv.notify_all();
    }
    
    void stop() {
        console_stop = true;
        file_stop = true;
        
        console_cv.notify_all();
        file_cv.notify_all();
        
        if (console_thread.joinable()) console_thread.join();
        if (file_thread1.joinable()) file_thread1.join();
        if (file_thread2.joinable()) file_thread2.join();
    }
    
    void waitForCompletion() {
        while (true) {
            size_t console_size, file_size;
            {
                std::scoped_lock lock(console_mutex, file_mutex);
                console_size = console_queue.size();
                file_size = file_queue.size();
            }
            
            if (console_size == 0 && file_size == 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

class AsyncBulkCommandManager {
private:
    size_t blockSize;
    std::vector<std::string> currentCommands;
    std::stack<size_t> dynamicBlockSizes;
    TimePoint firstCommandTime;
    bool inDynamicBlock = false;
    std::string buffer;
    
    inline static std::atomic<int> next_context_id{0};
    const std::string context_id;
    std::atomic<int> block_counter{0};
    
    std::shared_ptr<ThreadManager> thread_manager;
    
    void processBlock() {
        if (currentCommands.empty()) return;
        
        auto block = std::make_shared<CommandBlock>();
        block->commands = std::make_shared<std::vector<std::string>>(std::move(currentCommands));
        block->firstCommandTime = firstCommandTime;
        block->context_id = context_id;
        block->block_number = block_counter.fetch_add(1, std::memory_order_relaxed);
        
        currentCommands.clear();
        firstCommandTime = TimePoint{};
        
        thread_manager->submitBlock(std::move(block));
    }
    
public:
    AsyncBulkCommandManager(size_t size, std::shared_ptr<ThreadManager> tm)
        : blockSize(size),
          context_id(std::to_string(next_context_id.fetch_add(1, std::memory_order_relaxed))),
          thread_manager(std::move(tm)) {}
    
    ~AsyncBulkCommandManager() {
        if (!inDynamicBlock) {
            flush();
        }
    }
    
    AsyncBulkCommandManager(const AsyncBulkCommandManager&) = delete;
    AsyncBulkCommandManager& operator=(const AsyncBulkCommandManager&) = delete;
    
    void addData(const char* data, size_t size) {
        if (!data || size == 0) return;
        
        buffer.append(data, size);
        
        size_t start = 0;
        while (true) {
            size_t end = buffer.find('\n', start);
            if (end == std::string::npos) break;
            
            std::string command(buffer.begin() + start, buffer.begin() + end);
            processCommand(std::move(command));
            start = end + 1;
        }
        
        if (start > 0) {
            buffer.erase(0, start);
        }
    }
    
    void processCommand(std::string command) {
        auto start = command.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return;
        
        auto end = command.find_last_not_of(" \t\r\n");
        command = command.substr(start, end - start + 1);
        
        if (command.empty()) return;
        
        if (command == "{") {
            if (!inDynamicBlock) {
                processBlock();
                dynamicBlockSizes.push(blockSize);
                blockSize = 0;
                inDynamicBlock = true;
            } else {
                dynamicBlockSizes.push(blockSize);
                blockSize = 0;
            }
            return;
        }
        
        if (command == "}") {
            if (inDynamicBlock) {
                processBlock();
                if (!dynamicBlockSizes.empty()) {
                    blockSize = dynamicBlockSizes.top();
                    dynamicBlockSizes.pop();
                }
                inDynamicBlock = !dynamicBlockSizes.empty();
            }
            return;
        }
        
        if (currentCommands.empty()) {
            firstCommandTime = Clock::now();
        }
        
        currentCommands.push_back(std::move(command));
        
        if (!inDynamicBlock && blockSize > 0 && currentCommands.size() >= blockSize) {
            processBlock();
        }
    }
    
    void flush() {
        if (!buffer.empty()) {
            processCommand(std::move(buffer));
            buffer.clear();
        }
        if (!inDynamicBlock && !currentCommands.empty()) {
            processBlock();
        }
    }
};

class GlobalThreadManager {
private:
    std::shared_ptr<ThreadManager> thread_manager;
    std::mutex init_mutex;
    
public:
    static GlobalThreadManager& instance() {
        static GlobalThreadManager instance;
        return instance;
    }
    
    std::shared_ptr<ThreadManager> get() {
        std::lock_guard lock(init_mutex);
        if (!thread_manager) {
            thread_manager = std::make_shared<ThreadManager>();
        }
        return thread_manager;
    }
    
    void shutdown() {
        std::lock_guard lock(init_mutex);
        if (thread_manager) {
            thread_manager->waitForCompletion();
            thread_manager->stop();
            thread_manager.reset();
        }
    }
    
    ~GlobalThreadManager() {
        shutdown();
    }
};

class ContextManager {
private:
    mutable std::shared_mutex mutex;
    std::unordered_map<void*, std::unique_ptr<AsyncBulkCommandManager>> contexts;
    
public:
    static ContextManager& instance() {
        static ContextManager instance;
        return instance;
    }
    
    void* createContext(size_t block_size) {
        std::unique_lock lock(mutex);
        try {
            auto tm = GlobalThreadManager::instance().get();
            auto manager = std::make_unique<AsyncBulkCommandManager>(block_size, tm);
            void* context = manager.get();
            contexts.emplace(context, std::move(manager));
            return context;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create context: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    AsyncBulkCommandManager* getContext(void* context) {
        std::shared_lock lock(mutex);
        auto it = contexts.find(context);
        return it != contexts.end() ? it->second.get() : nullptr;
    }
    
    void destroyContext(void* context) {
        std::unique_lock lock(mutex);
        auto it = contexts.find(context);
        if (it != contexts.end()) {
            it->second->flush();
            contexts.erase(it);
        }
    }
    
    void clearAll() {
        std::unique_lock lock(mutex);
        contexts.clear();
    }
};

// C-интерфейс
ASYNC_API async_handle_t async_connect(std::size_t bulk) {
    return ContextManager::instance().createContext(bulk);
}

ASYNC_API void async_receive(async_handle_t handle, const char* data, std::size_t size) {
    auto manager = ContextManager::instance().getContext(handle);
    if (manager) {
        manager->addData(data, size);
    }
}

ASYNC_API void async_disconnect(async_handle_t handle) {
    ContextManager::instance().destroyContext(handle);
}

// Глобальные деструкторы для корректного завершения
namespace {
    struct GlobalCleanup {
        ~GlobalCleanup() {
            ContextManager::instance().clearAll();
            GlobalThreadManager::instance().shutdown();
        }
    } global_cleanup;
}