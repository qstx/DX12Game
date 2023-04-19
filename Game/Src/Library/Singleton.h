#pragma once

#include <mutex>

namespace Library
{

    template <typename T>
    class Singleton
    {
    public:
        static T* GetInstance();
        static std::mutex lock;
        Singleton(Singleton& other) = delete;
        void operator=(const Singleton& other) = delete;

    private:
        static T* instance;
        ~Singleton();
    };
    template <typename T>
    T* Singleton<T>::instance(nullptr);

    template <typename T>
    std::mutex Singleton<T>::lock;

    template <typename T>
    T* Singleton<T>::GetInstance()
    {
        if (!instance)
        {
            std::lock_guard<std::mutex> locker(lock);
            if (!instance)
            {
                instance = new T();
            }
        }
        return instance;
    }

    template <typename T>
    Singleton<T>::~Singleton()
    {
        delete instance;
    }
    template<typename T>
    inline T* GetInstance()
    {
        return Singleton<T>::GetInstance();
    }
//#define GetInstance(Type) (Singleton<Type>::GetInstance())
}