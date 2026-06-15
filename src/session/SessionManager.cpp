#include "session/SessionManager.h"

SessionManager::SessionManager(std::chrono::seconds timeout)
    : timeout_(timeout) {}

bool SessionManager::isExpired(const Session& s) const {
    return Clock::now() - s.lastAccess > timeout_;
}

void SessionManager::setKey(int64_t chatId, std::vector<uint8_t> key) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[chatId] = {std::move(key), Clock::now()};
}

const std::vector<uint8_t>* SessionManager::getKey(int64_t chatId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(chatId);
    if (it == sessions_.end()) return nullptr;
    if (isExpired(it->second)) {
        sessions_.erase(it);
        return nullptr;
    }
    it->second.lastAccess = Clock::now();
    return &it->second.key;
}

bool SessionManager::hasSession(int64_t chatId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(chatId);
    if (it == sessions_.end()) return false;
    if (isExpired(it->second)) {
        sessions_.erase(it);
        return false;
    }
    return true;
}

void SessionManager::removeSession(int64_t chatId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(chatId);
}
