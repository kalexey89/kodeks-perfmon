/// @file
/// Объявление абстрактного класса наблюдателя и вспомогательных типов.

#pragma once

#ifndef TESTTOOLS_ABSTRACTOBSERVER_H
#define TESTTOOLS_ABSTRACTOBSERVER_H

// Требуется С++ компилятор с поддержкой стандарта С++11
#if !defined(__cplusplus)
#error C++ compiler required.
#endif

#if (__cplusplus < 201103L) && (__cplusplus < 200000 && __cplusplus > 199711L)
#error C++11 compiler required.
#endif

#if defined(TESTTOOLS_WIN)

#if 0

#ifndef UNICODE
#define UNICODE 1
#endif

#ifndef _UNICODE
#define _UNICODE 1
#endif

#endif

// Используем ANSI версии WINAPI функций
#if 1

#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif

#endif

#define WIN32_LEAN_AND_MEAN 1

#include <windows.h>
#include <versionhelpers.h> // IsWindowsVistaOrGreater
#include <tlhelp32.h> // CreateToolhelp32Snapshot, Process32First, Process32Next
#include <psapi.h>
#include <shlwapi.h>
#include <pdh.h> // PDH функции
#include <pdhmsg.h> // PDH константы

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "pdh.lib")

#elif defined(TESTTOOLS_LINUX)

#else
#error Platform not supported
#endif

#include <cstdint>
#include <exception>
#include <memory>
#include <list>
#include <string>

/// Корневое пространство имен проекта.
namespace testtools
{

#if defined(TESTTOOLS_WIN)
/// Вспомогательные классы и типы для PDH.
namespace pdh
{

///  Класс удаления дескриптора запроса для умного указателя
struct QueryDeleter {
    /// Тип указателя на запрос
    typedef PDH_HQUERY pointer;
    /// Функтор указывающий на функцию удаления запроса
    inline void operator()(PDH_HQUERY handle) { ::PdhCloseQuery(handle); }
};

/// Класс удаления дескриптора счетчика для умного указателя
struct CounterDeleter {
    /// Тип указателя на счетчик
    typedef PDH_HCOUNTER pointer;
    /// Функтор указывающий на функцию удаления счетчика
    inline void operator()(PDH_HCOUNTER handle) { ::PdhRemoveCounter(handle); }
};

/// Результат выполнения функции
typedef PDH_STATUS Result;
/// Структура для хранения статуса и значения счетчика
typedef PDH_FMT_COUNTERVALUE Value;
/// Дескриптор запроса
typedef PDH_HQUERY Query;
/// Умный указатель на дескриптор запроса
typedef std::unique_ptr<Query, QueryDeleter> UniqueQuery;
/// Дескриптор счетчика
typedef PDH_HCOUNTER Counter;
/// Умный указатель на дескриптор счетчика
typedef std::unique_ptr<Counter, CounterDeleter> UniqueCounter;

} // namespace pdh
#endif

/// Абстрактный наблюдатель за производительностью. 
/// Является базовым для всех остальных классов наблюдателей.
class AbstractObserver
{
public:
    /// Тип наблюдателя
    enum Type
    { 
        System = 0,     ///< Система
        ProcessId,      ///< Конкретный процесс (по идентификатору)
        ProcessName     ///< Список процессов (по имени)
    };

    /// Структура для хранения данных о процессе
    typedef struct
    {
        uint32_t pid;
        uint32_t ppid;
        std::string name;
        std::string path;
        std::string owner;
        uint32_t priority;
        uint32_t status;
        uint32_t handles;
        uint32_t threads;
        double ktime;
        double utime;
        double pmemory;
        double vmemory;
        double start;
    } Process;

    /// Маска элементов опроса
    typedef uint32_t Mask;

    /// Базовый класс исключения.
    class Exception : public std::exception
    {
    public:
        /// @param[in] message Текст сообщения об ошибке
        explicit Exception(const std::string& message)
            : exception(nullptr)
            , message_(message) {}
        /// @param[in] message Текст сообщения об ошибке
        explicit Exception(const char* message)
            : exception(nullptr)
            , message_(message) {}
        virtual ~Exception() = default;

        /// Возвращает текст сообщения об ошибке
        /// @return Текст сообщения об ошибке
        inline virtual const char* what() const override { return message_.data(); };

    protected:
        /// Текст ошибки
        std::string message_;
    }; // class Exception

    /// Исключение выбрасывается в случае возникновения системной ошибки.
    class SystemError : public Exception
    {
    public:
        /// @param[in] code Системный код ошибки
        explicit SystemError(errno_t code);
        virtual ~SystemError() = default;
    }; // class SystemError

public:
#if defined(TESTTOOLS_WIN)
    /// @throw SystemError
    /// @param[in] type Тип наблюдателя
    /// @param[in] object Название объекта наблюдения
    AbstractObserver(uint8_t type, const std::string& object);
#endif
    virtual ~AbstractObserver() = default;

    /// Возвращает тип наблюдателя
    /// @return Тип наблюдателя
    inline uint8_t GetType() const noexcept { return type_; }

    /// Возвращает название объекта наблюдения (например - "kserver.exe")
    /// @return Название объекта наблюдения
    inline std::string GetObject() const noexcept { return object_; }

public:
    /// Возвращет список запущенных в системе процессов
    /// @throw SystemError
    /// @return Список структур с информацией о каждом процессе
    static std::list<Process> GetProcessList();

#if defined(TESTTOOLS_WIN)
protected:
    /// Добавляет новый счетчик к запросу
    /// @throw SystemError
    /// @param[in] query Дескриптор запроса
    /// @param[in] path Полный путь к счетчику
    /// @return Дескриптор счетчика
    static pdh::Counter AddPdhCounter(pdh::Query query, const std::string& path);

    /// Возвращает значение счетчика по его дескриптору
    /// @throw SystemError
    /// @param[in] handle Дескриптор счетчика
    /// @return Значение счетчика
    static double GetPdhValue(pdh::Counter handle);
#endif

#if defined(TESTTOOLS_WIN)
protected:
    /// Дескриптор запроса
    std::unique_ptr<
        std::remove_pointer<PDH_HQUERY>::type, 
        decltype(&::PdhCloseQuery)> query_;
#endif

private:
    /// Тип наблюдателя
    uint8_t type_;
    /// Название наблюдаемого объекта
    std::string object_;
}; // class AbstractObserver

} // namespace testtools

#endif // TESTTOOLS_ABSTRACTOBSERVER_H
