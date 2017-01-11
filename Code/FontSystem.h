// FontSystem.h

#pragma once

#include <string>
#include <map>
#include <vector>
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace FontSys
{
	class Font;
	class Glyph;
	class System;

	typedef std::map< std::string, Font* > FontMap;
	typedef std::map< std::string, GLuint > TextDisplayListMap;
	typedef std::map< FT_ULong, Glyph* > GlyphMap;
	typedef std::map< FT_ULong, FT_Vector > KerningMap;
}

// An instance of this class is a layer of software that sits between
// the application and the free-type library.
class FontSys::System
{
public:

	System( void );
	virtual ~System( void );

	bool Initialize( void );
	bool Finalize( void );

	enum Justification
	{
		JUSTIFY_LEFT,
		JUSTIFY_RIGHT,
		JUSTIFY_CENTER,
		JUSTIFY_LEFT_AND_RIGHT,
	};

	virtual std::string ResolveFontPath( const std::string& font );

	void SetJustification( Justification justification ) { this->justification = justification; }
	Justification GetJustification( void ) { return justification; }

	void SetLineWidth( GLfloat lineWidth ) { this->lineWidth = lineWidth; }
	GLfloat GetLineWidth( void ) { return lineWidth; }

	void SetLineHeight( GLfloat lineHeight ) { this->lineHeight = lineHeight; }
	GLfloat GetLineHeight( void ) { return lineHeight; }

	bool SetBaseLineDelta( GLfloat baseLineDelta );
	GLfloat GetBaseLineDelta( void ) { return baseLineDelta; }

	void SetFont( const std::string& font ) { this->font = font; }
	const std::string& GetFont( void ) { return font; }

	void SetWordWrap( bool wordWrap ) { this->wordWrap = wordWrap; }
	bool GetWordWrap( void ) { return wordWrap; }

	// When called, we assume that an OpenGL context is already bound.  Only one font
	// system should be used per context since the system caches texture objects and display lists.
	// To position and orient text, the caller must setup the appropriate modelview matrix.
	// The object-space of the text begins on the positive X-axis and then subsequent lines fill the 4th quadrant of the XY-plane.
	// The given flag can be set to true in the case that the text will never change.  This causes
	// us to use and cache a display list for rendering.  If the flag is falsely set for dynamic text,
	// OpenGL may run out of display list memory and/or otherwise bog down!
	bool DrawText( const std::string& text, bool staticText = false );

	// This is provided for convenience when a simple translation is all that's required.
	bool DrawText( GLfloat x, GLfloat y, const std::string& text, bool staticText = false );

	// This ignores wrapping.
	bool CalcTextLength( const std::string& text, GLfloat& length );

	// Tell us if a display list is cached for the given string.
	bool DisplayListCached( const std::string& text );

	FT_Library& GetLibrary( void ) { return library; }

private:

	Font* GetOrCreateCachedFont( void );
	std::string MakeFontKey( const std::string& font );

	std::string font;
	GLfloat lineWidth, lineHeight;
	GLfloat baseLineDelta;
	Justification justification;
	bool wordWrap;
	bool initialized;
	FT_Library library;
	FontMap fontMap;
};

// An instance of this class maintains a means of rendering a cached font using OpenGL.
class FontSys::Font
{
public:

	Font( System* fontSystem );
	virtual ~Font( void );

	virtual bool Initialize( const std::string& font );
	virtual bool Finalize( void );

	virtual bool DrawText( const std::string& text, bool staticText = false );
	virtual bool CalcTextLength( const std::string& text, GLfloat& length );
	virtual bool DisplayListCached( const std::string& text );

private:

	struct GlyphLink
	{
		GLfloat dx, dy;		// Adding this to the previous glyph origin gives us our origin.
		GLfloat x, y;		// This is the lower-left corner position of the glyph.
		GLfloat w, h;		// This is the width and height of the glyph.
		Glyph* glyph;
		GlyphLink* nextGlyphLink;

		void GetMetrics( FT_Glyph_Metrics& metrics ) const;
	};

	typedef std::vector< GlyphLink* > GlyphChainVector;

	GLfloat CalcConversionFactor( void );

	GlyphLink* GenerateGlyphChain( const wchar_t* charCodeString, GLfloat conversionFactor );
	void KernGlyphChain( GlyphLink* glyphLink, GLfloat conversionFactor );
	void RenderGlyphChain( GlyphLink* glyphLink, GLfloat ox, GLfloat oy );
	void DeleteGlyphChain( GlyphLink* glyphLink );
	GLfloat CalcGlyphChainLength( GlyphLink* glyphLink );
	GlyphLink* BreakGlyphChain( GlyphLink* glyphLink );
	void JustifyGlyphChain( GlyphLink* glyphLink );
	int CountGlyphsInChain( GlyphLink* glyphLink, FT_ULong charCode );

	FT_ULong MakeKerningKey( FT_UInt leftGlyphIndex, FT_UInt rightGlyphIndex );

	bool initialized;
	System* fontSystem;
	GlyphMap glyphMap;
	KerningMap kerningMap;
	TextDisplayListMap textDisplayListMap;
	GLuint lineHeightMetric;
};

class FontSys::Glyph
{
public:

	Glyph( void );
	virtual ~Glyph( void );

	bool Initialize( FT_GlyphSlot& glyphSlot, FT_UInt glyphIndex, FT_ULong charCode );
	bool Finalize( void );

	GLuint GetTexture( void ) { return texture; }
	const FT_Glyph_Metrics& GetMetrics( void ) { return metrics; }
	FT_UInt GetIndex( void ) { return glyphIndex; }
	FT_ULong GetCharCode( void ) { return charCode; }

private:

	GLuint texture;
	FT_Glyph_Metrics metrics;
	FT_UInt glyphIndex;
	FT_ULong charCode;
};

// FontSystem.h