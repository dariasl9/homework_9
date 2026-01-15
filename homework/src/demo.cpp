#include "async/async.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== Async Library Demo ===\n" << std::endl;
    
    std::cout << "Test 1: Basic scenario (block size = 5)" << std::endl;
    {
        std::size_t bulk = 5;
        auto h = async::connect(bulk);
        auto h2 = async::connect(bulk);
        
        async::receive(h, "1", 1);
        async::receive(h2, "1\n", 2);
        async::receive(h, "\n2\n3\n4\n5\n6\n{\na\n", 15);
        async::receive(h, "b\nc\nd\n}\n89\n", 11);
        
        async::disconnect(h);
        async::disconnect(h2);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "\nTest 2: Dynamic blocks (block size = 3)" << std::endl;
    {
        std::size_t bulk = 3;
        auto h = async::connect(bulk);
        
        async::receive(h, "cmd1\ncmd2\n{\ndyn1\ndyn2\ndyn3\n}\ncmd3\n", 30);
        
        async::disconnect(h);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "\nTest 3: Multiple contexts" << std::endl;
    {
        auto h1 = async::connect(2);
        auto h2 = async::connect(4);
        
        async::receive(h1, "a\nb\nc\n", 6);
        async::receive(h2, "x\ny\nz\nw\n", 8);
        
        async::disconnect(h1);
        async::disconnect(h2);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "\nTest 4: C interface" << std::endl;
    {
        auto h = async_connect(2);
        const char* data = "test1\ntest2\n";
        async_receive(h, data, 12);
        async_disconnect(h);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "\n=== Demo completed ===" << std::endl;
    std::cout << "Check created log files in current directory." << std::endl;
    
    return 0;
}