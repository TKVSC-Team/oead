/**
 * Copyright (C) 2020 leoetlino
 *
 * This file is part of oead.
 *
 * oead is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * oead is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with oead.  If not, see <http://www.gnu.org/licenses/>.
 */

// To be included in every translation unit.

#pragma once

#include <nonstd/span.h>
#include <optional>
#include <vector>

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <oead/types.h>
#include <oead/util/scope_guard.h>
#include "pybind11_variant_caster.h"

namespace py = pybind11;
using namespace py::literals;

#define OEAD_MAKE_OPAQUE(NAME, ...)                                                                \
  namespace pybind11::detail {                                                                     \
  template <>                                                                                      \
  class type_caster<__VA_ARGS__> : public type_caster_base<__VA_ARGS__> {                          \
  public:                                                                                          \
    static constexpr auto name = _(NAME);                                                          \
  };                                                                                               \
  }

#define OEAD_MAKE_VARIANT_CASTER(...)                                                              \
  namespace pybind11::detail {                                                                     \
  template <>                                                                                      \
  struct type_caster<__VA_ARGS__::Storage> : oead_variant_caster<__VA_ARGS__::Storage> {};         \
  template <>                                                                                      \
  struct type_caster<__VA_ARGS__> : oead_variant_wrapper_caster<__VA_ARGS__> {};                   \
  }

namespace pybind11::detail {
template <typename T>
constexpr auto OeadGetSpanCasterName() {
  if constexpr (std::is_same_v<std::decay_t<T>, u8>)
    return _("BytesLike");
  return _("Span[") + detail::concat(make_caster<T>::name) + _("]");
}

template <typename T>
struct type_caster<tcb::span<T>> {
  static handle cast(tcb::span<T> span, return_value_policy, handle) {
    return py::memoryview::from_memory(span.data(), ssize_t(span.size_bytes())).release();
  }

  bool load(handle src, bool) {
    const py::buffer_info buffer = src.cast<py::buffer>().request(!std::is_const_v<T>);
    if (buffer.itemsize != sizeof(T) || buffer.ndim != 1)
      return false;
    value = {static_cast<T*>(buffer.ptr), size_t(buffer.size)};
    return true;
  }

  PYBIND11_TYPE_CASTER(tcb::span<T>, OeadGetSpanCasterName<T>());
};
}  // namespace pybind11::detail

