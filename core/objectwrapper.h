/*
 *  objecthandle.h
 *
 *  This file is part of GammaRay, the Qt application inspection and
 *  manipulation tool.
 *
 *  Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
 *  Author: Anton Kreuzkamp <anton.kreuzkamp@kdab.com>
 *
 *  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
 *  accordance with GammaRay Commercial License Agreement provided with the Software.
 *
 *  Contact info@kdab.com if any conditions of this licensing are not clear to you.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GAMMARAY_OBJECTHANDLE_H
#define GAMMARAY_OBJECTHANDLE_H

#include <core/probe.h>

#include <QObject>
#include <QMetaObject>
#include <QMetaProperty>
#include <QDebug>
#include <QMetaType>
#include <QMutex>
#include <QSemaphore>
#include <QThread>

#include <future>


#include <future>
#include <tuple>
#include <memory>

#include <iostream>

#include <list>

#include <private/qobject_p.h>
#include <private/qmetaobject_p.h>

template<typename T> class error;

template<int i> class err;



/**
 * Adds the inline definition of a getter function with the name @p FieldName,
 * which returns the data stored at index @p StorageIndex in
 * m_control->dataStorage.
 * This is internal for use in other macros.
 */
#define DEFINE_GETTER(FieldName, StorageIndex, Command)                                                                                 \
decltype(wrap(std::declval<value_type>().Command)) FieldName() const                                                                    \
{                                                                                                                                       \
    m_control->semaphore.acquire();                                                                                                     \
    QSemaphoreReleaser releaser { &m_control->semaphore };                                                                              \
    return std::get< StorageIndex >(m_control->dataStorage);                                                                            \
}                                                                                                                                       \

/**
 * Adds the inline definition of a method, which holds state using overloading
 * and inheritance tricks as described at
 * https://woboq.com/blog/verdigris-implementation-tricks.html
 *
 * Incrementing the counter is done by using a combination of STATE_APPEND and
 * DEFINE_COUNTER. Use DEFINE_COUNTER to define a constexpr variable holding the
 * current count + 1. Use STATE_APPEND to make it the new current count.
 *
 * Use BodyExpression to return arbitary content, possibly using recursion. Eg.
 * you can do
 * `std::tuple_cat(MethodName(self, w_counter.prev()), std::make_tuple(...)))`
 * to recursively compose a compile-time list.
 *
 * This is meant for internal use in other MACROS only.
 */

// template<typename T, typename ...TupleArgs_t>
// auto tuple_append(std::tuple<TupleArgs_t...>, T) -> std::tuple<TupleArgs_t..., T>;
// template<typename Tuple_t, typename T>
// using tuple_append_t = decltype(tuple_append(std::declval<Tuple_t>(), std::declval<T>()));

template<typename T, typename ...Args>
struct tuple_append {};
template<typename T, typename ...TupleArgs_t>
struct tuple_append<std::tuple<TupleArgs_t...>, T> {
    using type = std::tuple<TupleArgs_t..., T>;
};
template<typename Tuple_t, typename T>
using tuple_append_t = typename tuple_append<Tuple_t, T>::type;

#define STATE_APPEND(MethodName, Counter, AppendedType, StateExpr)                                                                                   \
friend auto MethodName(ThisClass_t *self, w_number<Counter> w_counter)                                                                  \
-> tuple_append_t<decltype(MethodName(self, w_counter.prev())), AppendedType>                                                    \
{                                                                                                                                       \
    return std::tuple_cat(MethodName(self, w_counter.prev()), std::make_tuple(StateExpr));                                              \
}

/**
 * Defines a constexpr variable that fetches the current value of the count
 * used by the constexpr-state code defined by STATE_APPEND and stores it
 * incremented by one.
 *
 * @p CounterName is the name of the variable to be defined
 * @p CounterMethod is the name of the method, which holds the state. This
 *                  method must return a tuple, whose size equals the
 *                  current count.
 *
 * Incrementing the counter is done by using a combination of STATE_APPEND and
 * DEFINE_COUNTER. Use DEFINE_COUNTER to define a constexpr variable holding the
 * current count + 1. Use STATE_APPEND to make it the new current count.
 *
 * This is meant for internal use in other MACROS only.
 */
#define DEFINE_COUNTER(CounterName, CounterMethod)                                                                                      \
static constexpr int CounterName =                                                                                                      \
std::tuple_size<decltype(CounterMethod(static_cast<ThisClass_t*>(nullptr), w_number<255>{}))>::value + 1;                               \

