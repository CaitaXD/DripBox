#ifndef PREPROCESSOR_CALCULUS_H
#define PREPROCESSOR_CALCULUS_H

#define sum(...) (OPERATOR(+, 0, __VA_ARGS__))
#define multiply(...) (OPERATOR(*, 0, __VA_ARGS__))
#define subtract(...) (OPERATOR(-, 0, __VA_ARGS__))
#define divide(...) (OPERATOR(/, 1, __VA_ARGS__))
#define max(...) FOLD(max2, __VA_ARGS__)
#define min(...) FOLD(min2,  __VA_ARGS__)
#define mean(...) (sum(__VA_ARGS__)/ARGS_COUNT(__VA_ARGS__))

#define MAP_ARGS0(fn, dummy)
#define MAP_ARGS1(fn, a) fn(a)
#define MAP_ARGS2(fn, a, b) fn(a), fn(b)
#define MAP_ARGS3(fn, a, b, c) fn(a), fn(b), fn(c)
#define MAP_ARGS4(fn, a, b, c, d) fn(a), fn(b), fn(c), fn(d)
#define MAP_ARGS5(fn, a, b, c, d, e) fn(a), fn(b), fn(c), fn(d), fn(e)
#define MAP_ARGS6(fn, a, b, c, d, e, f) fn(a), fn(b), fn(c), fn(d), fn(e) ,fn(f)
#define MAP_ARGS7(fn, a, b, c, d, e, f, g) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g)
#define MAP_ARGS8(fn, a, b, c, d, e, f, g, h) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h)
#define MAP_ARGS9(fn, a, b, c, d, e, f, g, h, i) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i)
#define MAP_ARGS10(fn, a, b, c, d, e, f, g, h, i, j) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j)
#define MAP_ARGS11(fn, a, b, c, d, e, f, g, h, i, j, k) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k)
#define MAP_ARGS12(fn, a, b, c, d, e, f, g, h, i, j, k, l) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l)
#define MAP_ARGS13(fn, a, b, c, d, e, f, g, h, i, j, k, l, m) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m)
#define MAP_ARGS14(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n)
#define MAP_ARGS15(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o)
#define MAP_ARGS16(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p)
#define MAP_ARGS17(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q)
#define MAP_ARGS18(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r)
#define MAP_ARGS19(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s)
#define MAP_ARGS20(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t)
#define MAP_ARGS21(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t), fn(u)
#define MAP_ARGS22(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t), fn(u), fn(v)
#define MAP_ARGS23(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t), fn(u), fn(v), fn(w)
#define MAP_ARGS24(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t), fn(u), fn(v), fn(w), fn(x)
#define MAP_ARGS25(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t), fn(u), fn(v), fn(w), fn(x), fn(y)
#define MAP_ARGS26(fn, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) fn(a), fn(b), fn(c), fn(d), fn(e), fn(f), fn(g), fn(h), fn(i), fn(j), fn(k), fn(l), fn(m), fn(n), fn(o), fn(p), fn(q), fn(r), fn(s), fn(t), fn(u), fn(v), fn(w), fn(x), fn(y), fn(z)
#define MAP_ARGS27(fn, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26) fn(x0), fn(x1), fn(x2), fn(x3), fn(x4), fn(x5), fn(x6), fn(x7), fn(x8), fn(x9), fn(x10), fn(x11), fn(x12), fn(x13), fn(x14), fn(x15), fn(x16), fn(x17), fn(x18), fn(x19), fn(x20), fn(x21), fn(x22), fn(x23), fn(x24), fn(x25), fn(x26)
#define MAP_ARGS28(fn, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27) fn(x0), fn(x1), fn(x2), fn(x3), fn(x4), fn(x5), fn(x6), fn(x7), fn(x8), fn(x9), fn(x10), fn(x11), fn(x12), fn(x13), fn(x14), fn(x15), fn(x16), fn(x17), fn(x18), fn(x19), fn(x20), fn(x21), fn(x22), fn(x23), fn(x24), fn(x25), fn(x26), fn(x27)
#define MAP_ARGS29(fn, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28) fn(x0), fn(x1), fn(x2), fn(x3), fn(x4), fn(x5), fn(x6), fn(x7), fn(x8), fn(x9), fn(x10), fn(x11), fn(x12), fn(x13), fn(x14), fn(x15), fn(x16), fn(x17), fn(x18), fn(x19), fn(x20), fn(x21), fn(x22), fn(x23), fn(x24), fn(x25), fn(x26), fn(x27), fn(x28)
#define MAP_ARGS30(fn, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29) fn(x0), fn(x1), fn(x2), fn(x3), fn(x4), fn(x5), fn(x6), fn(x7), fn(x8), fn(x9), fn(x10), fn(x11), fn(x12), fn(x13), fn(x14), fn(x15), fn(x16), fn(x17), fn(x18), fn(x19), fn(x20), fn(x21), fn(x22), fn(x23), fn(x24), fn(x25), fn(x26), fn(x27), fn(x28), fn(x29)
#define MAP_ARGS31(fn, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30) fn(x0), fn(x1), fn(x2), fn(x3), fn(x4), fn(x5), fn(x6), fn(x7), fn(x8), fn(x9), fn(x10), fn(x11), fn(x12), fn(x13), fn(x14), fn(x15), fn(x16), fn(x17), fn(x18), fn(x19), fn(x20), fn(x21), fn(x22), fn(x23), fn(x24), fn(x25), fn(x26), fn(x27), fn(x28), fn(x29), fn(x30)
#define MAP_ARGS32(fn, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31) fn(x0), fn(x1), fn(x2), fn(x3), fn(x4), fn(x5), fn(x6), fn(x7), fn(x8), fn(x9), fn(x10), fn(x11), fn(x12), fn(x13), fn(x14), fn(x15), fn(x16), fn(x17), fn(x18), fn(x19), fn(x20), fn(x21), fn(x22), fn(x23), fn(x24), fn(x25), fn(x26), fn(x27), fn(x28), fn(x29), fn(x30), fn(x31)

