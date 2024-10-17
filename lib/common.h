#ifndef COMMON_H
#define COMMON_H

#define auto_var(name, value__, ...) __VA_ARGS__ typeof(value__) name = (value__)

#define min(a, b) ({\
    auto_var(min_a, (a));\
    auto_var(min_b, (b));\
    min_a < min_b ? min_a : min_b;\
})

#define max(a, b) ({\
    auto_var(max_a, (a));\
    auto_var(max_b, (b));\
    max_a > max_b ? max_a : max_b;\
})

#define clamp(value__, min__, max__) min(max((value__), (min__)), (max__))

#define container_of(ptr__, type__, member__) ((type__*) ((intptr_t)(ptr__) - offsetof(type__, member__)))

#define void_expression() ({;})

#ifndef typeof_member
#   define typeof_member(type__, member__) typeof(((type__*)0)->member__)
#endif

#define ARRAY_LITERAL(size__, ...) (uint8_t[(size__)]){ __VA_ARGS__ }

#endif //COMMON_H
