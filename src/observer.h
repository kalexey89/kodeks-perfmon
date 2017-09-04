/// @file
/// Объявление экспортируемого в Node.JS наблюдателя.

#pragma once

#ifndef TESTTOOLS_OBSERVER_H
#define TESTTOOLS_OBSERVER_H

#include "abstractobserver.h"

#include <nan.h>
#include <nan_object_wrap.h>

/// Раскрывается в функцию инициализации Node.JS модуля
#define NODE_MODULE_INIT(fn) void (fn)(v8::Local<v8::Object> exports, v8::Local<v8::Object> module)

namespace testtools
{

/// Экспортируемый в Node.JS класс наблюдателя.
class Observer : public Nan::ObjectWrap
{
public:
    /// Инициализирует класс наблюдателя при подключении модуля
    /// @param[in] exports Объект "exports" в Node.JS
    /// @param[in] module Объект "module" в Node.JS
    static NODE_MODULE_INIT(Initialize);

private:
    /// Инициализирует наблюдателя за системой
    Observer();
    /// Инициализирует наблюдателя за процессом с указанным @e pid
    /// @param[in] pid Идентификатор процесса
    Observer(uint32_t pid);
    /// Инициализирует наблюдателя за списком процессов с указанным @e name
    /// @param[in] name Имя процесса
    Observer(const std::string& name);
    virtual ~Observer() = default;

private:
    /// Инициализирует объект наблюдателя
    /// @param[in] info Информация о переданных в функцию аргументах
    static NAN_METHOD(New);
    
    /// Реализует работу метода @e type наблюдателя
    /// @param[in] info Информация о переданных в функцию аргументах
    static NAN_METHOD(GetType);
    /// Реализует работу метода @e object наблюдателя
    /// @param[in] info Информация о переданных в функцию аргументах
    static NAN_METHOD(GetObject);
    /// Реализует работу метода @e poll наблюдателя
    /// @param[in] info Информация о переданных в функцию аргументах
    static NAN_METHOD(Poll);

    static NAN_METHOD(Processes);

    /// Возвращает дескриптор конструктора класса в V8 engine
    /// @return Дескриптор конструктора класса в V8 engine
    static Nan::Persistent<v8::Function>& GetConstructor() {
        static Nan::Persistent<v8::Function> ctor;
        return ctor;
    }

private:
    /// Указатель на реалиализацию наблюдателя. Зависит от вызываемого конструктора класса.
    std::unique_ptr<AbstractObserver> impl_;
}; // class Observer

} // namespace testtools

#endif // TESTTOOLS_OBSERVER_H
