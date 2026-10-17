#pragma once

// Shared COM infrastructure for NEXLINK shims (dxgi, d3d11).
//
// COM interfaces are modelled as C++ abstract classes annotated with
// __attribute__((ms_abi)) so every virtual method (and the vtable itself)
// follows the Microsoft x64 ABI, which is what the relinked PE expects.
// Single inheritance keeps the vtable slot order identical to the COM layout.
//
// ComObjectBase provides a CRTP refcount + QueryInterface scaffold:
//   class MockFoo : public ComObjectBase<MockFoo, IFoo> { ... };
// The mock must inherit the *terminal* interface (e.g. IFoo3) so its
// intermediate methods can be `override`; the base handles IUnknown +
// a generic IDXGIObject/D3D11DeviceChild body and delegates extra IID
// resolution to Derived::query_extra().

#include "common/win32.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

namespace win2lin::com {

using namespace win2lin::win;

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};

using REFIID = const GUID&;
using REFGUID = const GUID&;

inline bool guid_eq(const GUID& a, const GUID& b) {
    return std::memcmp(&a, &b, sizeof(GUID)) == 0;
}

// Common HRESULTs.
constexpr HRESULT S_OK_ = 0;
constexpr HRESULT S_FALSE_ = 1;
constexpr HRESULT E_NOTIMPL_ = static_cast<HRESULT>(0x80004001LL);
constexpr HRESULT E_NOINTERFACE_ = static_cast<HRESULT>(0x80004002LL);
constexpr HRESULT E_POINTER_ = static_cast<HRESULT>(0x80004003LL);
constexpr HRESULT E_INVALIDARG_ = static_cast<HRESULT>(0x80070057LL);
constexpr HRESULT DXGI_ERROR_INVALID_CALL_ = static_cast<HRESULT>(0x887A0001LL);
constexpr HRESULT DXGI_ERROR_NOT_FOUND_ = static_cast<HRESULT>(0x887A0002LL);
constexpr HRESULT DXGI_ERROR_MORE_DATA_ = static_cast<HRESULT>(0x887A0003LL);
constexpr HRESULT DXGI_ERROR_UNSUPPORTED_ = static_cast<HRESULT>(0x887A0004LL);

// IUnknown (IID {00000000-0000-0000-C000-000000000046}).
constexpr GUID IID_IUnknown_ =
    {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct __attribute__((ms_abi)) IUnknown {
    virtual HRESULT WINAPI QueryInterface(REFIID riid, void** ppv) = 0;
    virtual ULONG WINAPI AddRef() = 0;
    virtual ULONG WINAPI Release() = 0;

protected:
    ~IUnknown() = default;
};

// --------------------------------------------------------------------------
// CRTP refcount + IUnknown body.
//
// Derived must implement:  bool query_extra(REFIID, void**)
// which returns true after filling *ppv for its own interface family.
// --------------------------------------------------------------------------
template <typename Derived, typename Iface>
class ComObjectBase : public Iface {
public:
    HRESULT WINAPI QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER_;
        *ppv = nullptr;
        if (guid_eq(riid, IID_IUnknown_)) {
            *ppv = static_cast<Iface*>(this);
        } else if (static_cast<Derived*>(this)->query_extra(riid, ppv)) {
            // Derived filled *ppv.
        } else {
            return E_NOINTERFACE_;
        }
        AddRef();
        return S_OK_;
    }

    ULONG WINAPI AddRef() override {
        return ++refcount_;
    }

    ULONG WINAPI Release() override {
        const ULONG n = --refcount_;
        if (n == 0) delete static_cast<Derived*>(this);
        return n;
    }

protected:
    ComObjectBase() = default;
    ~ComObjectBase() = default;

private:
    std::atomic<ULONG> refcount_{1};
};

} // namespace win2lin::com
