// FontSystem.cpp

#include "FontSystem.h"
#include FT_MODULE_H
#include FT_TRUETYPE_IDS_H
#include <algorithm>
#include <locale>
#include <codecvt>

// TODO: Find and plug mem-leak.

using namespace FontSys;

System::System( void )
{
	initialized = false;
	font = "ChanticleerRomanNF.ttf"; //"OpenSans-Regular.ttf"; //"Anonymous_Pro.ttf";
	lineWidth = 0.f;
	lineHeight = 5.f;
	baseLineDelta = -7.f;
	justification = JUSTIFY_LEFT;
	wordWrap = false;
}

/*virtual*/ System::~System( void )
{
	Finalize();
}

bool System::SetBaseLineDelta( GLfloat baseLineDelta )
{
	if( baseLineDelta >= 0.f )
		return false;

	if( -baseLineDelta < lineHeight )
		return false;

	this->baseLineDelta = baseLineDelta;
	return true;
}

bool System::Initialize( void )
{
	bool success = false;

	do
	{
		if( initialized )
			break;

		FT_Error error = FT_Init_FreeType( &library );
		if( error != FT_Err_Ok )
			break;

		initialized = true;

		success = true;
	}
	while( false );

	return success;
}

bool System::Finalize( void )
{
	bool success = false;
	
	do
	{
		while( fontMap.size() > 0 )
		{
			FontMap::iterator iter = fontMap.begin();
			Font* font = iter->second;
			font->Finalize();
			delete font;
			fontMap.erase( iter );
		}

		FT_Error error = FT_Done_Library( library );
		if( error != FT_Err_Ok )
			break;

		initialized = false;

		success = true;
	}
	while( false );

	return success;
}

/*virtual*/ std::string System::ResolveFontPath( const std::string& font )
{
	return "Fonts/" + font;
}

bool System::DrawText( GLfloat x, GLfloat y, const std::string& text, bool staticText /*= false*/ )
{
	bool success = false;

	glPushMatrix();
	glTranslatef( x, y, 0.f );

	success = DrawText( text, staticText );

	glPopMatrix();

	return success;
}

bool System::DrawText( const std::string& text, bool staticText /*= false*/ )
{
	bool success = false;

	do
	{
		if( !initialized )
			break;

		Font* cachedFont = GetOrCreateCachedFont();
		if( !cachedFont )
			break;

		if( !cachedFont->DrawText( text, staticText ) )
			break;

		success = true;
	}
	while( false );

	return success;
}

bool System::CalcTextLength( const std::string& text, GLfloat& length )
{
	bool success = false;

	do
	{
		if( !initialized )
			break;

		Font* cachedFont = GetOrCreateCachedFont();
		if( !cachedFont )
			break;

		if( !cachedFont->CalcTextLength( text, length ) )
			break;

		success = true;
	}
	while( false );

	return success;
}

bool System::DisplayListCached( const std::string& text )
{
	if( !initialized )
		return false;

	Font* cachedFont = GetOrCreateCachedFont();
	if( !cachedFont )
		return false;

	return cachedFont->DisplayListCached( text );
}

Font* System::GetOrCreateCachedFont( void )
{
	Font* cachedFont = nullptr;
	
	if( initialized && !font.empty() )
	{
		std::string key = MakeFontKey( font );

		FontMap::iterator iter = fontMap.find( key );
		if( iter != fontMap.end() )
			cachedFont = iter->second;
		else
		{
			cachedFont = new Font( this );

			if( !cachedFont->Initialize( font ) )
				delete cachedFont;
			else
				fontMap[ key ] = cachedFont;
		}
	}

	return cachedFont;
}

std::string System::MakeFontKey( const std::string& font )
{
	std::string key = font;
	std::transform( key.begin(), key.end(), key.begin(), ::tolower );
	return key;
}

Font::Font( System* fontSystem )
{
	initialized = false;
	this->fontSystem = fontSystem;
	lineHeightMetric = 0;
}

/*virtual*/ Font::~Font( void )
{
	if( initialized )
		Finalize();
}