/**
 * Adds a member field to the object wrapper. The data will be accessible
 * through a getter in the wrapper, named as \p FieldName and will contain the
 * result of a call to `obj->\p Command`.
 *
 * Example: If you used obj->x() before to access some data, you can make that
 * available to the wrapper, by writing `MEMBER(x, x())`. Later, use wrapper.x()
 * to access it.
 */
#define MEMBER(FieldName, Command)                                                                                                      \
DEFINE_COUNTER(W_COUNTER_##FieldName, w_data)                                                                                           \
STATE_APPEND(w_data, W_COUNTER_##FieldName, decltype(wrap(std::declval<value_type>().Command)), wrap(self->object->Command))                                                               \
DEFINE_GETTER(FieldName, W_COUNTER_##FieldName - 1, Command)                                                                            \

/**
 * The same as MEMBER, but doesn't prepend `obj->` to the command. Instead
 * you need to use `self->object` in your command. This is useful if you
 * want to add a data field to the wrapper that corresponds to no getter
 * in the original class, e.g.
 * `MEMBER2(name, Utils::getCanonicalName(self->object))`.
 */
#define MEMBER2(FieldName, Command)                                                                                                     \
DEFINE_COUNTER(W_COUNTER_##FieldName, w_data)                                                                                           \
STATE_APPEND(w_data, W_COUNTER_##FieldName, decltype(wrap(Command)), wrap(Command))                                                                             \
DEFINE_GETTER(FieldName, W_COUNTER_##FieldName - 1, Command)                                                                            \

/**
 * Adds a member field for a Q_PROPERTY. As with usual getters, the data will be
 * accessible through a getter in the wrapper, named as \p FieldName and will
 * contain the result of a call to `obj->\p Command`. \p Command is necessary to
 * provide a strongly typed API, as Q_PROPERTIES are type-erased.
 *
 * Unlike MEMBER, this will receive updates through notify-signals.
 * \sa MEMBER
 * \sa MEMBER_WITH_NOTIFY
 */
#define QPROP_MEMBER(FieldName, Command)                                                                                                \
MEMBER(FieldName, Command)                                                                                                              \
friend void w_connectToUpdates(ThisClass_t *self, w_number<W_COUNTER_##FieldName>)                                                      \
{                                                                                                                                       \
                                                                                                                                        \
    connectToUpdates< W_COUNTER_##FieldName - 1 >(self, [](value_type *obj) { return wrap(obj->Command); }, #FieldName);                      \
    w_connectToUpdates(self, w_number< W_COUNTER_##FieldName - 1 >{});                                                                  \
}                                                                                                                                       \

/**
 * Adds a member field to the object wrapper, which can update itself by
 * listening to the \p NotifySignal. Use only the signal's identifier as
 * parameter to NotifySignal.
 *
 * Example:
 * MEMBER_WITH_NOTIFY(parent, parentItem(), itemReparented)
 *
 * \sa MEMBER
 */
#define MEMBER_WITH_NOTIFY(FieldName, Command, NotifySignal)                                                                            \
MEMBER(FieldName, Command)                                                                                                              \
friend void w_connectToUpdates(ThisClass_t *self, w_number<W_COUNTER_##FieldName>)                                                      \
{                                                                                                                                       \
    connectToUpdates< W_COUNTER_##FieldName - 1 >(self, [](value_type *obj) { return wrap(obj->Command); }, &value_type::NotifySignal);       \
    w_connectToUpdates(self, w_number< W_COUNTER_##FieldName - 1 >{});                                                                  \
}                                                                                                                                       \

/**
 * Adds a member field for data which is only available through the private
 * object. It uses `PrivateClass::get(obj)->Command` to get the data.
 */
#define DPTR_MEMBER(PrivateClass, FieldName, Command)                                                                                   \
DEFINE_COUNTER(W_COUNTER_##FieldName, w_data)                                                                                           \
STATE_APPEND(w_data, W_COUNTER_##FieldName, decltype(wrap(std::declval<PrivateClass>().Command)), wrap(PrivateClass::get(self->object)->Command))                                             \
DEFINE_GETTER(FieldName, W_COUNTER_##FieldName - 1, Command)                                                                            \


/**
 * Adds a member field for data which is only available through the private
 * object and can be updated through a notifySignal. The notify signal is
 * assumed to be a signal of the object, not the private object.
 *
 * \sa DPTR_MEMBER
 * \sa MEMBER_WITH_NOTIFY
 */
#define DPTR_MEMBER_WITH_NOTIFY(PrivateClass, FieldName, Command, NotifySignal)                                                         \
DEFINE_COUNTER(W_COUNTER_##FieldName, w_data)                                                                                           \
STATE_APPEND(w_data, W_COUNTER_##FieldName, decltype(wrap(std::declval<PrivateClass>().Command)), wrap(PrivateClass::get(self->object)->Command))                                             \
DEFINE_GETTER(FieldName, W_COUNTER_##FieldName - 1, Command)                                                                            \
friend void w_connectToUpdates(ThisClass_t *self, w_number<W_COUNTER_##FieldName>)                                                      \
{                                                                                                                                       \
    connectToUpdates< W_COUNTER_##FieldName - 1 >(self, [](value_type *obj) { return wrap(PrivateClass::get(obj)->Command); },                \
                                                  &value_type::NotifySignal);                                                           \
    w_connectToUpdates(self, w_number< W_COUNTER_##FieldName - 1 >{});                                                                  \
}                                                                                                                                       \


#define DIRECT_ACCESS_METHOD(MethodName)                                                                                                \
template<typename ...Args> auto MethodName(Args &&...args) -> decltype(object->MethodName(args...))                                     \
{                                                                                                                                       \
    return object->MethodName(args...);                                                                                                 \
}                                                                                                                                       \


#define BLOCKING_ASYNC_METHOD(MethodName)                                                                                               \
template<typename ...Args> auto MethodName(Args &&...args) -> decltype(object->MethodName(args...))                                     \
{                                                                                                                                       \
    return call(object, &value_type::MethodName, args...).get();                                                                        \
}                                                                                                                                       \

#define ASYNC_VOID_METHOD(MethodName)                                                                                                   \
template<typename ...Args> void MethodName(Args &&...args)                                                                              \
{                                                                                                                                       \
    call(object, &value_type::MethodName, args...);                                                                                     \
}                                                                                                                                       \


#define DECLARE_OBJECT_WRAPPER(Class, ...)                                                                                              \
template<>                                                                                                                              \
class ObjectWrapper<Class> : public GammaRay::ObjectWrapperBase                                                                         \
{                                                                                                                                       \
private:                                                                                                                                \
    Class *object = nullptr;                                                                                                            \
                                                                                                                                        \
public:                                                                                                                                 \
    using value_type = Class;                                                                                                           \
    using ThisClass_t = ObjectWrapper<Class>;                                                                                           \
    friend std::tuple<> w_data(ObjectWrapper<Class> *, w_number<0>) { return {}; }                                                      \
    friend void w_connectToUpdates(ObjectWrapper<Class> *, w_number<0>) {}                                                              \
                                                                                                                                        \
    __VA_ARGS__;                                                                                                                        \
                                                                                                                                        \
    using ControlData = ControlBlock<Class, decltype( w_data(static_cast<ObjectWrapper<Class>*>(nullptr), w_number<255>()) )>;          \
                                                                                                                                        \
    explicit ObjectWrapper<Class>(Class *object)                                                                                        \
    : object(object)                                                                                                                    \
    {                                                                                                                                   \
        initialize<ObjectWrapper<Class>>(this);                                                                                         \
    }                                                                                                                                   \
    explicit ObjectWrapper<Class>(std::shared_ptr<ControlData> controlBlock)                                                            \
        : object(controlBlock->object)                                                                                                  \
        , m_control(std::move(controlBlock))                                                                                            \
    {}                                                                                                                                  \
    explicit ObjectWrapper<Class>() = default;                                                                                          \
                                                                                                                                        \
private:                                                                                                                                \
    friend class ObjectWrapperBase;                                                                                                     \
    friend class ObjectWrapperTest;                                                                                                     \
    friend class ObjectHandle<Class>;                                                                                                   \
    std::shared_ptr<ControlData> m_control;                                                                                             \
};                                                                                                                                      \



namespace GammaRay {


template<int N> struct w_number : public w_number<N - 1> {
    static constexpr int value = N;
    static constexpr w_number<N-1> prev() { return {}; }
};
// Specialize for 0 to break the recursion.
template<> struct w_number<0> { static constexpr int value = 0; };

template<typename Class> class ObjectWrapper {
public:
    using isDummyWrapper_t = void;
};

template<typename T1, typename T2> using second_t = T2;

class ObjectWrapperBase
{
protected:
    struct ControlBlockBase {};
    template<typename Class, typename Data_t>
    struct ControlBlock : public ControlBlockBase
    {
        Class *object;
        Data_t dataStorage;
        std::vector<QMetaObject::Connection> connections;
        QSemaphore semaphore;

        explicit ControlBlock(Class *object);
        explicit ControlBlock(Class *object, Data_t &&dataStorage);

        ~ControlBlock();
    };

    template<typename Derived_t>
    static void initialize(Derived_t *self);

    template<int storageIndex, typename Derived_t, typename CommandFunc_t>
    static void connectToUpdates(Derived_t *self, CommandFunc_t command, const char* propertyName);

    template<int storageIndex, typename Derived_t, typename CommandFunc_t, typename SignalFunc_t>
    static void connectToUpdates(Derived_t *self, CommandFunc_t command, SignalFunc_t signal);

    friend class ObjectShadowDataRepository;
};



template<typename T>
class ObjectHandle
{
public:
    using value_type = T;

    explicit ObjectHandle(T *obj);
    explicit ObjectHandle(std::shared_ptr<typename ObjectWrapper<T>::ControlData> controlBlock);
    explicit ObjectHandle() = default;

    explicit operator bool() const;
    explicit operator T*() const;

    inline const ObjectWrapper<T> *operator->() const;
    inline const ObjectWrapper<T> &operator*() const;
    inline ObjectWrapper<T> &operator*();

    inline T *object() const;

    template<typename Func, typename ...Args>
    auto call(Func &&f, Args &&...args) -> std::future<decltype(std::declval<T*>()->*f(args...))>;

private:
    ObjectWrapper<T> m_objectWrapper;
};

template<typename T>
class WeakObjectHandle
{
public:
    explicit WeakObjectHandle() = default;
    explicit WeakObjectHandle(std::weak_ptr<typename ObjectWrapper<T>::ControlData> controlBlock);
    explicit operator bool() const;

    static WeakObjectHandle nullhandle();

    inline ObjectHandle<T> lock() const;

    // TODO: Do we actually want implicit locking for WeakObjectHandle?
    inline const ObjectHandle<T> operator->() const;
    inline const ObjectWrapper<T> &operator*() const;
    inline ObjectWrapper<T> &operator*();
    inline T *object() const;

private:
    std::weak_ptr<typename ObjectWrapper<T>::ControlData> m_controlBlock;
};


class ObjectShadowDataRepository
{
public:
    static ObjectShadowDataRepository *instance();

    template<typename Class>
    static ObjectHandle<Class> handleForObject(Class *obj);

    template<typename Class>
    static WeakObjectHandle<Class> weakHandleForObject(Class *obj);

private:
    explicit ObjectShadowDataRepository() = default;
    friend class Probe;

    QHash<QObject*, std::weak_ptr<ObjectWrapperBase::ControlBlockBase>> m_objectToWrapperControlBlockMap;

    friend class ObjectWrapperBase;
    friend class ObjectWrapperTest;
    template<typename Class, typename Data_t>
    friend struct ObjectWrapperBase::ControlBlock;
};




// === ObjectWrapperBase ===


template<typename Derived_t>
void ObjectWrapperBase::initialize(Derived_t *self)
{
    if (!Probe::instance()->isValidObject(self->object)) {
        return;
    }
    self->m_control = std::make_shared<typename Derived_t::ControlData>(self->object);
    std::weak_ptr<ObjectWrapperBase::ControlBlockBase> controlWeakPtr { std::static_pointer_cast<ObjectWrapperBase::ControlBlockBase>(self->m_control) };
    ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.insert(self->object, controlWeakPtr );

    auto fetchData = [self]() mutable {
        QMutexLocker locker { Probe::objectLock() };
        QSemaphoreReleaser releaser { self->m_control->semaphore }; // releases the first (and only) resource
        self->m_control->dataStorage = w_data(self, w_number<255>());
    };

//     self->m_control->connections.push_back(QObject::connect(self->object, &QObject::destroyed, &));
    if (self->object->thread() == QThread::currentThread()) {
        fetchData();
    } else {
        QMetaObject::invokeMethod(self->object, fetchData, Qt::QueuedConnection);
    }
    w_connectToUpdates(self, w_number<255>{});
}

template<int storageIndex, typename Derived_t, typename CommandFunc_t>
void ObjectWrapperBase::connectToUpdates(Derived_t *self, CommandFunc_t command, const char* propertyName)
{
    auto mo = self->object->metaObject();
    auto prop = mo->property(mo->indexOfProperty(propertyName));

    if (!prop.hasNotifySignal()) {
        return;
    }

    auto controlPtr = std::weak_ptr<typename Derived_t::ControlData> {self->m_control};
    auto f = [controlPtr, command]() {
        std::cout << "Updating cache."<< storageIndex <<"\n";
        QMutexLocker locker { Probe::objectLock() };
        auto control = controlPtr.lock();
        control->semaphore.acquire();
        QSemaphoreReleaser releaser { control->semaphore };
        std::get< storageIndex >(control->dataStorage) = command(control->object);
    };

    auto connection = QObjectPrivate::connect(self->object,
                                                prop.notifySignal().methodIndex(),
                                                new QtPrivate::QFunctorSlotObjectWithNoArgs<decltype(f), void>(std::move(f)),
                                                Qt::DirectConnection
    );

    self->m_control->connections.push_back(connection);
}


template<int storageIndex, typename Derived_t, typename CommandFunc_t, typename SignalFunc_t>
void ObjectWrapperBase::connectToUpdates(Derived_t *self, CommandFunc_t command, SignalFunc_t signal)
{
    auto controlPtr = std::weak_ptr<typename Derived_t::ControlData> {self->m_control};
    auto f = [controlPtr, command]() {
        std::cout << "Updating cache."<< storageIndex <<"\n";
        QMutexLocker locker { Probe::objectLock() };
        auto control = controlPtr.lock();
        control->semaphore.acquire();
        QSemaphoreReleaser releaser { control->semaphore };
        std::get< storageIndex >(control->dataStorage) = command(control->object);
    };

    auto connection = QObject::connect(self->object, signal, f);

    self->m_control->connections.push_back(connection);
}


// === ObjectHandle ===

template<typename T>
ObjectHandle<T>::ObjectHandle(T *obj)
    : m_objectWrapper(obj)
{}

template<typename T>
ObjectHandle<T>::ObjectHandle(std::shared_ptr<typename ObjectWrapper<T>::ControlData> controlBlock)
    : m_objectWrapper(std::move(controlBlock))
{}

template<typename T>
ObjectHandle<T>::operator bool() const
{
    return Probe::instance()->isValidObject(m_objectWrapper->object);
}

template<typename T>
ObjectHandle<T>::operator T*() const
{
    return m_objectWrapper->object;
}

template<typename T>
const ObjectWrapper<T> *ObjectHandle<T>::operator->() const
{
    return &m_objectWrapper;
}

template<typename T>
const ObjectWrapper<T> &ObjectHandle<T>::operator*() const
{
    return m_objectWrapper;
}

template<typename T>
ObjectWrapper<T> &ObjectHandle<T>::operator*()
{
    return m_objectWrapper;
}

template<typename T>
T *ObjectHandle<T>::object() const
{
    return m_objectWrapper.object;
}



template<typename T>
template<typename Func, typename ...Args>
auto ObjectHandle<T>::call(Func &&f, Args &&...args) -> std::future<decltype(std::declval<T*>()->*f(args...))>
{
    if (!Probe::instance()->isValidObject(m_objectWrapper->object)) {
        return {};
    }

    std::promise<decltype(m_objectWrapper->object->*f(args...))> p;
    auto future = p.get_future();
    if (m_objectWrapper->object->thread == QThread::currentThread()) {
        p.set_value(m_objectWrapper->object->*f(args...));
    } else {
        T *ptr = m_objectWrapper->object;
        QMetaObject::invokeMethod(m_objectWrapper->object, [p, ptr, f, args...]() {
            p.set_value(ptr->*f(args...));
        }, Qt::QueuedConnection);
    }
    return future;
}

// === WeakObjectHandle ===

template<typename T>
WeakObjectHandle<T>::WeakObjectHandle(std::weak_ptr<typename ObjectWrapper<T>::ControlData> controlBlock)
: m_controlBlock(std::move(controlBlock))
{}

template<typename T>
WeakObjectHandle<T>::operator bool() const
{
    return !m_controlBlock.expired() && Probe::instance()->isValidObject(m_controlBlock.lock()->object); // FIXME we should not need to lock this just to do a null check
}

template<typename T>
ObjectHandle<T> WeakObjectHandle<T>::lock() const
{
    return ObjectHandle<T> { m_controlBlock.lock() };
}
template<typename T>
WeakObjectHandle<T> WeakObjectHandle<T>::nullhandle()
{
    return WeakObjectHandle<T> { std::weak_ptr<typename ObjectWrapper<T>::ControlData>{} };
}

template<typename T>
const ObjectHandle<T> WeakObjectHandle<T>::operator->() const
{
    return lock();
}

template<typename T>
const ObjectWrapper<T> &WeakObjectHandle<T>::operator*() const
{
    return *lock();
}

template<typename T>
ObjectWrapper<T> &WeakObjectHandle<T>::operator*()
{
    return *lock();
}

template<typename T>
T *WeakObjectHandle<T>::object() const
{
    return m_controlBlock.lock()->object;
}

// === ObjectShadowDataRepository ===

ObjectShadowDataRepository *ObjectShadowDataRepository::instance()
{
//     return Probe::instance()->objectShadowDataRepository();
    static ObjectShadowDataRepository *self = new ObjectShadowDataRepository();
    return self;
}

template<typename Class>
ObjectHandle<Class> ObjectShadowDataRepository::handleForObject(Class *obj)
{
    auto self = instance();

    if (self->m_objectToWrapperControlBlockMap.contains(obj)) {
        auto controlBasePtr = self->m_objectToWrapperControlBlockMap.value(obj).lock();
        auto controlPtr = std::static_pointer_cast<typename ObjectWrapper<Class>::ControlData>(controlBasePtr);
        return ObjectHandle<Class> { std::move(controlPtr) };
    } else {
        return ObjectHandle<Class> { obj }; // will insert itself into the map
    }
}

template<typename Class>
WeakObjectHandle<Class> ObjectShadowDataRepository::weakHandleForObject(Class *obj)
{
    auto self = instance();

    if (!self->m_objectToWrapperControlBlockMap.contains(obj)) {
        ObjectHandle<Class> handle { obj }; // will insert itself into the map
        // FIXME: This is the only strong handle, i.e. control block will be deleted immediately...
    }

    auto controlBasePtr = self->m_objectToWrapperControlBlockMap.value(obj).lock();
    std::weak_ptr<typename ObjectWrapper<Class>::ControlData> controlPtr =
        std::static_pointer_cast<typename ObjectWrapper<Class>::ControlData>(controlBasePtr);
    return WeakObjectHandle<Class> { std::move(controlPtr) };
}

template<typename Class, typename Data_t>
ObjectWrapperBase::ControlBlock<Class, Data_t>::ControlBlock(Class *obj, Data_t &&dataStorage)
    : object(obj)
    , dataStorage(dataStorage)
{
    ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.insert(object, this);
}

template<typename Class, typename Data_t>
ObjectWrapperBase::ControlBlock<Class, Data_t>::ControlBlock(Class *obj)
: object(obj)
{
}


template<typename Class, typename Data_t>
ObjectWrapperBase::ControlBlock<Class, Data_t>::~ControlBlock()
{
    for (auto &&c : connections) {
        QObject::disconnect(c);
    }
    ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.remove(object);
}


template<typename T>
auto wrap(T &&value) -> second_t<typename ObjectWrapper<T>::isDummyWrapper_t, T>
{
    return std::forward<T>(value);
}
template<typename T>
auto wrap(T *object) -> second_t<typename ObjectWrapper<T>::value_type, WeakObjectHandle<T>>
{
    return ObjectShadowDataRepository::weakHandleForObject(object);
}
template<typename T>
auto wrap(const QList<T*> &list) -> second_t<typename ObjectWrapper<T>::value_type, QList<WeakObjectHandle<T>>>
{
    QList<WeakObjectHandle<T>> handleList;
    std::transform(list.cBegin(), list.cEnd(), handleList.begin(), [](T *t) { return ObjectShadowDataRepository::weakHandleForObject(t); });
    return handleList;
}
template<typename T>
auto wrap(QVector<T*> list) -> second_t<typename ObjectWrapper<T>::value_type, QVector<WeakObjectHandle<T>>>
{
    QVector<WeakObjectHandle<T>> handleList;
    handleList.reserve(list.size());
    for (T *t : qAsConst(list)) {
        handleList.push_back(ObjectShadowDataRepository::weakHandleForObject(t));
    }
    //     std::transform(list.cbegin(), list.cend(), handleList.begin(), [](T *t) { return WeakObjectHandle<T> { t }; });
    return handleList;
}

}



//TODO: calling setters, access through d-ptr, QObject-properties as ObjectWrappers (also object-list-properties), central data cache, meta object, updates
//TODO: Check threading


// TODO: Revise shared_ptr-model: we'd actually need GC, because the handles reference each other.
// TODO: Delete the handles when the QObject is destroyed => connect to destroyed-signal => maybe we don't actually need shared-pointers at all?

#endif // GAMMARAY_OBJECTHANDLE_H
