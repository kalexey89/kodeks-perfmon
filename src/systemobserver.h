/// @file
/// Объявление наблюдателя за системой.

#pragma once

#ifndef TESTTOOLS_SYSTEMOBSERVER_H
#define TESTTOOLS_SYSTEMOBSERVER_H

#include "abstractobserver.h"

#include <unordered_map>

namespace testtools
{

/// Наблюдатель за производительностью системы.
class SystemObserver final : public AbstractObserver
{
public:
    /// Счетчики доступные для опроса
    enum Counter
    {
        ProcessCount                = 1,    ///< Количество процессов
        ThreadCount                 = 2,    ///< Количество потоков
        ProcessorUsage              = 4,    ///< Процент загрузки процессора
        PhysicalMemoryUsage         = 8,    ///< Процент потребления физической памяти
        PhysicalMemoryUsageKBytes   = 16,   ///< Потребление физической памяти в килобайтах
        VirtualMemoryUsage          = 32,   ///< Процент потребления виртуальной памяти
        VirtualMemoryUsageKBytes    = 64,   ///< Потребление виртуальной памяти в килобайтах
        DiskUsage                   = 128   ///< Процент загрузки жесткого диска
    };

    /// Результаты опроса счетчиков
    typedef std::unordered_map<std::string, double> Result;

public:
    /// Конструктор по умолчанию
    /// @throw AbstractObserver#SystemError
    SystemObserver();
    ~SystemObserver() = default;

    /// Возвращает результат опроса счетчиков
    /// @throw SystemError
    /// @param[in] mask Маска счетчиков
    /// @return Карта значений счетчиков в формате "счетчик=значение"
    Result Poll(Mask mask) const;

#if defined(TESTTOOLS_WIN)
private:
    /// Возвращает значение загрузки физической памяти
    /// @throw AbstractObserver#SystemError
    /// @param[in] kbytes Вернуть значение в килобайтах?
    /// @return Если параметр @e kbytes равен @b false - процент, иначе - количество килобайт
    static double GetPhysicalMemoryUsage(bool kbytes = false);
    /// Возвращает значение загрузки виртуалной памяти
    /// @throw AbstractObserver#SystemError
    /// @param[in] kbytes Вернуть значение в килобайтах?
    /// @return Если параметр @e kbytes равен @b false - процент, иначе - количество килобайт
    static double GetVirtualMemoryUsage(bool kbytes = false);
#endif

#if defined(TESTTOOLS_WIN)
private:
    /// Счетчик количества процессов
    pdh::UniqueCounter processCount_;
    /// Счетчик количесва потоков
    pdh::UniqueCounter threadCount_;
    /// Счетчик процента загрузки процессора
    pdh::UniqueCounter processorUsage_;
    /// Счетчик процента загрузки жесткого диска
    pdh::UniqueCounter diskUsage_;
#endif
}; // class SystemObserver

} // namespace testtools

#endif // TESTTOOLS_SYSTEMOBSERVER_H
