#pragma once
#include <functional>

template <typename T>
class Property
{
public:
    using Getter = std::function<T()>;
    using Setter = std::function<void(const T&)>;

    Property(T& value)
        : m_getter([&value]() { return value; })
        , m_setter([&value](const T& val) { value = val; })
    {}

    Property(Getter getter, Setter setter)
        : m_getter(std::move(getter))
        , m_setter(std::move(setter))
    {}

    Property(T& value, Getter getter)
        : m_getter(std::move(getter))
        , m_setter([&value](const T& val) { value = val; })
    {}

    Property(T& value, Setter setter)
        : m_getter([&value]() { return value; })
        , m_setter(std::move(setter))
    {}

    Property(const Property& other) = delete;
    Property(Property&& other) noexcept = delete;

    Property& operator=(const Property& other) { m_setter(other); return *this; }
    Property& operator=(Property&& other) noexcept { m_setter(std::move(other)); return *this; }
    Property& operator=(const T& value) { m_setter(value); return *this; }
    Property& operator=(T&& value) noexcept { m_setter(std::move(value)); return *this; }
    operator T() const { return m_getter(); }

private:
    Getter m_getter;
    Setter m_setter;
};

template <typename T>
class ReadOnlyProperty
{
public:
    using Getter = typename Property<T>::Getter;

    ReadOnlyProperty(T& value)
        : m_getter([&value]() { return value; })
    {}

    ReadOnlyProperty(Getter getter)
        : m_getter(std::move(getter))
    {}

    ReadOnlyProperty(const ReadOnlyProperty& other) = delete;
    ReadOnlyProperty(ReadOnlyProperty&& other) noexcept = delete;
    ReadOnlyProperty& operator=(const ReadOnlyProperty& other) = delete;
    ReadOnlyProperty& operator=(ReadOnlyProperty&& other) noexcept = delete;

    operator T() const { return m_getter(); }

private:
    Getter m_getter;
};

template <typename T>
class WriteOnlyProperty
{
public:
    using Setter = typename Property<T>::Setter;

    WriteOnlyProperty(T& value)
        : m_setter([&value](const T& val) { value = val; })
    {}

    WriteOnlyProperty(Setter setter)
        : m_setter(std::move(setter))
    {}

    WriteOnlyProperty(const WriteOnlyProperty& other) = delete;
    WriteOnlyProperty(WriteOnlyProperty&& other) noexcept = delete;
    WriteOnlyProperty& operator=(const WriteOnlyProperty& other) = delete;
    WriteOnlyProperty& operator=(WriteOnlyProperty&& other) noexcept = delete;

    WriteOnlyProperty& operator=(const Property<T>& other) { m_setter(other); return *this; }
    WriteOnlyProperty& operator=(Property<T>&& other) noexcept { m_setter(std::move(other)); return *this; }
    WriteOnlyProperty& operator=(const ReadOnlyProperty<T>& other) { m_setter(other); return *this; }
    WriteOnlyProperty& operator=(ReadOnlyProperty<T>&& other) noexcept { m_setter(std::move(other)); return *this; }
    WriteOnlyProperty& operator=(const T& value) { m_setter(value); return *this; }
    WriteOnlyProperty& operator=(T&& value) noexcept { m_setter(std::move(value)); return *this; }

private:
    Setter m_setter;
};