#define MAP_ARGS__(fn, n, ...) MAP_ARGS##n(fn, __VA_ARGS__)
#define MAP_ARGS_(fn, n, ...) MAP_ARGS__(fn, n, __VA_ARGS__)
#define MAP_ARGS(fn, ...) MAP_ARGS_(fn, ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

#define FOLD0(fn, seed) seed
#define FOLD1(fn, seed, a) fn(seed, a)
#define FOLD2(fn, seed, a, b) fn(fn(seed, a), b)
#define FOLD3(fn, seed, a, b, c) fn(fn(fn(seed, a), b), c)
#define FOLD4(fn, seed, a, b, c, d) fn(fn(fn(fn(seed, a), b), c), d)
#define FOLD5(fn, seed, a, b, c, d, e) fn(fn(fn(fn(fn(seed, a), b), c), d), e)
#define FOLD6(fn, seed, a, b, c, d, e, f) fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e))
#define FOLD7(fn, seed, a, b, c, d, e, f, g) fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g)
#define FOLD8(fn, seed, a, b, c, d, e, f, g, h) fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h)
#define FOLD9(fn, seed, a, b, c, d, e, f, g, h, i) fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i)
#define FOLD10(fn, seed, a, b, c, d, e, f, g, h, i, j) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j)
#define FOLD11(fn, seed, a, b, c, d, e, f, g, h, i, j, k) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k)
#define FOLD12(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l)
#define FOLD13(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m)
#define FOLD14(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n)
#define FOLD15(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o)
#define FOLD16(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p)
#define FOLD17(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q)
#define FOLD18(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r)
#define FOLD19(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s)
#define FOLD20(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t)
#define FOLD21(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t), u)
#define FOLD22(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t), u), v)
#define FOLD23(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t), u), v), w)
#define FOLD24(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t), u), v), w), x)
#define FOLD25(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t), u), v), w), x), y)
#define FOLD26(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, a), b), c), d), e), f), g), h), i), j), k), l), m), n), o), p), q), r), s), t), u), v), w), x), y), z)
#define FOLD27(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, x0), x1), x2), x3), x4), x5), x6), x7), x8), x9), x10), x11), x12), x13), x14), x15), x16), x17), x18), x19), x20), x21), x22), x23), x24), x25), x26)
#define FOLD28(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, x0), x1), x2), x3), x4), x5), x6), x7), x8), x9), x10), x11), x12), x13), x14), x15), x16), x17), x18), x19), x20), x21), x22), x23), x24), x25), x26), x27)
#define FOLD29(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, x0), x1), x2), x3), x4), x5), x6), x7), x8), x9), x10), x11), x12), x13), x14), x15), x16), x17), x18), x19), x20), x21), x22), x23), x24), x25), x26), x27), x28)
#define FOLD30(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, x0), x1), x2), x3), x4), x5), x6), x7), x8), x9), x10), x11), x12), x13), x14), x15), x16), x17), x18), x19), x20), x21), x22), x23), x24), x25), x26), x27), x28), x29)
#define FOLD31(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, x0), x1), x2), x3), x4), x5), x6), x7), x8), x9), x10), x11), x12), x13), x14), x15), x16), x17), x18), x19), x20), x21), x22), x23), x24), x25), x26), x27), x28), x29), x30)
#define FOLD32(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31) fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(fn(seed, x0), x1), x2), x3), x4), x5), x6), x7), x8), x9), x10), x11), x12), x13), x14), x15), x16), x17), x18), x19), x20), x21), x22), x23), x24), x25), x26), x27), x28), x29), x30), x31)

