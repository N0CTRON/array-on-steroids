#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <random>
#include <thread>

template <typename arrType>
class AOS // AOS = array on steroids
{
private:
    std::uint64_t vsize = 0;
    std::uint64_t threads = 6;

public:
    arrType* data = nullptr;

    AOS() : data(nullptr) {}
    AOS(std::uint64_t nSize) { init(nSize); }
    AOS(std::uint64_t nSize, arrType defaultVal) { init(nSize, defaultVal); }

    AOS(std::initializer_list<arrType> list)
    {
        vsize = list.size();
        data = new arrType[vsize];
        std::copy(list.begin(), list.end(), data);
    }

    void init(std::uint64_t nSize)
    {
        vsize = nSize;
        try
        {
            if (data == nullptr) delete[] data;
            data = new arrType[nSize];
            std::fill_n(data, nSize, arrType());
        }
        catch (const std::bad_alloc& e)
        {
            throw std::runtime_error("[ | AOS-LOG ]: Failed to allocate memory for array at aos.hpp, first init function.\n" + *e.what());
            exit(-1);
        }
    }

    void init(std::uint64_t nSize, arrType defaultVal)
    {
        vsize = nSize;
        try
        {
            if (data == nullptr) delete[] data;
            data = new arrType[nSize];
            std::fill_n(data, nSize, defaultVal);
        }
        catch (const std::bad_alloc& e)
        {
            throw std::runtime_error(std::string("[ | AOS-LOG ]: Failed to allocate memory for array at aos.hpp, second init function.\n" + *e.what()).c_str());
            exit(-1);
        }
    }

    void setThreads(std::uint64_t nThreads) { this->threads = nThreads; }

    void resize(std::uint64_t nSize)
    {
        if (nSize != vsize)
        {
            arrType* newData = new arrType[nSize];
            std::uint64_t minSize = std::min(vsize, nSize);
            std::copy(data, data + minSize, newData);
            std::fill(newData + minSize, newData + nSize, arrType());
            if (data) delete[] data;
            data = newData;
            vsize = nSize;
        }
    }

    void random(AOS<float> range)
    {
        if (range.size() >= 2)
        {
            try
            {
                std::random_device rd;
                if (range[0] > range[1])
                {
                    float tmp = range[0];
                    range[0] = range[1];
                    range[1] = tmp;
                }
                std::uniform_real_distribution<float> dist(range[0], range[1]);
                auto thread_function = [&](std::uint64_t start, std::uint64_t end)
                { for (std::uint64_t b = start; b < end; ++b) data[b] = (arrType)dist(rd); };

                std::vector<std::thread> thread_pool;
                std::uint64_t chunk_size = vsize / threads;
                std::uint64_t remainder = vsize % threads;
                std::uint64_t start = 0;

                for (std::uint64_t i = 0; i < threads; ++i)
                {
                    std::uint64_t end = start + chunk_size + (i < remainder ? 1 : 0);
                    thread_pool.emplace_back(thread_function, start, end);
                    start = end;
                }
                for (auto& t : thread_pool) { t.join(); }
            }
            catch (const std::exception& e)
            { throw std::runtime_error(std::string("AOS threw exception at \"random\" function: " + *e.what()).c_str()); }
        }
        else throw std::runtime_error("AOS exception at random: AOS<float>'s size is less than two.");
    }

    std::uint64_t size() const { return vsize; }

    arrType dot(const AOS<arrType>& other) const
    {
        if (other.size() != vsize) throw std::runtime_error("Sizes don't match for dot product");

        arrType result = 0;
        std::vector<arrType> partial_results(threads, 0);
        std::vector<std::thread> thread_pool;

        auto worker = [&](std::uint64_t start, std::uint64_t end, std::uint64_t thread_id)
        {
            arrType local_sum = 0;
            for (std::uint64_t i = start; i < end; ++i) local_sum += this->data[i] * other.data[i];
            partial_results[thread_id] = local_sum;
        };

        std::uint64_t chunk_size = vsize / threads;
        for (std::uint64_t t = 0; t < threads; ++t)
        {
            std::uint64_t start = t * chunk_size;
            std::uint64_t end = (t == threads - 1) ? vsize : (t + 1) * chunk_size;
            thread_pool.emplace_back(worker, start, end, t);
        }

        for (auto& thread : thread_pool) thread.join();
        for (const auto& partial : partial_results) result += partial;

        return result;
    }

    void suicide()
    {
        if (this->data)
        {
            delete[] this->data;
            data = nullptr;
        }
    }

    ~AOS() { if (data == nullptr) delete[] data; }

    arrType& operator[](const std::uint64_t index)
    {
#ifdef DEBUG
        if (index >= vsize) throw std::runtime_error("Array subscript out of range: index was " + std::to_string(index) + ", max_size was " + std::to_string(vsize) + ".");
#endif // DEBUG

        return data[index];
    }
    
    AOS& operator=(const std::initializer_list<arrType> list)
    {
        if (data) delete[] data;
        vsize = list.size();
        try { data = new arrType[vsize]; }
        catch (const std::bad_alloc& e)
        {
            std::cerr << "[ | AOS-LOG ]: Failed to allocate memory for array at aos.hpp at overloaded = std::init... operator.\n" << e.what() << std::endl;
            exit(-1);
        }
        std::copy(list.begin(), list.end(), data);
        return *this;
    }

