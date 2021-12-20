// SdkGenny - Genny.hpp - A single file header framework for generating C++ compatible SDKs
// https://github.com/cursey/sdkgenny
// GennyIda.hpp is an optional extra for SdkGenny that generates output intended to be consumed by IDA.

#pragma once

#include "Genny.hpp"

namespace genny::ida {
inline auto default_usable_name = [](const std::string& s) { return s; };

// Does a destructive transformation to the Sdk to make it's output parsable by IDA.
inline void transform(Sdk& sdk, std::function<std::string(const std::string&)> usable_name = default_usable_name) {
    auto g = sdk.global_ns();
    std::unordered_set<Type*> types{};
    std::unordered_set<EnumClass*> enum_classes{};

    // Make plain enum types for all enum classes.
    g->get_all_in_children<EnumClass>(enum_classes);

    // We have to remove the enum class from it's owner before we can add a normal enum with the same name to it.
    // Variables may still reference the enum class however so we must keep them alive until all variables have had
    // their types changed to the normal enum versions.
    std::vector<std::unique_ptr<Object>> enumclass_keepalive{};

    for (auto& e : enum_classes) {
        Enum* new_enum{};
        auto owner = e->direct_owner();

        if (auto ns_owner = dynamic_cast<Namespace*>(owner)) {
            enumclass_keepalive.emplace_back(ns_owner->remove(e));
            new_enum = ns_owner->enum_(e->name());
        } else if (auto owner_struct = dynamic_cast<Struct*>(owner)) {
            enumclass_keepalive.emplace_back(owner_struct->remove(e));
            new_enum = owner_struct->enum_(e->name());
        } else {
            continue;
        }

        for (auto&& [name, value] : e->values()) {
            new_enum->value(name, value);
        }

        new_enum->type(e->type());
    }

    g->get_all_in_children<Type>(types);

    for (auto&& t : types) {
        if (!t->is_a<Struct>() && !t->is_a<Enum>()) {
            continue;
        }

        auto owners = t->owners<Object>();
        std::string new_name = t->name();

        for (auto&& owner : owners) {
            if (!owner->name().empty()) {
                new_name = owner->name() + "::" + new_name;
            }
        }

        new_name = usable_name(new_name);

        t->usable_name = [new_name] { return new_name; };

        if (!t->direct_owner()->is_a<Struct>()) {
            t->usable_name_decl = t->usable_name;
        }

        t->simple_typename_generation(true);
        t->remove_all<Function>();
        t->remove_all<Constant>();

        // Convert all enum classes to normal enums
        for (auto&& v : t->get_all<Variable>()) {
            auto v_t = v->type();

            if (!v_t->is_a<EnumClass>()) {
                continue;
            }

            auto owner = v_t->direct_owner();

            if (auto owner_ns = dynamic_cast<Namespace*>(owner)) {
                v->type(owner_ns->enum_(v_t->name()));
            } else if (auto owner_struct = dynamic_cast<Struct*>(owner)) {
                v->type(owner_struct->enum_(v_t->name()));
            }
        }
    }

    sdk.generate_namespaces(false);
}

} // namespace genny::ida