#define FOLD__(fn, seed, n, ...) FOLD##n(fn, seed, __VA_ARGS__)
#define FOLD_(fn, seed, n, ...) FOLD__(fn, seed, n, __VA_ARGS__)
#define FOLD(fn, seed, ...) FOLD_(fn, seed, ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

#define FOLDR0(fn, seed) seed
#define FOLDR1(fn, seed, a) fn(seed, a)
#define FOLDR2(fn, seed, a, b) fn(seed, fn(a, b))
#define FOLDR3(fn, seed, a, b, c) fn(seed, fn(a, fn(b, c)))
#define FOLDR4(fn, seed, a, b, c, d) fn(seed, fn(a, fn(b, fn(c, d))))
#define FOLDR5(fn, seed, a, b, c, d, e) fn(seed, fn(a, fn(b, fn(c, fn(d, e)))))
#define FOLDR6(fn, seed, a, b, c, d, e, f) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, f))))))
#define FOLDR7(fn, seed, a, b, c, d, e, f, g) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, g)))))))
#define FOLDR8(fn, seed, a, b, c, d, e, f, g, h) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, h))))))))
#define FOLDR9(fn, seed, a, b, c, d, e, f, g, h, i) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, i)))))))))
#define FOLDR10(fn, seed, a, b, c, d, e, f, g, h, i, j) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, j))))))))))
#define FOLDR11(fn, seed, a, b, c, d, e, f, g, h, i, j, k) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, k)))))))))))
#define FOLDR12(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, l))))))))))))
#define FOLDR13(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, m)))))))))))))
#define FOLDR14(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, n))))))))))))))
#define FOLDR15(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, o)))))))))))))))
#define FOLDR16(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, p))))))))))))))))
#define FOLDR17(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, q)))))))))))))))))
#define FOLDR18(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, r)))))))))))))))))
#define FOLDR19(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, s))))))))))))))))))
#define FOLDR20(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, t)))))))))))))))))))
#define FOLDR21(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, fn(t, u))))))))))))))))))))
#define FOLDR22(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, fn(t, fn(u, v)))))))))))))))))))))
#define FOLDR23(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, fn(t, fn(u, fn(v, w)))))))))))))))))))))
#define FOLDR24(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, fn(t, fn(u, fn(v, fn(w, x))))))))))))))))))))))
#define FOLDR25(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, fn(t, fn(u, fn(v, fn(w, fn(x, y))))))))))))))))))))))
#define FOLDR26(fn, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) fn(seed, fn(a, fn(b, fn(c, fn(d, fn(e, fn(f, fn(g, fn(h, fn(i, fn(j, fn(k, fn(l, fn(m, fn(n, fn(o, fn(p, fn(q, fn(r, fn(s, fn(t, fn(u, fn(v, fn(w, fn(x, fn(y, z)))))))))))))))))))))))
#define FOLDR27(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27) fn(seed, fn(x0, fn(x1, fn(x2, fn(x3, fn(x4, fn(x5, fn(x6, fn(x7, fn(x8, fn(x9, fn(x10, fn(x11, fn(x12, fn(x13, fn(x14, fn(x15, fn(x16, fn(x17, fn(x18, fn(x19, fn(x20, fn(x21, fn(x22, fn(x23, fn(x24, fn(x25, x26))))))))))))))))))))))))))
#define FOLDR28(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28) fn(seed, fn(x0, fn(x1, fn(x2, fn(x3, fn(x4, fn(x5, fn(x6, fn(x7, fn(x8, fn(x9, fn(x10, fn(x11, fn(x12, fn(x13, fn(x14, fn(x15, fn(x16, fn(x17, fn(x18, fn(x19, fn(x20, fn(x21, fn(x22, fn(x23, fn(x24, fn(x25, fn(x26, x27))))))))))))))))))))))))))
#define FOLDR29(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29) fn(seed, fn(x0, fn(x1, fn(x2, fn(x3, fn(x4, fn(x5, fn(x6, fn(x7, fn(x8, fn(x9, fn(x10, fn(x11, fn(x12, fn(x13, fn(x14, fn(x15, fn(x16, fn(x17, fn(x18, fn(x19, fn(x20, fn(x21, fn(x22, fn(x23, fn(x24, fn(x25, fn(x26, fn(x27, x28))))))))))))))))))))))))))
#define FOLDR30(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30) fn(seed, fn(x0, fn(x1, fn(x2, fn(x3, fn(x4, fn(x5, fn(x6, fn(x7, fn(x8, fn(x9, fn(x10, fn(x11, fn(x12, fn(x13, fn(x14, fn(x15, fn(x16, fn(x17, fn(x18, fn(x19, fn(x20, fn(x21, fn(x22, fn(x23, fn(x24, fn(x25, fn(x26, fn(x27, fn(x28, x29))))))))))))))))))))))))))
#define FOLDR31(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31) fn(seed, fn(x0, fn(x1, fn(x2, fn(x3, fn(x4, fn(x5, fn(x6, fn(x7, fn(x8, fn(x9, fn(x10, fn(x11, fn(x12, fn(x13, fn(x14, fn(x15, fn(x16, fn(x17, fn(x18, fn(x19, fn(x20, fn(x21, fn(x22, fn(x23, fn(x24, fn(x25, fn(x26, fn(x27, fn(x28, fn(x29, x30))))))))))))))))))))))))))
#define FOLDR32(fn, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32) fn(seed, fn(x0, fn(x1, fn(x2, fn(x3, fn(x4, fn(x5, fn(x6, fn(x7, fn(x8, fn(x9, fn(x10, fn(x11, fn(x12, fn(x13, fn(x14, fn(x15, fn(x16, fn(x17, fn(x18, fn(x19, fn(x20, fn(x21, fn(x22, fn(x23, fn(x24, fn(x25, fn(x26, fn(x27, fn(x28, fn(x29, fn(x30, x31)))))))))))))))))))))))))))

