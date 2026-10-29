#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

struct D3DXVECTOR2 {
    float x;
    float y;

    constexpr D3DXVECTOR2() noexcept : x(0.0f), y(0.0f) {}
    constexpr D3DXVECTOR2(float x_, float y_) noexcept : x(x_), y(y_) {}

    constexpr bool operator==(const D3DXVECTOR2& other) const noexcept {
        return x == other.x && y == other.y;
    }

    constexpr bool operator!=(const D3DXVECTOR2& other) const noexcept {
        return !(*this == other);
    }

    explicit constexpr operator bool() const noexcept {
        return x != 0.0f || y != 0.0f;
    }
};

struct D3DXVECTOR3 {
    float x;
    float y;
    float z;

    constexpr D3DXVECTOR3() noexcept : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr D3DXVECTOR3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr D3DXVECTOR3 operator+(const D3DXVECTOR3& other) const noexcept {
        return { x + other.x, y + other.y, z + other.z };
    }

    constexpr D3DXVECTOR3 operator-(const D3DXVECTOR3& other) const noexcept {
        return { x - other.x, y - other.y, z - other.z };
    }

    constexpr D3DXVECTOR3 operator*(float scalar) const noexcept {
        return { x * scalar, y * scalar, z * scalar };
    }

    constexpr D3DXVECTOR3 operator/(float scalar) const noexcept {
        return { x / scalar, y / scalar, z / scalar };
    }

    constexpr D3DXVECTOR3& operator+=(const D3DXVECTOR3& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    constexpr D3DXVECTOR3& operator-=(const D3DXVECTOR3& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    constexpr D3DXVECTOR3& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    constexpr D3DXVECTOR3& operator/=(float scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    constexpr bool operator==(const D3DXVECTOR3& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }

    constexpr bool operator!=(const D3DXVECTOR3& other) const noexcept {
        return !(*this == other);
    }
};

inline constexpr D3DXVECTOR3 operator*(float scalar, const D3DXVECTOR3& vec) noexcept {
    return vec * scalar;
}

struct D3DXVECTOR4 {
    float x;
    float y;
    float z;
    float w;

    constexpr D3DXVECTOR4() noexcept : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr D3DXVECTOR4(float x_, float y_, float z_, float w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}
};

struct D3DXMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };

    operator float*() noexcept {
        return &m[0][0];
    }

    operator const float*() const noexcept {
        return &m[0][0];
    }

    explicit operator bool() const noexcept {
        const float* values = &m[0][0];
        for (std::size_t i = 0; i < 16; ++i) {
            if (values[i] != 0.0f) {
                return true;
            }
        }
        return false;
    }
};

inline float D3DXVec3Length(const D3DXVECTOR3* vec) {
    if (!vec) {
        return 0.0f;
    }

    return std::sqrt((vec->x * vec->x) + (vec->y * vec->y) + (vec->z * vec->z));
}

inline float D3DXVec3LengthSq(const D3DXVECTOR3* vec) {
    if (!vec) {
        return 0.0f;
    }

    return (vec->x * vec->x) + (vec->y * vec->y) + (vec->z * vec->z);
}

inline float D3DXVec3Dot(const D3DXVECTOR3* left, const D3DXVECTOR3* right) {
    if (!left || !right) {
        return 0.0f;
    }

    return (left->x * right->x) + (left->y * right->y) + (left->z * right->z);
}

inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* vec) {
    if (!out || !vec) {
        return out;
    }

    const float length = D3DXVec3Length(vec);
    if (length <= std::numeric_limits<float>::epsilon()) {
        out->x = 0.0f;
        out->y = 0.0f;
        out->z = 0.0f;
        return out;
    }

    out->x = vec->x / length;
    out->y = vec->y / length;
    out->z = vec->z / length;
    return out;
}

inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out, const D3DXMATRIX* matrix) {
    if (!out || !matrix) {
        return out;
    }

    D3DXMATRIX result{};
    result._11 = matrix->_11; result._12 = matrix->_21; result._13 = matrix->_31; result._14 = matrix->_41;
    result._21 = matrix->_12; result._22 = matrix->_22; result._23 = matrix->_32; result._24 = matrix->_42;
    result._31 = matrix->_13; result._32 = matrix->_23; result._33 = matrix->_33; result._34 = matrix->_43;
    result._41 = matrix->_14; result._42 = matrix->_24; result._43 = matrix->_34; result._44 = matrix->_44;
    *out = result;
    return out;
}