    AOS& operator=(const AOS& other)
    {
        if (this != &other)
        {
            if (data) delete[] data;
            vsize = other.vsize;
            data = new arrType[vsize];
            if (!data)
            {
                std::cerr << "[ | AOS-LOG ]: Failed to allocate memory for array at aos.hpp at AOS& operator=(const AOS&)." << std::endl;
                exit(-1);
            }
            std::copy(other.data, other.data + vsize, data);
        }
        return *this;
    }

    AOS operator+(const AOS& other) const
    {
        if (other.size() == vsize)
        {
            AOS<arrType> nList(vsize);
            std::vector<std::thread> thread_pool;

            auto worker = [&](std::uint64_t start, std::uint64_t end)
            { for (std::uint64_t i = start; i < end; ++i) nList[i] = this->data[i] + other.data[i]; };

            std::uint64_t chunk_size = vsize / threads;
            for (std::uint64_t t = 0; t < threads; ++t)
            {
                std::uint64_t start = t * chunk_size;
                std::uint64_t end = (t == threads - 1) ? vsize : (t + 1) * chunk_size;
                thread_pool.emplace_back(worker, start, end);
            }
            for (auto& thread : thread_pool) thread.join();
            return nList;
        }
        throw std::runtime_error("Sizes don't match");
    }

    AOS operator-(const AOS& other) const
    {
        if (other.size() == vsize)
        {
            AOS<arrType> nList(vsize);
            std::vector<std::thread> thread_pool;

            auto worker = [&](std::uint64_t start, std::uint64_t end)
            { for (std::uint64_t i = start; i < end; ++i) nList[i] = this->data[i] - other.data[i]; };

            std::uint64_t chunk_size = vsize / threads;
            for (std::uint64_t t = 0; t < threads; ++t)
            {
                std::uint64_t start = t * chunk_size;
                std::uint64_t end = (t == threads - 1) ? vsize : (t + 1) * chunk_size;
                thread_pool.emplace_back(worker, start, end);
            }
            for (auto& thread : thread_pool) thread.join();

            return nList;
        }
        throw std::runtime_error("Sizes don't match");
    }

    AOS operator*(const AOS& other) const
    {
        if (other.size() == vsize)
        {
            AOS<arrType> nList(vsize);
            std::vector<std::thread> thread_pool;

            auto worker = [&](std::uint64_t start, std::uint64_t end)
            { for (std::uint64_t i = start; i < end; ++i) nList[i] = this->data[i] * other.data[i]; };

            std::uint64_t chunk_size = vsize / threads;
            for (std::uint64_t t = 0; t < threads; ++t)
            {
                std::uint64_t start = t * chunk_size;
                std::uint64_t end = (t == threads - 1) ? vsize : (t + 1) * chunk_size;
                thread_pool.emplace_back(worker, start, end);
            }
            for (auto& thread : thread_pool) thread.join();

            return nList;
        }
        throw std::runtime_error("Sizes don't match");
    }

    AOS operator/(const AOS& other) const
    {
        if (other.size() == vsize)
        {
            AOS<arrType> nList(vsize);
            std::vector<std::thread> thread_pool;

            auto worker = [&](std::uint64_t start, std::uint64_t end)
            { for (std::uint64_t i = start; i < end; ++i) nList[i] = this->data[i] / other.data[i]; };

            std::uint64_t chunk_size = vsize / threads;
            for (std::uint64_t t = 0; t < threads; ++t)
            {
                std::uint64_t start = t * chunk_size;
                std::uint64_t end = (t == threads - 1) ? vsize : (t + 1) * chunk_size;
                thread_pool.emplace_back(worker, start, end);
            }
            for (auto& thread : thread_pool) thread.join();

            return nList;
        }
        throw std::runtime_error("Sizes don't match");
    }

    AOS operator%(const AOS& other) const
    {
        if (other.size() == vsize)
        {
            AOS<arrType> nList(vsize);
            std::vector<std::thread> thread_pool;

            auto worker = [&](std::uint64_t start, std::uint64_t end)
            { for (std::uint64_t i = start; i < end; ++i) nList[i] = this->data[i] % other.data[i]; };

            std::uint64_t chunk_size = vsize / threads;
            for (std::uint64_t t = 0; t < threads; ++t)
            {
                std::uint64_t start = t * chunk_size;
                std::uint64_t end = (t == threads - 1) ? vsize : (t + 1) * chunk_size;
                thread_pool.emplace_back(worker, start, end);
            }
            for (auto& thread : thread_pool) thread.join();

            return nList;
        }
        throw std::runtime_error("Sizes don't match");
    }

    AOS(const AOS& other)
    {
        vsize = other.size();
        data = new arrType[vsize];
        if (!data)
        {
            std::cerr << "[ | AOS-LOG ]: Failed to allocate memory for array at aos.hpp at AOS(const AOS&)." << std::endl;
            exit(-1);
        }
        std::copy(other.data, other.data + vsize, data);
    }
};