#define FOLDR__(fn, seed, n, ...) FOLDR##n(fn, seed, __VA_ARGS__)
#define FOLDR_(fn, seed, n, ...) FOLDR__(fn, seed, n, __VA_ARGS__)
#define FOLDR(fn, seed, ...) FOLDR_(fn, seed, ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

#define OPERATOR0(op, seed) (seed)
#define OPERATOR1(op, seed, a) ((seed) op (a))
#define OPERATOR2(op, seed, a, b) ((seed) op (a) op (b))
#define OPERATOR3(op, seed, a, b, c) ((seed) op (a) op (b) op (c))
#define OPERATOR4(op, seed, a, b, c, d) ((seed) op (a) op (b) op (c) op (d))
#define OPERATOR5(op, seed, a, b, c, d, e) ((seed) op (a) op (b) op (c) op (d) op (e))
#define OPERATOR6(op, seed, a, b, c, d, e, f) ((seed) op (a) op (b) op (c) op (d) op (e) op (f))
#define OPERATOR7(op, seed, a, b, c, d, e, f, g) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g))
#define OPERATOR8(op, seed, a, b, c, d, e, f, g, h) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h))
#define OPERATOR9(op, seed, a, b, c, d, e, f, g, h, i) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i))
#define OPERATOR10(op, seed, a, b, c, d, e, f, g, h, i, j) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j))
#define OPERATOR11(op, seed, a, b, c, d, e, f, g, h, i, j, k) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k))
#define OPERATOR12(op, seed, a, b, c, d, e, f, g, h, i, j, k, l) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l))
#define OPERATOR13(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m))
#define OPERATOR14(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n))
#define OPERATOR15(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o))
#define OPERATOR16(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p))
#define OPERATOR17(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q))
#define OPERATOR18(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r))
#define OPERATOR19(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s))
#define OPERATOR20(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t))
#define OPERATOR21(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t) op (u))
#define OPERATOR22(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t) op (u) op (v))
#define OPERATOR23(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t) op (u) op (v) op (w))
#define OPERATOR24(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t) op (u) op (v) op (w) op (x))
#define OPERATOR25(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t) op (u) op (v) op (w) op (x) op (y))
#define OPERATOR26(op, seed, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) ((seed) op (a) op (b) op (c) op (d) op (e) op (f) op (g) op (h) op (i) op (j) op (k) op (l) op (m) op (n) op (o) op (p) op (q) op (r) op (s) op (t) op (u) op (v) op (w) op (x) op (y) op (z))
#define OPERATOR27(op, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26) ((seed) op (x0) op (x1) op (x2) op (x3) op (x4) op (x5) op (x6) op (x7) op (x8) op (x9) op (x10) op (x11) op (x12) op (x13) op (x14) op (x15) op (x16) op (x17) op (x18) op (x19) op (x20) op (x21) op (x22) op (x23) op (x24) op (x25) op (x26))
#define OPERATOR28(op, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27) ((seed) op (x0) op (x1) op (x2) op (x3) op (x4) op (x5) op (x6) op (x7) op (x8) op (x9) op (x10) op (x11) op (x12) op (x13) op (x14) op (x15) op (x16) op (x17) op (x18) op (x19) op (x20) op (x21) op (x22) op (x23) op (x24) op (x25) op (x26) op (x27))
#define OPERATOR29(op, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28) ((seed) op (x0) op (x1) op (x2) op (x3) op (x4) op (x5) op (x6) op (x7) op (x8) op (x9) op (x10) op (x11) op (x12) op (x13) op (x14) op (x15) op (x16) op (x17) op (x18) op (x19) op (x20) op (x21) op (x22) op (x23) op (x24) op (x25) op (x26) op (x27) op (x28))
#define OPERATOR30(op, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29) ((seed) op (x0) op (x1) op (x2) op (x3) op (x4) op (x5) op (x6) op (x7) op (x8) op (x9) op (x10) op (x11) op (x12) op (x13) op (x14) op (x15) op (x16) op (x17) op (x18) op (x19) op (x20) op (x21) op (x22) op (x23) op (x24) op (x25) op (x26) op (x27) op (x28) op (x29))
#define OPERATOR31(op, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30) ((seed) op (x0) op (x1) op (x2) op (x3) op (x4) op (x5) op (x6) op (x7) op (x8) op (x9) op (x10) op (x11) op (x12) op (x13) op (x14) op (x15) op (x16) op (x17) op (x18) op (x19) op (x20) op (x21) op (x22) op (x23) op (x24) op (x25) op (x26) op (x27) op (x28) op (x29) op (x30))
#define OPERATOR32(op, seed, x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31) ((seed) op (x0) op (x1) op (x2) op (x3) op (x4) op (x5) op (x6) op (x7) op (x8) op (x9) op (x10) op (x11) op (x12) op (x13) op (x14) op (x15) op (x16) op (x17) op (x18) op (x19) op (x20) op (x21) op (x22) op (x23) op (x24) op (x25) op (x26) op (x27) op (x28) op (x29) op (x30) op (x31))