inline D3DXVECTOR4* D3DXVec3Transform(D3DXVECTOR4* out, const D3DXVECTOR3* vec, const D3DXMATRIX* matrix) {
    if (!out || !vec || !matrix) {
        return out;
    }

    out->x = (vec->x * matrix->_11) + (vec->y * matrix->_21) + (vec->z * matrix->_31) + matrix->_41;
    out->y = (vec->x * matrix->_12) + (vec->y * matrix->_22) + (vec->z * matrix->_32) + matrix->_42;
    out->z = (vec->x * matrix->_13) + (vec->y * matrix->_23) + (vec->z * matrix->_33) + matrix->_43;
    out->w = (vec->x * matrix->_14) + (vec->y * matrix->_24) + (vec->z * matrix->_34) + matrix->_44;
    return out;
}

struct D3DX11_IMAGE_LOAD_INFO {
    UINT Width = 0;
    UINT Height = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

struct ID3DX11ThreadPump {};

namespace directx_compat {
    class scoped_com_init {
    public:
        scoped_com_init() noexcept
            : hr_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

        ~scoped_com_init() {
            if (SUCCEEDED(hr_)) {
                CoUninitialize();
            }
        }

        HRESULT result() const noexcept {
            return hr_;
        }

    private:
        HRESULT hr_;
    };

    inline HRESULT create_shader_resource_view_from_memory(
        ID3D11Device* device,
        const void* source_data,
        SIZE_T source_size,
        ID3D11ShaderResourceView** shader_resource_view,
        UINT* width,
        UINT* height) {
        if (!device || !source_data || source_size == 0 || !shader_resource_view) {
            return E_INVALIDARG;
        }

        *shader_resource_view = nullptr;
        if (width) {
            *width = 0;
        }
        if (height) {
            *height = 0;
        }

        scoped_com_init com_init;
        const HRESULT init_hr = com_init.result();
        if (FAILED(init_hr) && init_hr != RPC_E_CHANGED_MODE) {
            return init_hr;
        }

        using Microsoft::WRL::ComPtr;

        ComPtr<IWICImagingFactory> imaging_factory;
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&imaging_factory));
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<IWICStream> stream;
        hr = imaging_factory->CreateStream(&stream);
        if (FAILED(hr)) {
            return hr;
        }

        if (source_size > static_cast<SIZE_T>((std::numeric_limits<DWORD>::max)())) {
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }

        hr = stream->InitializeFromMemory(
            reinterpret_cast<BYTE*>(const_cast<void*>(source_data)),
            static_cast<DWORD>(source_size));
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<IWICBitmapDecoder> decoder;
        hr = imaging_factory->CreateDecoderFromStream(
            stream.Get(),
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) {
            return hr;
        }

        UINT image_width = 0;
        UINT image_height = 0;
        hr = frame->GetSize(&image_width, &image_height);
        if (FAILED(hr)) {
            return hr;
        }

        ComPtr<IWICFormatConverter> converter;
        hr = imaging_factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) {
            return hr;
        }

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            return hr;
        }

        if (image_width > ((std::numeric_limits<UINT>::max)() / 4)) {
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        }

        const UINT row_pitch = image_width * 4;
        if (image_height != 0 && row_pitch > ((std::numeric_limits<UINT>::max)() / image_height)) {
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
        }

        const UINT image_size = row_pitch * image_height;
        std::vector<std::uint8_t> pixels(image_size);
        hr = converter->CopyPixels(nullptr, row_pitch, image_size, pixels.data());
        if (FAILED(hr)) {
            return hr;
        }

        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = image_width;
        texture_desc.Height = image_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initial_data{};
        initial_data.pSysMem = pixels.data();
        initial_data.SysMemPitch = row_pitch;

        ComPtr<ID3D11Texture2D> texture;
        hr = device->CreateTexture2D(&texture_desc, &initial_data, &texture);
        if (FAILED(hr)) {
            return hr;
        }

        hr = device->CreateShaderResourceView(texture.Get(), nullptr, shader_resource_view);
        if (FAILED(hr)) {
            return hr;
        }

        if (width) {
            *width = image_width;
        }
        if (height) {
            *height = image_height;
        }

        return S_OK;
    }
}

inline HRESULT D3DX11CreateShaderResourceViewFromMemory(
    ID3D11Device* device,
    const void* source_data,
    SIZE_T source_size,
    D3DX11_IMAGE_LOAD_INFO* load_info,
    ID3DX11ThreadPump*,
    ID3D11ShaderResourceView** shader_resource_view,
    HRESULT* result) {
    UINT width = 0;
    UINT height = 0;
    const HRESULT hr = directx_compat::create_shader_resource_view_from_memory(
        device,
        source_data,
        source_size,
        shader_resource_view,
        &width,
        &height);

    if (load_info) {
        load_info->Width = width;
        load_info->Height = height;
        load_info->Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    if (result) {
        *result = hr;
    }

    return hr;
}
