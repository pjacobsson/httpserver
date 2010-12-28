#ifndef UTIL_H_
#define UTIL_H_

#include <map>
#include <vector>

#include <pthread.h>

using namespace std;

// TODO: Constructors.
namespace util {

  // Simple threadsafe map.
  template<class K, class V>
  class SynchronizedMap {
  public:
    void Initialize() {
      pthread_mutex_init(&mutex_, NULL);
    };

    V* Get(K key) {
      pthread_mutex_lock(&mutex_);
      typename map<K,V*>::iterator it = data_.find(key);
      V* result = NULL;
      if (it != data_.end()) {
	result = it->second;
      }
      pthread_mutex_unlock(&mutex_);
      return result;
    };

    void Put(K key, V* value) {
      pthread_mutex_lock(&mutex_);
      data_[key] = value;
      pthread_mutex_unlock(&mutex_);
     };

    void Remove(K key) {
      pthread_mutex_lock(&mutex_);
      typename map<K, V*>::iterator it = data_.find(key);
      if (it != data_.end()) {
	data_.erase(it);
      }
      pthread_mutex_unlock(&mutex_);
   };
  private:
    map<K, V*> data_;
    pthread_mutex_t mutex_;
  };

  // Simple threadsafe queue.
  template<class T>
  class SynchronizedQueue {
  public:
    void Initialize() {
      pthread_mutex_init(&mutex_, NULL);
    };

    void Push(T* value) {
      pthread_mutex_lock(&mutex_);
      data_.push_back(value);
      pthread_mutex_unlock(&mutex_);
    };

    T* Pop() {
      pthread_mutex_lock(&mutex_);
      T* value = data_.back();
      data_.pop_back();
      pthread_mutex_unlock(&mutex_);
      return value;
    };

    // Delete all queued up entries
    void Clear() {
      pthread_mutex_lock(&mutex_);
      for (int i=0; i<data_.size(); ++i) {
	delete data_[i];
      }
      data_.clear();
      pthread_mutex_unlock(&mutex_);
    };
  private:
    vector<T*> data_;
    pthread_mutex_t mutex_;
  };
}

#endif  // UTIL_H_