/*virtual*/ bool Font::Initialize( const std::string& font )
{
	bool success = false;
	FT_Face face = nullptr;

	do
	{
		if( initialized )
			break;

		std::string fontFile = fontSystem->ResolveFontPath( font );

		FT_Error error = FT_New_Face( fontSystem->GetLibrary(), fontFile.c_str(), 0, &face );
		if( error != FT_Err_Ok || !face )
			break;

		int i, j = -1;
		for( i = 0; i < face->num_charmaps && j == -1; i++ )
		{
			FT_CharMapRec* charmapRec = face->charmaps[i];
			if( charmapRec->encoding == FT_ENCODING_UNICODE )
				j = i;
		}

		if( j == -1 )
			break;

		error = FT_Set_Charmap( face, face->charmaps[j] );
		if( error != FT_Err_Ok )
			break;

		error = FT_Set_Char_Size( face, 0, 128*64, 0, 0 );
		if( error != FT_Err_Ok )
			break;

		const wchar_t* charCodeString = L"abcdefghijklmnopqrstuvwxyz"
										L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
										L"`~!@#$%^&*()_+-={}[],.<>/?'\";: ";

		lineHeightMetric = 0;

		for( i = 0; charCodeString[i] != '\0'; i++ )
		{
			FT_ULong charCode = charCodeString[i];
			FT_UInt glyphIndex = FT_Get_Char_Index( face, charCode );
			if( glyphIndex == 0 )
				continue;

			error = FT_Load_Glyph( face, glyphIndex, FT_LOAD_DEFAULT );
			if( error != FT_Err_Ok )
				break;

			FT_GlyphSlot& glyph = face->glyph;

			if( glyph->format != FT_GLYPH_FORMAT_BITMAP )
			{
				error = FT_Render_Glyph( glyph, FT_RENDER_MODE_NORMAL );
				if( error != FT_Err_Ok )
					break;
			}

			Glyph* cachedGlyph = new Glyph();
			glyphMap[ charCode ] = cachedGlyph;

			if( !cachedGlyph->Initialize( glyph, glyphIndex, charCode ) )
				break;

			if( glyph->metrics.height == glyph->metrics.horiBearingY )
				if( ( unsigned )glyph->metrics.height > lineHeightMetric )
					lineHeightMetric = glyph->metrics.height;
		}

		if( charCodeString[i] != '\0' )
			break;

		if( lineHeightMetric == 0 )
			break;

		kerningMap.clear();

		if( FT_HAS_KERNING( face ) )
		{
			for( i = 0; charCodeString[i] != '\0'; i++ )
			{
				FT_ULong leftCharCode = charCodeString[i];
				FT_UInt leftGlyphIndex = FT_Get_Char_Index( face, leftCharCode );
				if( leftGlyphIndex == 0 )
					continue;

				for( j = 0; j < charCodeString[j] != '\0'; j++ )
				{
					FT_ULong rightCharCode = charCodeString[j];
					FT_UInt rightGlyphIndex = FT_Get_Char_Index( face, rightCharCode );
					if( rightGlyphIndex == 0 )
						continue;

					// TODO: I'm probably getting the kerning vector in the wrong units.
					FT_Vector kerning;
					FT_Get_Kerning( face, leftGlyphIndex, rightGlyphIndex, FT_KERNING_DEFAULT, &kerning );

					FT_ULong key = MakeKerningKey( leftGlyphIndex, rightGlyphIndex );
					kerningMap[ key ] = kerning;
				}
			}
		}

		initialized = true;

		success = true;
	}
	while( false );

	if( face )
		FT_Done_Face( face );

	return success;
}

/*virtual*/ bool Font::Finalize( void )
{
	bool success = false;

	do
	{
		while( glyphMap.size() > 0 )
		{
			GlyphMap::iterator iter = glyphMap.begin();
			Glyph* glyph = iter->second;
			glyph->Finalize();
			delete glyph;
			glyphMap.erase( iter );
		}

		while( textDisplayListMap.size() > 0 )
		{
			TextDisplayListMap::iterator iter = textDisplayListMap.begin();
			GLuint displayList = iter->second;
			glDeleteLists( displayList, 1 );
			textDisplayListMap.erase( iter );
		}

		initialized = false;

		success = true;
	}
	while( false );
	
	return success;
}

FT_ULong Font::MakeKerningKey( FT_UInt leftGlyphIndex, FT_UInt rightGlyphIndex )
{
	FT_ULong left = leftGlyphIndex;
	FT_ULong right = rightGlyphIndex;

	left = left << 16;		// I thought that longs were 64-bit while ints were 32-bit...?

	return( left | right );
}

/*virtual*/ bool Font::DisplayListCached( const std::string& text )
{
	TextDisplayListMap::iterator iter = textDisplayListMap.find( text );
	return( iter == textDisplayListMap.end() ? false : true );
}

