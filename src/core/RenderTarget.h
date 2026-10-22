#pragma once

#include "GL.h"

namespace sparkles {

/// A colour-only framebuffer with an optional mip chain.
///
/// This deliberately replaces ffglex::FFGLFBO, for two reasons:
///
///  1. FFGLFBO::Release() leaks. Its second guard tests `depthBufferID` (already
///     zeroed by the block above) instead of `colorTextureID`, so the colour
///     texture is never deleted — a full-resolution texture leaked on every
///     resize. This source reallocates on every composition resolution change,
///     which would make that leak load-bearing.
///  2. FFGLFBO always allocates a depth renderbuffer. Nothing here uses
///     depth, and at 4K that is ~32 MB of pure waste per buffer.
///
/// It also adds float internal formats, which the trail accumulation needs so
/// that additive light can exceed 1.0 and fade without banding.
class RenderTarget
{
public:
	RenderTarget() = default;
	~RenderTarget();

	RenderTarget( const RenderTarget& )            = delete;
	RenderTarget& operator=( const RenderTarget& ) = delete;
	RenderTarget( RenderTarget&& other ) noexcept;
	RenderTarget& operator=( RenderTarget&& other ) noexcept;

	/// Allocates the texture and framebuffer. Releases any previous allocation
	/// first, so this doubles as "resize".
	///
	/// \param internalFormat  Must be colour-renderable in GL 4.1 core:
	///                        GL_R16F, GL_RG16F, GL_RGBA16F, GL_RGBA8, ...
	/// \param withMips        Allocate a full mip chain (for pyramid search and
	///                        for whole-frame reductions down to 1x1).
	bool Allocate( GLsizei width, GLsizei height, GLint internalFormat, bool withMips = false );

	void Release();

	bool IsValid() const { return fboID != 0 && textureID != 0; }

	GLuint  GetFBO() const     { return fboID; }
	GLuint  GetTexture() const { return textureID; }
	GLsizei GetWidth() const   { return width; }
	GLsizei GetHeight() const  { return height; }
	GLint   GetFormat() const  { return internalFormat; }
	bool    HasMips() const    { return hasMips; }
	/// Number of mip levels, i.e. the level index of the 1x1 top of the chain plus one.
	GLint   GetMipLevels() const { return mipLevels; }

	/// Recomputes the mip chain from level 0. No-op when the target has no mips.
	void GenerateMips() const;

	/// Clears level 0 to the given colour without disturbing the host's clear state.
	void Clear( float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 0.0f ) const;

	/// Bytes of VRAM this target occupies, mips included. Used for budget logging.
	size_t ByteSize() const;

private:
	GLuint  fboID          = 0;
	GLuint  textureID      = 0;
	GLsizei width          = 0;
	GLsizei height         = 0;
	GLint   internalFormat = 0;
	bool    hasMips        = false;
	GLint   mipLevels      = 1;
};

}  // namespace sparkles
