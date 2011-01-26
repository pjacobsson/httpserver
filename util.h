#ifndef UTIL_H_
#define UTIL_H_

#include <map>
#include <vector>

#include <pthread.h>

using namespace std;

namespace util {

  // Simple threadsafe map, storing keys and pointers to values.
  template<class K, class V>
  class SynchronizedMap {
  public:
    SynchronizedMap() {};

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

    SynchronizedMap(const SynchronizedMap&);
    SynchronizedMap& operator=(const SynchronizedMap&);
  };

  // Simple threadsafe queue, storing pointers to values.
  template<class T>
  class SynchronizedQueue {
  public:
    SynchronizedQueue() {};

    void Initialize() {
      pthread_mutex_init(&mutex_, NULL);
    };

    void Push(T* value) {
      pthread_mutex_lock(&mutex_);
      data_.push_back(value);
      pthread_mutex_unlock(&mutex_);
    };

    // TODO: Should pull from the front
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

    SynchronizedQueue(const SynchronizedQueue&);
    SynchronizedQueue& operator=(const SynchronizedQueue&);
  };
}

#endif  // UTIL_H_