#define OPERATOR__(op__, seed, n, ...) OPERATOR##n(op__, seed, __VA_ARGS__)
#define OPERATOR_(op__, seed, n, ...) OPERATOR__(op__, seed, n, __VA_ARGS__)
#define OPERATOR(op__, seed, ...) OPERATOR_(op__, seed, ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

#define ARGS_COUNT_(dummy, x100, x99, x98, x97, x96, x95, x94, x93, x92, x91, x90, x89, x88, x87, x86, x85, x84, x83, x82, x81, x80, x79, x78, x77, x76, x75, x74, x73, x72, x71, x70, x69, x68, x67, x66, x65, x64, x63, x62, x61, x60, x59, x58, x57, x56, x55, x54, x53, x52, x51, x50, x49, x48, x47, x46, x45, x44, x43, x42, x41, x40, x39, x38, x37, x36, x35, x34, x33, x32, x31, x30, x29, x28, x27, x26, x25, x24, x23, x22, x21, x20, x19, x18, x17, x16, x15, x14, x13, x12, x11, x10, x9, x8, x7, x6, x5, x4, x3, x2, x1, x0, ...) x0
#define ARGS_COUNT(...) ARGS_COUNT_(dummy, ##__VA_ARGS__, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define HEAD(head, ...) head
#define TAIL(head, ...) __VA_ARGS__
#define SWAP(a, b) a, b

