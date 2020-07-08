{{warning}}

#pragma once

#include <cereal/cereal.hpp>

#include <cereal/types/polymorphic.hpp>

#include <cereal/types/map.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/string.hpp>

#include <glm/glm.hpp>

#include <entt/entt.hpp>

{%- for file in files %}
#include "{{file}}"
{%- endfor %}

#include <string>
#include <unordered_map>
#include <cassert>

// custom class relations should be registered out of cereal namespace

{%- for type in classes.values() %}
{%- if type.parents %}
//CEREAL_REGISTER_TYPE_WITH_NAME({{type.name}}, "{{type.name}}");
{% for parent in type.parents -%}
//CEREAL_REGISTER_POLYMORPHIC_RELATION({{parent}}, {{type.name}});
{%- endfor %}
{%- endif %}
{%- endfor %}

namespace cereal {

class JSONInputArchive;
class JSONOutputArchive;

// custom enums

{% for enum in enums.values() -%}

template<typename Archive>
void load(Archive& archive, {{enum.name}}& object) {
    static const std::unordered_map<std::string, {{enum.name}}> Values = {
        {%- for value in enum['values'] %}
        { /*{{enum.name}}::*/"{{value}}", {{enum.name}}::{{value}} },
        {%- endfor %}
    };

    std::string value;
    archive(value);
    auto iter = Values.find(value);
    assert(iter != Values.cend());
    if (iter != Values.cend()) {
        object = iter->second;
    }
}

template<typename Archive>
void save(Archive& archive, const {{enum.name}}& object) {
    static const std::unordered_map<{{enum.name}}, std::string> Values = {
        {%- for value in enum['values'] %}
        { {{enum.name}}::{{value}}, /*{{enum.name}}::*/"{{value}}" },
        {%- endfor %}
    };

    auto iter = Values.find(object);
    assert(iter != Values.cend());
    std::string value = iter != Values.cend() ? iter->second : "";
    archive(value);
}

{% endfor -%}

// custom classes

{% for type in classes.values() -%}

template<typename Archive>
void serialize(Archive& archive, {{type.name}}& object) {
    {%- if type.attributes %}
    archive(
        {% for attribute in type.attributes -%}
        ::cereal::make_nvp("{{attribute.name}}", object.{{attribute.name}})
        {%- if loop.index < type.attributes|count %},{% endif %}
        {% endfor -%}
    );
    {%- else %}
    // structure "{{type.name}}" contains no serializable attributes
    static_cast<void>(archive);
    static_cast<void>(object);
    {%- endif %}
}

{% endfor -%}

// glm

template<typename Archive>
using WriteMathObjectsAsArrays = std::bool_constant<std::is_base_of_v<cereal::JSONInputArchive, Archive> || std::is_base_of_v<cereal::JSONOutputArchive, Archive>>;

template<typename Archive, glm::length_t S, typename T, glm::qualifier Q>
void serialize(Archive& archive, glm::vec<S, T, Q>& object) {

    if constexpr (WriteMathObjectsAsArrays<Archive>::value) {
        cereal::size_type size = S;
        archive(cereal::make_size_tag(size));
        assert(size == S);

        for (size_t i = 0; i < S; ++i) {
            archive(object[i]);
        }
    }
    else {
        static constexpr const char* NAMING[] = {
            "x", "y", "z", "w",
        };

        for (size_t i = 0; i < S; ++i) {
            archive(::cereal::make_nvp(NAMING[i], object[i]));
        }
    }

}

template<typename Archive, glm::length_t R, glm::length_t C, typename T, glm::qualifier Q>
void serialize(Archive& archive, glm::mat<R, C, T, Q>& object) {

    if constexpr (WriteMathObjectsAsArrays<Archive>::value) {
        constexpr glm::length_t S = R * C;
        cereal::size_type s = S;
        archive(cereal::make_size_tag(s));
        assert(s == S);

        for (size_t i = 0; i < R; ++i) {
            for (size_t j = 0; j < C; ++j) {
                archive(object[i][j]);
            }
        }
    }
    else {
        static constexpr const char* NAMING[] = {
            "m[0][0]", "m[0][1]", "m[0][2]", "m[0][3]",
            "m[1][0]", "m[1][1]", "m[1][2]", "m[1][3]",
            "m[2][0]", "m[2][1]", "m[2][2]", "m[2][3]",
            "m[3][0]", "m[3][1]", "m[3][2]", "m[3][3]",
        };

        for (size_t i = 0; i < R; ++i) {
            for (size_t j = 0; j < C; ++j) {
                archive(::cereal::make_nvp(NAMING[i * 4 + j], object[i][j]));
            }
        }
    }

}

// entt

template<typename Component, typename Archive, typename Entity>
void load_entt_component(Archive& archive, entt::basic_registry<Entity>& registry, Entity entity, const char* const* name, size_t& name_index) {

    std::optional<Component> component;

    if (name) {
        archive(::cereal::make_nvp(name[name_index++], component));
    }
    else {
        archive(component);
    }

    if (component) {
        registry.emplace_or_replace<Component>(entity, std::move(*component));
    }
}

template<typename Component, typename Archive, typename Entity>
void save_entt_component(Archive& archive, entt::basic_registry<Entity>& registry, Entity entity, const char* const* name, size_t& name_index) {

    std::optional<Component> component;

    if (registry.has<Component>(entity)) {
        component = registry.get<Component>(entity);
    }

    if (name) {
        archive(::cereal::make_nvp(name[name_index++], component));
    }
    else {
        archive(component);
    }
}

template<typename... Components, typename Archive, typename Entity>
void load_entt_components(Archive& archive, entt::basic_registry<Entity>& registry, Entity entity, const std::array<const char*, sizeof...(Components)>& names) {
    size_t i = 0;
    ( load_entt_component<Components>(archive, registry, entity, names.data(), i), ... );
}

template<typename... Components, typename Archive, typename Entity>
void save_entt_components(Archive& archive, entt::basic_registry<Entity>& registry, Entity entity, const std::array<const char*, sizeof...(Components)>& names) {
    size_t i = 0;
    ( save_entt_component<Components>(archive, registry, entity, names.data(), i), ... );
}

template<typename Entity, typename... Components>
struct EntityWrapper {
    entt::basic_registry<Entity>& registry;
    const std::array<const char*, sizeof...(Components)>& names;
    Entity entity;
};

template<typename Archive, typename Entity, typename... Components>
void load(Archive& archive, EntityWrapper<Entity, Components...>& entity_wrapper) {
    load_entt_components<Components...>(archive, entity_wrapper.registry, entity_wrapper.entity, entity_wrapper.names);
}

template<typename Archive, typename Entity, typename... Components>
void save(Archive& archive, const EntityWrapper<Entity, Components...>& entity_wrapper) {
    save_entt_components<Components...>(archive, entity_wrapper.registry, entity_wrapper.entity, entity_wrapper.names);
}

template<typename Entity, typename... Components>
struct RegistryWrapper {
    entt::basic_registry<Entity>& registry;
    const std::array<const char*, sizeof...(Components)> names;
};

template<typename Archive, typename Entity, typename... Components>
void load(Archive& archive, RegistryWrapper<Entity, Components...>& registry_wrapper) {

    size_t count = 0;
    archive(::cereal::make_size_tag(count));

    while (count > 0) {
        archive(EntityWrapper<Entity, Components...>{registry_wrapper.registry, registry_wrapper.names, registry_wrapper.registry.create()});
        --count;
    }
}

template<typename Archive, typename Entity, typename... Components>
void save(Archive& archive, const RegistryWrapper<Entity, Components...>& registry_wrapper) {

    size_t count = 0;

    registry_wrapper.registry.each([&count, &registry_wrapper](Entity entity) {
        if (registry_wrapper.registry.template any<Components...>(entity)) {
            ++count;
        }
    });

    archive(::cereal::make_size_tag(count));

    registry_wrapper.registry.each([&archive, &registry_wrapper](Entity entity) {
        if (registry_wrapper.registry.template any<Components...>(entity)) {
            archive(EntityWrapper<Entity, Components...>{registry_wrapper.registry, registry_wrapper.names, entity});
        }
    });
}

template<typename... Components, typename Archive, typename Entity>
void serialize_world(Archive& archive, entt::basic_registry<Entity>& registry, const std::array<const char*, sizeof...(Components)>& names) {
    archive(::cereal::make_nvp("world", RegistryWrapper<Entity, Components...>{registry, names}));
}

template<typename... Components, typename Archive, typename Entity>
void serialize_world(Archive& archive, entt::basic_registry<Entity>& registry) {
    std::array<const char*, sizeof...(Components)> names;
    std::fill(names.begin(), names.end(), nullptr);
    archive(::cereal::make_nvp("world", RegistryWrapper<Entity, Components...>{registry, names}));
}

} // namespace cereal
