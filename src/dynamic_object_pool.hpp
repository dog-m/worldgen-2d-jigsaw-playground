#pragma once

#include <list>
#include <array>
#include <memory>

namespace misc
{

    template <class Obj, size_t GroupSize = 10>
    class ObjectPoolDynamic
    {
    private:
        //#pragma pack(push, 1)
        struct Element
        {
            volatile uint8_t inactive = true;
            Obj obj;
        };
        //#pragma pack(pop)

        using ElementRef = Element *;
        using Group = std::array<Element, GroupSize>;

        std::list<Group> groups;

    public:
        using ObjRef = Obj *;

        inline size_t allocated() const noexcept
        {
            return GroupSize * groups.size();
        }

        ObjRef get()
        {
            ElementRef candidate = nullptr;

            // find unused element
            for (auto &group : groups)
                for (auto &element : group)
                    if (element.inactive)
                    {
                        candidate = &element;
                        break;
                    }

            // no unused elements left?
            if (!candidate)
            {
                auto &group = groups.emplace_back();
                candidate = std::addressof(group[0]);
            }

            // activate it
            candidate->inactive = false;

            // reply with just the reference to the object
            return &candidate->obj;
        }

        inline void release(ObjRef const obj)
        {
            auto ptr = reinterpret_cast<decltype(Element::inactive) *>(obj);

            // WARNING: this field should be just above the object data
            *(ptr - offsetof(Element, obj) + offsetof(Element, inactive)) = true;
        }

        auto get_scoped()
        {
            class ScopeManagedObjectReference
            {
            private:
                ObjectPoolDynamic *const pool;
                ObjRef const ref;

            public:
                ScopeManagedObjectReference(ObjectPoolDynamic *const p)
                    : pool(p), ref(p->get()) {}

                ~ScopeManagedObjectReference()
                {
                    pool->release(ref);
                }

                inline ObjRef get() const noexcept
                {
                    return ref;
                }

                inline ObjRef operator->() const noexcept
                {
                    return ref;
                }
            };

            return ScopeManagedObjectReference(this);
        }
    };

} // namespace misc