namespace oead::bind {
inline tcb::span<u8> PyBytesToSpan(py::bytes b) {
  return {reinterpret_cast<u8*>(PYBIND11_BYTES_AS_STRING(b.ptr())),
          size_t(PYBIND11_BYTES_SIZE(b.ptr()))};
}

template <typename Vector, typename holder_type = std::unique_ptr<Vector>, typename... Args>
py::class_<Vector, holder_type> BindVector(py::handle scope, const std::string& name,
                                           Args&&... args) {
  using Value = typename Vector::value_type;
  auto cl = py::bind_vector<Vector, holder_type>(scope, name, std::forward<Args>(args)...);
  cl.def(py::self == py::self);
  py::implicitly_convertible<py::list, Vector>();
  return cl;
}

template <typename Map, typename Key, typename CastFn>
static Map MapFromIter(py::iterator it, CastFn cast_value) {
  Map map;
  while (it != py::iterator::sentinel()) {
    auto pair = py::cast<std::pair<py::handle, py::handle>>(*it);
    map.emplace(pair.first.cast<Key>(), cast_value(pair.second));
    ++it;
  }
  return map;
}

template <typename Map, typename Key, typename CastFn>
static Map MapFromDict(py::dict dict, CastFn cast_value) {
  Map map;
  for (std::pair<py::handle, py::handle> pair : dict)
    map.emplace(pair.first.cast<Key>(), cast_value(pair.second));
  return map;
}

template <typename Map, typename Key, typename Value>
static Value MapCastValue(py::handle handle) {
  if constexpr (std::is_convertible<Map, Value>()) {
    if (py::isinstance<py::dict>(handle))
      return MapFromDict<Map, Key>(handle.cast<py::dict>(), MapCastValue<Map, Key, Value>);
    if (py::isinstance<py::iterator>(handle))
      return MapFromIter<Map, Key>(handle.cast<py::iterator>(), MapCastValue<Map, Key, Value>);
  }
  return handle.cast<Value>();
}

template <typename T, typename = void>
struct has_value_method : std::false_type {};

template <typename T>
struct has_value_method<T, std::void_t<decltype(std::declval<T&>().value())>> : std::true_type {};

template <typename Iterator>
decltype(auto) GetIteratorValue(Iterator& it) {
  if constexpr (has_value_method<Iterator>::value) {
    return it.value();
  } else {
    return it->second;
  }
}

template <typename Map, typename holder_type = std::unique_ptr<Map>, typename... Args>
py::class_<Map, holder_type> OeadBindMap(py::handle scope, const std::string &name, Args &&...args) {
    using KeyType = typename Map::key_type;
    using MappedType = typename Map::mapped_type;
    using KeysView = py::detail::keys_view;
    using ValuesView = py::detail::values_view;
    using ItemsView = py::detail::items_view;
    using Class_ = py::class_<Map, holder_type>;

    auto *tinfo = py::detail::get_type_info(typeid(MappedType));
    bool local = !tinfo || tinfo->module_local;
    if (local) {
        tinfo = py::detail::get_type_info(typeid(KeyType));
        local = !tinfo || tinfo->module_local;
    }

    Class_ cl(scope, name.c_str(), py::module_local(local), std::forward<Args>(args)...);

    if (!py::detail::get_type_info(typeid(KeysView))) {
        py::class_<KeysView> keys_view(scope, "KeysView", py::module_local(local));
        keys_view.def("__len__", &KeysView::len);
        keys_view.def("__iter__", &KeysView::iter, py::keep_alive<0, 1>());
        keys_view.def("__contains__", &KeysView::contains);
    }
    if (!py::detail::get_type_info(typeid(ValuesView))) {
        py::class_<ValuesView> values_view(scope, "ValuesView", py::module_local(local));
        values_view.def("__len__", &ValuesView::len);
        values_view.def("__iter__", &ValuesView::iter, py::keep_alive<0, 1>());
    }
    if (!py::detail::get_type_info(typeid(ItemsView))) {
        py::class_<ItemsView> items_view(scope, "ItemsView", py::module_local(local));
        items_view.def("__len__", &ItemsView::len);
        items_view.def("__iter__", &ItemsView::iter, py::keep_alive<0, 1>());
    }

    cl.def(py::init<>());

    py::detail::map_if_insertion_operator<Map, Class_>(cl, name);

    cl.def(
        "__bool__",
        [](const Map &m) -> bool { return !m.empty(); },
        "Check whether the map is nonempty");

    cl.def(
        "__iter__",
        [](Map &m) { return py::make_key_iterator(m.begin(), m.end()); },
        py::keep_alive<0, 1>()
    );

    cl.def(
        "keys",
        [](Map &m) { return std::unique_ptr<KeysView>(new py::detail::KeysViewImpl<Map>(m)); },
        py::keep_alive<0, 1>()
    );

    cl.def(
        "values",
        [](Map &m) { return std::unique_ptr<ValuesView>(new py::detail::ValuesViewImpl<Map>(m)); },
        py::keep_alive<0, 1>()
    );

    cl.def(
        "items",
        [](Map &m) { return std::unique_ptr<ItemsView>(new py::detail::ItemsViewImpl<Map>(m)); },
        py::keep_alive<0, 1>()
    );

    cl.def(
        "__getitem__",
        [](Map &m, const KeyType &k) -> decltype(auto) {
            auto it = m.find(k);
            if (it == m.end()) {
                throw py::key_error();
            }
            return GetIteratorValue(it);
        },
        py::return_value_policy::reference_internal
    );

    cl.def("__contains__", [](Map &m, const KeyType &k) -> bool {
        auto it = m.find(k);
        return it != m.end();
    });
    cl.def("__contains__", [](Map &, const py::object &) -> bool { return false; });

    cl.def("__setitem__", [](Map &m, const KeyType &k, const MappedType &v) {
        m.insert_or_assign(k, v);
    });

    cl.def("__delitem__", [](Map &m, const KeyType &k) {
        auto it = m.find(k);
        if (it == m.end()) {
            throw py::key_error();
        }
        m.erase(it);
    });

    cl.def("__len__", [](const Map &m) { return m.size(); });

    return cl;
}

template <typename Map, typename holder_type = std::unique_ptr<Map>, typename... Args>
py::class_<Map, holder_type> BindMap(py::handle scope, const std::string& name, Args&&... args) {
  using Key = typename Map::key_type;
  using Value = typename Map::mapped_type;
  auto cl = OeadBindMap<Map, holder_type>(scope, name, std::forward<Args>(args)...)
                .def(py::init([&](py::iterator it) {
                       return MapFromIter<Map, Key>(it, MapCastValue<Map, Key, Value>);
                     }),
                     "iterator"_a)
                .def(py::init([&](py::dict dict) {
                       return MapFromDict<Map, Key>(dict, MapCastValue<Map, Key, Value>);
                     }),
                     "dictionary"_a)
                .def(py::self == py::self)
                .def(
                    "__contains__",
                    [](const Map& map, const py::object& arg) {
                      try {
                        auto key = py::cast<Key>(arg);
                        return map.find(key) != map.end();
                      } catch (const py::cast_error&) {
                        return false;
                      }
                    },
                    py::prepend{})
                .def("clear", &Map::clear)
                .def(
                    "get",
                    [](const Map& map, const Key& key,
                       std::optional<py::object> default_value) -> std::variant<py::object, Value> {
                      const auto it = map.find(key);
                      if (it == map.cend()) {
                        if (default_value)
                          return *default_value;
                        throw py::key_error();
                      }
                      return GetIteratorValue(it);
                    },
                    "key"_a, "default"_a = std::nullopt, py::keep_alive<0, 1>());
  py::implicitly_convertible<py::dict, Map>();
  return cl;
}
}  // namespace oead::bind

OEAD_MAKE_OPAQUE("oead.Bytes", std::vector<u8>);
OEAD_MAKE_OPAQUE("oead.BufferInt", std::vector<int>);
OEAD_MAKE_OPAQUE("oead.BufferF32", std::vector<f32>);
OEAD_MAKE_OPAQUE("oead.BufferU32", std::vector<u32>);
OEAD_MAKE_OPAQUE("oead.BufferBool", std::vector<bool>);
OEAD_MAKE_OPAQUE("oead.BufferString", std::vector<std::string>);
