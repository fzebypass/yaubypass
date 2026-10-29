#include <slate.h>

cshader glowshader{ R"(
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};
sampler sampler0; 
Texture2D texture0;

cbuffer TimeBuffer : register(b0)
{
    float2 glow_pos;
    float glow_strength;
};

float4 main(PS_INPUT input) : SV_Target
{
    float2 pos = input.pos.xy;
    float4 color = input.col;

    float dist = distance(pos, glow_pos);

    float radius = glow_strength;      // радиус блюра
    float softness = 1.0;              // можно вынести в cbuffer

    float alpha = exp(-pow(dist / radius, 2.0) * softness);

    return float4(color.rgb, color.a * alpha);
};
)" };

cshader shader{ R"(
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
sampler sampler0;
Texture2D texture0;

cbuffer TimeBuffer : register(b0)
{
    float iTime;
};

// ==== Colormap ====
float colormap_red(float x)
{
    if (x < 0.0) {
        return 54.0 / 255.0;
    } else if (x < 20049.0 / 82979.0) {
        return (829.79 * x + 54.51) / 255.0;
    } else {
        return 1.0;
    }
}

float colormap_green(float x)
{
    if (x < 20049.0 / 82979.0) {
        return 0.0;
    } else if (x < 327013.0 / 810990.0) {
        return (8546482679670.0 / 10875673217.0 * x - 2064961390770.0 / 10875673217.0) / 255.0;
    } else if (x <= 1.0) {
        return (103806720.0 / 483977.0 * x + 19607415.0 / 483977.0) / 255.0;
    } else {
        return 1.0;
    }
}

float colormap_blue(float x)
{
    if (x < 0.0) {
        return 54.0 / 255.0;
    } else if (x < 7249.0 / 82979.0) {
        return (829.79 * x + 54.51) / 255.0;
    } else if (x < 20049.0 / 82979.0) {
        return 127.0 / 255.0;
    } else if (x < 327013.0 / 810990.0) {
        return (792.0224934 * x - 64.3647907) / 255.0;
    } else {
        return 1.0;
    }
}

float4 colormap(float x)
{
    return float4(colormap_red(x), colormap_green(x), colormap_blue(x), 1.0);
}

// ==== Noise / FBM ====
float rand(float2 n)
{
    return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

float noise(float2 p)
{
    float2 ip = floor(p);
    float2 u = frac(p);
    u = u * u * (3.0 - 2.0 * u);

    float res = lerp(
        lerp(rand(ip), rand(ip + float2(1.0, 0.0)), u.x),
        lerp(rand(ip + float2(0.0, 1.0)), rand(ip + float2(1.0, 1.0)), u.x),
        u.y);

    return res * res;
}

static const float2x2 mtx = float2x2(0.80, 0.60, -0.60, 0.80);

float fbm(float2 p)
{
    float f = 0.0;

    f += 0.500000 * noise(p + iTime); p = mul(mtx, p) * 2.02;
    f += 0.031250 * noise(p);         p = mul(mtx, p) * 2.01;
    f += 0.250000 * noise(p);         p = mul(mtx, p) * 2.03;
    f += 0.125000 * noise(p);         p = mul(mtx, p) * 2.01;
    f += 0.062500 * noise(p);         p = mul(mtx, p) * 2.04;
    f += 0.015625 * noise(p + sin(iTime));

    return f / 0.96875;
}

float pattern(float2 p)
{
    return fbm(p + fbm(p + fbm(p)));
}

// ==== Main ====
float4 main(PS_INPUT input) : SV_Target
{
    uint texWidth, texHeight;
    texture0.GetDimensions(texWidth, texHeight);
    float2 resolution = float2(texWidth, texHeight);

    float2 fragCoord = input.pos;

    float2 uv = input.pos / resolution.x;
    float shade = pattern(uv);

    return float4(input.col.rgb, shade * input.col.w);
}

)" };

void c_shadertoy::_pass( c_buffer* buffer )
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    m_ctx->Map( constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
    memcpy( mapped.pData, buffer, sizeof( *buffer ) );
    m_ctx->Unmap( constant_buffer, 0 );

    m_ctx->PSSetConstantBuffers( 0, 1, &constant_buffer );
    m_ctx->PSSetShader( glowshader.shader, 0, 0 );
}
void c_shadertoy::pass( const ImDrawList* drawlist, const ImDrawCmd* cmd )
{
	g_shadertoy->_pass( ( c_buffer* )( cmd->UserCallbackData ) );
}

void c_shadertoy::_pass2( )
{
    c_buffer2 buffer = { };
    buffer.iTime = ImGui::GetTime( );

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    m_ctx->Map( constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
    memcpy( mapped.pData, &buffer, sizeof( buffer ) );
    m_ctx->Unmap( constant_buffer, 0 );

    m_ctx->PSSetConstantBuffers( 0, 1, &constant_buffer );
    m_ctx->PSSetShader( shader.shader, 0, 0 );
}
void c_shadertoy::pass2( const ImDrawList* drawlist, const ImDrawCmd* cmd )
{
	g_shadertoy->_pass2( );
}


void c_shadertoy::setup( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain )
{
	m_device = device;
	m_ctx = ctx;
	m_swapchain = swapchain;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof( c_buffer );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if ( !constant_buffer )
        device->CreateBuffer( &cbDesc, nullptr, &constant_buffer );

    glowshader.initialize( m_device );
    shader.initialize( m_device );
}
void c_shadertoy::draw( ImDrawList* draw, ImVec2 start, ImVec2 endpos, ImColor col, float rounding, c_buffer* buffer )
{
    draw->AddCallback( pass, buffer );
    draw->AddRectFilled( start, endpos, col, rounding );
	draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_shadertoy::bg( ImDrawList* draw, ImVec2 start, ImVec2 endpos, ImColor col, float rounding )
{
    draw->AddCallback( pass2, 0 );
    draw->AddRectFilled( start, endpos, col, rounding );
	draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}