/*virtual*/ bool Font::DrawText( const std::string& text, bool staticText /*= false*/ )
{
	bool success = false;
	GlyphChainVector glyphChainVector;
	GLuint displayList = 0;

	do
	{
		if( text.empty() )
			break;

		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		glEnable( GL_TEXTURE_2D );
		glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND );

		GLfloat color[4];
		glGetFloatv( GL_CURRENT_COLOR, color );
		glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color );

		if( staticText )
		{
			TextDisplayListMap::iterator iter = textDisplayListMap.find( text );
			if( iter != textDisplayListMap.end() )
			{
				displayList = iter->second;
				glCallList( displayList );
			}
		}

		if( displayList == 0 )
		{
			GLfloat conversionFactor = CalcConversionFactor();
			std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
			std::wstring wideText = converter.from_bytes( text.c_str() );
			const wchar_t* charCodeString = wideText.c_str();

			GlyphLink* glyphLink = GenerateGlyphChain( charCodeString, conversionFactor );
			if( !glyphLink )
				break;

			if( kerningMap.size() > 0 )
				KernGlyphChain( glyphLink, conversionFactor );

			glyphChainVector.push_back( glyphLink );

			if( fontSystem->GetLineWidth() > 0.f )
			{
				if( fontSystem->GetWordWrap() )
				{
					while( true )
					{
						glyphLink = BreakGlyphChain( glyphLink );
						if( !glyphLink )
							break;

						while( !glyphLink->glyph || glyphLink->glyph->GetCharCode() == ' ' )
						{
							GlyphLink* deleteGlyphLink = glyphLink;
							glyphLink = glyphLink->nextGlyphLink;
							delete deleteGlyphLink;
						}

						glyphLink->dx = 0.f;
						glyphChainVector.push_back( glyphLink );
					}
				}

				if( fontSystem->GetJustification() != System::JUSTIFY_LEFT )
				{
					for( unsigned int i = 0; i < glyphChainVector.size(); i++ )
					{
						glyphLink = glyphChainVector[i];
						JustifyGlyphChain( glyphLink );
					}
				}
			}

			if( staticText )
			{
				displayList = glGenLists(1);
				glNewList( displayList, GL_COMPILE_AND_EXECUTE );
			}

			GLfloat baseLine = 0.f;
			for( unsigned int i = 0; i < glyphChainVector.size(); i++ )
			{
				glyphLink = glyphChainVector[i];
				RenderGlyphChain( glyphLink, 0.f, baseLine );
				baseLine += fontSystem->GetBaseLineDelta();
			}

			if( staticText )
			{
				glEndList();
				textDisplayListMap[ text ] = displayList;
			}
		}

		success = true;
	}
	while( false );

	glDisable( GL_BLEND );
	glDisable( GL_TEXTURE_2D );

	for( unsigned int i = 0; i < glyphChainVector.size(); i++ )
	{
		GlyphLink* glyphLink = glyphChainVector[i];
		DeleteGlyphChain( glyphLink );
	}

	return success;
}

/*virtual*/ bool Font::CalcTextLength( const std::string& text, GLfloat& length )
{
	bool success = false;
	GlyphLink* glyphLink = nullptr;

	do
	{
		length = 0.f;

		if( !text.empty() )
		{
			GLfloat conversionFactor = CalcConversionFactor();
			std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
			std::wstring wideText = converter.from_bytes( text.c_str() );
			const wchar_t* charCodeString = wideText.c_str();

			glyphLink = GenerateGlyphChain( charCodeString, conversionFactor );
			if( !glyphLink )
				break;

			if( kerningMap.size() > 0 )
				KernGlyphChain( glyphLink, conversionFactor );

			length = CalcGlyphChainLength( glyphLink );
		}

		success = true;
	}
	while( false );

	DeleteGlyphChain( glyphLink );

	return true;
}

GLfloat Font::CalcConversionFactor( void )
{
	return( fontSystem->GetLineHeight() / GLfloat( lineHeightMetric ) );
}

void Font::GlyphLink::GetMetrics( FT_Glyph_Metrics& metrics ) const
{
	if( glyph )
		metrics = glyph->GetMetrics();
	else
	{
		memset( &metrics, 0, sizeof( FT_Glyph_Metrics ) );
		metrics.width = 500;
		metrics.height = 500;
	}
}

