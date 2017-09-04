/// @file
/// Объявление наблюдателя за процессами.

#pragma once

#ifndef TESTTOOLS_PROCESSNAMEOBSERVER_H
#define TESTTOOLS_PROCESSNAMEOBSERVER_H

#include "abstractobserver.h"

#include <list>
#include <unordered_map>

namespace testtools
{

/// Наблюдатель за производительностью процессов.
class ProcessObserver : public AbstractObserver
{
public:
    /// Счетчики доступные для опроса
    enum Counter
    {
        HandleCount                 = 1,    ///< Количество открытых дескрипторов
        ThreadCount                 = 2,    ///< Количество потоков
        ProcessorUsage              = 4,    ///< Процент загрузки процессора
        PhysicalMemoryUsage         = 8,    ///< Процент потребления физической памяти
        PhysicalMemoryUsageKBytes   = 16,   ///< Потребление физической памяти в килобайтах
        VirtualMemoryUsage          = 32,   ///< Процент потребления виртуальной памяти
        VirtualMemoryUsageKBytes    = 64,   ///< Потребление виртуальной памяти в килобайтах
    };

    /// Исключение выбрасывается в случае невозможности найти процесс по идентификатору.
    class ProcessNotFound : public Exception
    {
    public:
        /// @param[in] pid Идентификатор процесса
        explicit ProcessNotFound(uint32_t pid);
        virtual ~ProcessNotFound() = default;
    }; // class ProcessNotFound

protected:
    /// Экземпляр процесса
    class Instance
    {
        friend class ProcessObserver;

    public:
        /// Результат опроса счетчиков
        typedef std::unordered_map<std::string, double> Result;

    public:
#if defined(TESTTOOLS_WIN)
        /// @throw AbstractObserver#SystemError
        /// @param[in] query Дескриптор запроса
        /// @param[in] name Название процесса
        /// @param[in] index Индекс экземпляра
        Instance(pdh::Query query, const std::string& name, int16_t index = 0);
#endif
        ~Instance() = default;

        /// Возвращает результат опроса счетчиков
        /// @throw AbstractObserver#SystemError
        /// @param[in] mask Маска счетчиков
        /// @return Карта значений счетчиков в формате "счетчик=значение"
        Result Poll(Mask mask) const;

#if defined(TESTTOOLS_WIN)
    private:
        /// Возвращает значение потребления физической памяти для конкретного экземпляра процесса
        /// @throw AbstractObserver#SystemError
        /// @param[in] handle Дескриптор счетчика потребления физической памяти (@e physicalMemoryUsage_)
        /// @param[in] kbytes Вернуть значение в килобайтах?
        /// @return Если параметр @e kbytes равен @b false - процент, иначе - количество килобайт
        static double GetPhysicalMemoryUsage(pdh::Counter handle, bool kbytes = false);
        /// Возвращает значение потребления виртуальной памяти для конкретного экземпляра процесса
        /// @throw AbstractObserver#SystemError
        /// @param[in] handle Дескриптор счетчика потребления фиртуальной памяти (@e virtualMemoryUsage_)
        /// @param[in] kbytes Вернуть значение в килобайтах?
        /// @return Если параметр @e kbytes равен @b false - процент, иначе - количество килобайт
        static double GetVirtualMemoryUsage(pdh::Counter handle, bool kbytes = false);
#endif

#if defined(TESTTOOLS_WIN)
        /// Количество доступной физической памяти в байтах
        static double totalPhysicalMemory_;
        /// Количество доступной виртуальной памяти в байтах
        static double totalVirtualMemory_;

        /// Счетчик с идентификатором процесса
        pdh::UniqueCounter processId_;
        /// Счетчик количества открытых дескрипторов
        pdh::UniqueCounter handleCount_;
        /// Счетчик количества потоков
        pdh::UniqueCounter threadCount_;
        /// Счетчик процента загрузки процессора
        pdh::UniqueCounter processorUsage_;
        /// Счетчик потребления физической памяти
        pdh::UniqueCounter physicalMemoryUsage_;
        /// Счетчик потребления виртуальной памяти
        pdh::UniqueCounter virtualMemoryUsage_;
#endif
    };

protected:
    /// @throw AbstractObserver#SystemError
    /// @param[in] pid Идентификатор процесса
    ProcessObserver(uint32_t pid);
    /// @throw AbstractObserver#SystemError
    /// @param[in] name Название процесса
    ProcessObserver(const std::string& name);
    virtual ~ProcessObserver() = default;
}; // class ProcessObserver

/// Наблюдатель за производительностью конкретного процесса по его идентификатору.
class ProcessIdObserver final : public ProcessObserver
{
public:
    /// Результат опроса счетчиков
    typedef std::unordered_map<std::string, double> Result;

public:
    /// @throw AbstractObserver#SystemError, ProcessObserver#ProcessNotFound
    /// @param[in] pid Идентификатор процесса
    explicit ProcessIdObserver(uint32_t pid);
    ~ProcessIdObserver() = default;

    /// Возвращает результат опроса счетчиков
    /// @throw AbstractObserver#SystemError
    /// @param[in] mask Маска счетчиков
    /// @return Карта значений счетчиков в формате "счетчик=значение"
    Result Poll(Mask mask);

private:
    /// Идентификатор процесса
    uint32_t pid_;
    /// Индекс экземпляра процесса
    int16_t index_;
    /// Наблюдатель за процессом
    std::unique_ptr<Instance> instance_;
}; // class ProcessIdObserver

/// Наблюдатель за производительностью списка процессов по имени.
class ProcessNameObserver final : public ProcessObserver
{
public:
    /// Результаты опроса счетчиков
    typedef std::list<std::unordered_map<std::string, double>> Result;

public:
    /// @throw AbstractObserver#SystemError
    /// @param[in] name Название процесса
    explicit ProcessNameObserver(const std::string& name);
    ~ProcessNameObserver() = default;

    /// Возвращает результат опроса счетчиков
    /// @throw AbstractObserver#SystemError
    /// @param[in] mask Маска счетчиков
    /// @return Массив карт значений счетчиков в формате "счетчик=значение" для каждого экземпляра процесса
    Result Poll(Mask mask);

private:
    /// Список умных указателей на экземпляры процессов
    std::list<std::unique_ptr<Instance>> instances_;
}; // class ProcessNameObserver

} // namespace testtools

#endif // TESTTOOLS_PROCESSNAMEOBSERVER_H
