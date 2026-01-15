// include/async/async.h
#ifndef ASYNC_H
#define ASYNC_H

#include <cstddef>

#ifdef _WIN32
    #ifdef ASYNC_EXPORTS
        #define ASYNC_API __declspec(dllexport)
    #else
        #define ASYNC_API __declspec(dllimport)
    #endif
#else
    #define ASYNC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* async_handle_t;

ASYNC_API async_handle_t async_connect(std::size_t bulk);
ASYNC_API void async_receive(async_handle_t handle, const char* data, std::size_t size);
ASYNC_API void async_disconnect(async_handle_t handle);

#ifdef __cplusplus

namespace async {

using handle_t = async_handle_t;

inline handle_t connect(std::size_t bulk) {
    return async_connect(bulk);
}

inline void receive(handle_t handle, const char* data, std::size_t size) {
    async_receive(handle, data, size);
}

inline void disconnect(handle_t handle) {
    async_disconnect(handle);
}

} // namespace async

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // ASYNC_H