Font::GlyphLink* Font::GenerateGlyphChain( const wchar_t* charCodeString, GLfloat conversionFactor )
{
	GlyphLink* firstGlyphLink = nullptr;
	GlyphLink* prevGlyphLink = nullptr;

	for( int i = 0; charCodeString[i] != '\0'; i++ )
	{
		wchar_t charCode = charCodeString[i];

		Glyph* glyph = nullptr;
		GlyphMap::iterator iter = glyphMap.find( charCode );
		if( iter != glyphMap.end() )
			glyph = iter->second;

		GlyphLink* glyphLink = new GlyphLink();
		glyphLink->glyph = glyph;

		FT_Glyph_Metrics metrics;
		glyphLink->GetMetrics( metrics );

		glyphLink->w = GLfloat( metrics.width ) * conversionFactor;
		glyphLink->h = GLfloat( metrics.height ) * conversionFactor;

		if( !prevGlyphLink )
		{
			glyphLink->dx = 0.f;
			glyphLink->dy = 0.f;

			glyphLink->x = 0.f;
			glyphLink->y = GLfloat( metrics.horiBearingY - metrics.height ) * conversionFactor;

			firstGlyphLink = glyphLink;
		}
		else
		{
			FT_Glyph_Metrics prevMetrics;
			prevGlyphLink->GetMetrics( prevMetrics );

			glyphLink->dx = GLfloat( prevMetrics.horiAdvance ) * conversionFactor;
			glyphLink->dy = 0.f;

			glyphLink->x = GLfloat( metrics.horiBearingX ) * conversionFactor;
			glyphLink->y = GLfloat( metrics.horiBearingY - metrics.height ) * conversionFactor;

			prevGlyphLink->nextGlyphLink = glyphLink;
		}

		prevGlyphLink = glyphLink;
	}

	return firstGlyphLink;
}

void Font::KernGlyphChain( GlyphLink* glyphLink, GLfloat conversionFactor )
{
	GlyphLink* prevGlyphLink = nullptr;

	while( glyphLink )
	{
		if( prevGlyphLink && prevGlyphLink->glyph && glyphLink->glyph )
		{
			FT_ULong key = MakeKerningKey( prevGlyphLink->glyph->GetIndex(), glyphLink->glyph->GetIndex() );
			KerningMap::iterator iter = kerningMap.find( key );
			if( iter != kerningMap.end() )
			{
				FT_Vector kerning = iter->second;
				glyphLink->dx += GLfloat( kerning.x ) * conversionFactor;
			}
		}

		prevGlyphLink = glyphLink;
		glyphLink = glyphLink->nextGlyphLink;
	}
}

void Font::RenderGlyphChain( GlyphLink* glyphLink, GLfloat ox, GLfloat oy )
{
	while( glyphLink )
	{
		ox += glyphLink->dx;
		oy += glyphLink->dy;

		GLuint texture = 0;
		if( glyphLink->glyph )
			texture = glyphLink->glyph->GetTexture();

		// Note that if the 0-texture is bound, we should just draw a solid quad.
		glBindTexture( GL_TEXTURE_2D, texture );
		glBegin( GL_QUADS );

		glTexCoord2f( 0.f, 0.f );	glVertex2f( ox + glyphLink->x, oy + glyphLink->y );
		glTexCoord2f( 1.f, 0.f );	glVertex2f( ox + glyphLink->x + glyphLink->w, oy + glyphLink->y );
		glTexCoord2f( 1.f, 1.f );	glVertex2f( ox + glyphLink->x + glyphLink->w, oy + glyphLink->y + glyphLink->h );
		glTexCoord2f( 0.f, 1.f );	glVertex2f( ox + glyphLink->x, oy + glyphLink->y + glyphLink->h );

		glEnd();

		glyphLink = glyphLink->nextGlyphLink;
	}
}

void Font::DeleteGlyphChain( GlyphLink* glyphLink )
{
	while( glyphLink )
	{
		GlyphLink* nextGlyphLink = glyphLink->nextGlyphLink;
		delete glyphLink;
		glyphLink = nextGlyphLink;
	}
}

GLfloat Font::CalcGlyphChainLength( GlyphLink* glyphLink )
{
	GLfloat length = 0.f;

	while( glyphLink )
	{
		if( glyphLink->nextGlyphLink )
			length += glyphLink->nextGlyphLink->dx;
		else
			length += glyphLink->x + glyphLink->w;

		glyphLink = glyphLink->nextGlyphLink;
	}

	return length;
}