#define ARGS_REVERSE_LAZY(...)\
    IF(HAS_ARGS(__VA_ARGS__))\
    (\
        DEFER2(ARGS_REVERSE_LAZY_LAZY_REC)() (TAIL(__VA_ARGS__))\
        IF(HAS_ARGS(TAIL(__VA_ARGS__)))(,) HEAD(__VA_ARGS__)\
    )
#define ARGS_REVERSE_LAZY_LAZY_REC() ARGS_REVERSE_LAZY

#define ARGS_REVERSE(...) EVAL(ARGS_REVERSE_LAZY(__VA_ARGS__))

#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__

#define EMPTY()
#define DEFER(id) id EMPTY()
#define DEFER2(m) m EMPTY EMPTY()()
#define DEFER3(m) m EMPTY EMPTY EMPTY()()()
#define DEFER4(m) m EMPTY EMPTY EMPTY EMPTY()()()()
#define EXPAND(...) __VA_ARGS__

#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__

#define CHECK_N(x, n, ...) n
#define CHECK(...) CHECK_N(__VA_ARGS__, 0,)

#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 ~, 1,

#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0

#define BOOL(x) COMPL(NOT(x))

#define IIF(c) PRIMITIVE_CAT(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t

#define IF_ELSE(c) IIF(BOOL(c))

#define EAT(...)
#define EXPAND(...) __VA_ARGS__
#define IF(c) IF_ELSE(c)(EXPAND, EAT)

#define ARGS_TRUE_(dummy, x1, x0, ...) x0
#define ARGS_TRUE(...) ARGS_TRUE_(dummy, ##__VA_ARGS__, 1, 0)

#define HAS_ARGS(...) BOOL(ARGS_TRUE(__VA_ARGS__))

#endif //PREPROCESSOR_CALCULUS_H
