#pragma once
#include <vector>
#include <numeric>
#include <fstream>
#include <mutex>

namespace thermometer
{

    template <typename T>
    class Measure
    {
    public:
        Measure() = default;

        void add_to_series(const T &value)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            series.push_back(value);
        }

        virtual void save_series_to_file(const std::string &label, const std::string &filename)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::ofstream file(filename, std::ios_base::app);
            file << label << '\n';
            for (auto &value : series)
            {
                file << value << '\n';
            }
            file.close();
        }

    protected:
        std::vector<T> series;
        std::mutex mutex_;
    };

    template <typename T>
    class AggregateMean : public Measure<T>
    {
    public:
        void save_series_to_file(const std::string &label, const std::string &filename) override
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            if (this->series.empty())
                return;
            std::ofstream file(filename, std::ios_base::app);
            T sum = std::accumulate(this->series.begin(), this->series.end(), static_cast<T>(0));
            file << label << ": " << sum / this->series.size() << '\n';
            file.close();
        }
    };

    template <typename T>
    class AggregateMeanSecondsToMinutes : public Measure<T>
    {
    public:
        void save_series_to_file(const std::string &label, const std::string &filename) override
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            if (this->series.empty())
                return;
            std::ofstream file(filename, std::ios_base::app);
            T sum = std::accumulate(this->series.begin(), this->series.end(), static_cast<T>(0));
            file << label << ": " << sum / 60.0 / this->series.size() << '\n';
            file.close();
        }
    };

} // namespace thermometer