// TODO: We should force a break on new-line characters.
Font::GlyphLink* Font::BreakGlyphChain( GlyphLink* glyphLink )
{
	GLfloat ox = 0.f;

	GlyphLink* prevGlyphLink = nullptr;
	GlyphLink* glyphLinkBreak = nullptr;
	GlyphLink* prevGlyphLinkBreak = nullptr;

	while( true )
	{
		if( !glyphLink )
			return nullptr;		// All glyphs are in bounds.

		ox += glyphLink->dx;

		if( ox + glyphLink->x + glyphLink->w >= fontSystem->GetLineWidth() )
			break;				// We reached a glyph out of bounds.

		if( !glyphLink->glyph || glyphLink->glyph->GetCharCode() == ' ' )
		{
			glyphLinkBreak = glyphLink;
			prevGlyphLinkBreak = prevGlyphLink;
		}

		prevGlyphLink = glyphLink;
		glyphLink = glyphLink->nextGlyphLink;
	}

	if( !prevGlyphLinkBreak )
		return nullptr;

	prevGlyphLinkBreak->nextGlyphLink = nullptr;
	return glyphLinkBreak;
}

void Font::JustifyGlyphChain( GlyphLink* glyphLink )
{
	GLfloat length = CalcGlyphChainLength( glyphLink );
	GLfloat delta = fontSystem->GetLineWidth() - length;

	switch( fontSystem->GetJustification() )
	{
		case System::JUSTIFY_LEFT:
		{
			// We assume that the given chain is already left-justify; so in this case, we're done!
			break;
		}
		case System::JUSTIFY_RIGHT:
		{
			glyphLink->dx += delta;
			break;
		}
		case System::JUSTIFY_CENTER:
		{
			glyphLink->dx += delta / 2.f;
			break;
		}
		case System::JUSTIFY_LEFT_AND_RIGHT:
		{
			int spaceCount = CountGlyphsInChain( glyphLink, ' ' );
			if( spaceCount > 0 )
			{
				delta /= GLfloat( spaceCount );

				// TODO: We may want to leave it left-justified if the delta is too big.
				while( glyphLink )
				{
					if( glyphLink->glyph && glyphLink->glyph->GetCharCode() == ' ' )
						glyphLink->dx += delta;

					glyphLink = glyphLink->nextGlyphLink;
				}
			}

			break;
		}
	}
}

int Font::CountGlyphsInChain( GlyphLink* glyphLink, FT_ULong charCode )
{
	int count = 0;

	while( glyphLink )
	{
		if( glyphLink->glyph && glyphLink->glyph->GetCharCode() == charCode )
			count++;

		glyphLink = glyphLink->nextGlyphLink;
	}

	return count;
}

Glyph::Glyph( void )
{
	texture = 0;
	glyphIndex = 0;
	charCode = 0;
}

/*virtual*/ Glyph::~Glyph( void )
{
	Finalize();
}

bool Glyph::Initialize( FT_GlyphSlot& glyphSlot, FT_UInt glyphIndex, FT_ULong charCode )
{
	bool success = false;
	
	do
	{
		this->glyphIndex = glyphIndex;
		this->charCode = charCode;

		FT_Bitmap& bitmap = glyphSlot->bitmap;
		if( bitmap.pixel_mode != FT_PIXEL_MODE_GRAY )
			break;

		if( bitmap.pitch != bitmap.width )
			break;

		metrics = glyphSlot->metrics;

		GLuint width = bitmap.width;
		GLuint height = bitmap.rows;

		// Note that the formatting code will depend on the texture map fitting the glyph as tightly as possible.
		// Some glyphs are just spaces, in which cases, there will be no buffer.
		GLubyte* bitmapBuffer = ( GLubyte* )bitmap.buffer;
		if( bitmapBuffer != nullptr )
		{
			glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

			glGenTextures( 1, &texture );
			if( texture == 0 )
				break;

			glBindTexture( GL_TEXTURE_2D, texture );

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

			GLuint bytesPerTexel = 4;
			GLubyte* textureBuffer = new GLubyte[ width * height * bytesPerTexel ];

			// We have to flip the image for OpenGL.
			for( GLuint i = 0; i < height; i++ )
			{
				for( GLuint j = 0; j < width; j++ )
				{
					GLubyte grey = bitmapBuffer[ ( height - i - 1 ) * width + j ];

					GLubyte* texel = &textureBuffer[ ( i * width + j  ) * bytesPerTexel ];

					texel[0] = grey;
					texel[1] = grey;
					texel[2] = grey;
					texel[3] = grey;
				}
			}

			gluBuild2DMipmaps( GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA, GL_UNSIGNED_BYTE, textureBuffer );

			delete[] textureBuffer;
		}

		success = true;
	}
	while( false );

	return success;
}

bool Glyph::Finalize( void )
{
	if( texture != 0 )
		glDeleteTextures( 1, &texture );

	return true;
}

// System.cpp