/*
 * Copyright 2010 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "graphic/graphic.h"
#include "graphic/render/surface_opengl.h"

#include "log.h"
#include "upcast.h"
#include "wexception.h"

#include <SDL.h>


#ifdef USE_OPENGL

/*
 * Updating the whole Surface
 */
void SurfaceOpenGL::update() {
	assert(g_opengl);
	SDL_GL_SwapBuffers();
}

/*
===============
Draws the outline of a rectangle
===============
*/
void SurfaceOpenGL::draw_rect(const Rect rc, const RGBColor clr) {
	assert(g_opengl);
	//log("SurfaceOpenGL::draw_rect() for opengl is experimental\n");
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	
	glBegin(GL_LINE_LOOP);
		glColor3f
		((clr.r() / 256.0f),
			(clr.g() / 256.0f),
			(clr.b() / 256.0f));
		glVertex2f(rc.x + 1,            rc.y);
		glVertex2f(rc.x + rc.w, rc.y);
		glVertex2f(rc.x + rc.w, rc.y + rc.h - 1);
		glVertex2f(rc.x,            rc.y + rc.h -1 );
	glEnd();
	glEnable(GL_TEXTURE_2D);
}


/*
===============
Draws a filled rectangle
===============
*/
void SurfaceOpenGL::fill_rect(const Rect rc, const RGBAColor clr) {
	assert(rc.x >= 0);
	assert(rc.y >= 0);
	assert(rc.w >= 1);
	assert(rc.h >= 1);
	assert(g_opengl);
		/*log("SurfaceOpenGL::fill_rect((%d, %d, %d, %d),(%u, %u, %u, %u)) for opengl\n",
		    rc.x, rc.y, rc.w, rc.h,
		    clr.r, clr.g, clr.b, clr.a);*/
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glBegin(GL_QUADS);
		glColor4f
		(((static_cast<GLfloat>(clr.r)) / 256.0f),
			((static_cast<GLfloat>(clr.g)) / 256.0f),
			((static_cast<GLfloat>(clr.b)) / 256.0f),
			((static_cast<GLfloat>(clr.a)) / 256.0f));
		glVertex2f(rc.x ,       rc.y);
		glVertex2f(rc.x + rc.w, rc.y);
		glVertex2f(rc.x + rc.w, rc.y + rc.h);
		glVertex2f(rc.x ,       rc.y + rc.h);
	glEnd();
	glEnable(GL_TEXTURE_2D);
}

/*
===============
Change the brightness of the given rectangle
This function is slow as hell.

* This function is a possible point to optimize on
  slow system. It takes a lot of cpu time atm and is
  not needed. It is used by the ui_basic stuff to
  highlight things.
===============
*/
void SurfaceOpenGL::brighten_rect(const Rect rc, const int32_t factor) {
	if(!factor)
		return;
	assert(rc.x >= 0);
	assert(rc.y >= 0);
	assert(rc.w >= 1);
	assert(rc.h >= 1);

	assert(g_opengl);
	/* glBlendFunc is a very nice feature of opengl. You can specify how the 
		* color is calculated.
		* 
		* glBlendFunc(GL_ONE, GL_ONE) means the following:
		* Rnew = Rdest + Rsrc 
		* Gnew = Gdest + Gsrc
		* Bnew = Bdest + Bsrc
		* Anew = Adest + Asrc
		* where Xnew is the new calculated color for destination, Xdest is the old 
		* color of the destination and Xsrc is the color of the source.
		*/
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	// And now simply draw a rect with facor as the color (this is the source color)
	// over the region
	glBegin(GL_QUADS);
		glColor3f
		((factor / 256.0f),
			(factor / 256.0f),
			(factor / 256.0f));
		glVertex2f(rc.x,        rc.y);
		glVertex2f(rc.x + rc.w, rc.y);
		glVertex2f(rc.x + rc.w, rc.y + rc.h);
		glVertex2f(rc.x,        rc.y + rc.h);
	glEnd();
}


/*
===============
Clear the entire bitmap to black
===============
*/
void SurfaceOpenGL::clear() {
	assert(g_opengl);
	//log("SurfaceOpenGL::clear() for opengl mode\n");
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear(GL_COLOR_BUFFER_BIT);
	return;

}

/*
===============

===============
*/
/*
GLuint SurfaceOpenGL::getTexture()
{
	if(!m_glTexUpdate and m_texture)
		return m_texture->id();
	
	//log("Warning: SurfaceOpenGL::getTexture() creates Texture from Surface\n");

	GLuint texture;
	SDL_Surface *surface;
	GLenum pixels_format, pixels_type;
	GLint  Bpp;

	surface=m_surface;

	SDL_Surface * tsurface = NULL;
	//SDL_PixelFormat & fmt = *surface->format;
	
	if(surface->format->palette or (surface->format->colorkey > 0))
	{
		//log("Warning: trying to use a paletted picture for opengl texture\n");
		SDL_Surface * tsurface = SDL_DisplayFormatAlpha(surface);
		SDL_BlitSurface(surface, 0, tsurface, 0);
		surface = tsurface;
	}
	
	SDL_PixelFormat const & fmt = *surface->format;
	Bpp = fmt.BytesPerPixel;

	log
		("SurfaceOpenGL::getTexture() Size: (%d, %d) %db(%dB) ", get_w(), get_h(),
		 fmt.BitsPerPixel, Bpp);

	log("R:%X, G:%X, B:%X, A:%X", fmt.Rmask, fmt.Gmask, fmt.Bmask, fmt.Amask);
		 
	if(Bpp==4) {
		if(fmt.Rmask==0x000000ff and fmt.Gmask==0x0000ff00 and fmt.Bmask==0x00ff0000) {
			if(fmt.Amask==0xff000000) {
				pixels_format=GL_RGBA; log(" RGBA 8888 ");
			} else {
				pixels_format=GL_RGB; log(" RGB 8880 ");
			}
		} else if(fmt.Bmask==0x000000ff and fmt.Gmask==0x0000ff00 and fmt.Rmask==0x00ff0000) {
			if(fmt.Amask==0xff000000) { 
				pixels_format=GL_BGRA; log(" RGBA 8888 ");
			} else {
				pixels_format=GL_BGR; log(" RGBA 8888 ");
			}
		} else
			assert(false);
		pixels_type=GL_UNSIGNED_INT_8_8_8_8_REV;
	} else if (Bpp==3) {
		if(fmt.Rmask==0x000000ff and fmt.Gmask==0x0000ff00 and fmt.Bmask==0x00ff0000) {
			pixels_format=GL_RGB; log(" RGB 888 ");
		} else
			assert(false);
		pixels_type=GL_UNSIGNED_BYTE;
	} else if (Bpp==2) {
		if((fmt.Rmask==0xF800) and (fmt.Gmask==0x7E0) and (fmt.Bmask==0x1F)) {
			pixels_format=GL_RGB; log(" RGB 565"); 
		} else if ((fmt.Bmask==0xF800) and (fmt.Gmask==0x7E0) and (fmt.Rmask==0x1F)) {
			pixels_format=GL_BGR; log(" BGR 565"); 
		} else
			assert(false);
		pixels_type = GL_UNSIGNED_SHORT_5_6_5;
	} else
		assert(false);
	log("\n");

	// Let OpenGL create a texture object
	glGenTextures( 1, &texture );

	// selcet the texture object
	glBindTexture( GL_TEXTURE_2D, texture );

	// set texture filter to siply take the nearest pixel.
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	SDL_LockSurface(surface);

	glTexImage2D( GL_TEXTURE_2D, 0, Bpp, surface->w, surface->h, 0,
	pixels_format, pixels_type, surface->pixels );
	SDL_UnlockSurface(surface);
	
	if(tsurface)
		SDL_FreeSurface(tsurface);

	//SDL_FreeSurface(m_surface);
	if(m_texture)
		delete m_texture;
	m_texture = new oglTexture(texture);
	m_glTexUpdate = false;
	return texture;
}
*/

void SurfaceOpenGL::blit(Point const dst, Surface * const src, Rect const srcrc, bool enable_alpha)
{
	assert(g_opengl);
	if (m_surf_type != SURFACE_SCREEN)
	{
		log("ERROR: SurfaceOpenGL::blit(): Destination surface is not the screen\n");
		return;
	}

	upcast(SurfaceOpenGL, oglsrc, src);

#if defined(DEBUG)
	if (not oglsrc)
	{
		log("ERROR: SurfaceOpenGL::blit(): Source surface is not an opengl surface\n");
		throw wexception("Invalid Surface: Not a opengl surface");
	}
#endif

	GLuint tex;

	try {
		tex = oglsrc->get_texture();
	} catch (...) {
		log("WARNING: SurfaceOpenGL::blit(): Source surface has no texture\n");
		return;
	}

	/* Set a texture scaling factor. Normaly texture coordiantes 
	* (see glBegin()...glEnd() Block below) are given in the range 0-1
	* to avoid the calculation (and let opengl do it) the texture 
	* space is modified. glMatrixMode select which matrix to manipulate
	* (the texture transformation matrix in this case). glLoadIdentity()
	* resets the (selected) matrix to the identity matrix. And finally 
	* glScalef() calculates the texture matrix.
	*/
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(1.0f/static_cast<GLfloat>(src->get_w()), 1.0f/static_cast<GLfloat>(src->get_h()), 1);

	// Enable Alpha blending 
	if(enable_alpha) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else
		glDisable(GL_BLEND);

	/* select the texture to paint on the screen
		* openGL does not know anything about SDL_Surfaces
		* opengl uses textures to handle images
		* getTexture() returns the texture id of the Surface. It creates
		* the texture from the SDL_Surface if it doesn't exist
		*/
	
	glBindTexture( GL_TEXTURE_2D, tex);
	
	/* This block between blBegin() and glEnd() does the blit.
		* It draws a textured rectangle. glTexCoord2i() set the Texture
		* Texture cooardinates. This is the source rectangle.
		* glVertex2f() sets the screen coordiantes which belong to the 
		* previous texture coordinate. This is the destination rectangle 
		*/
	unsigned int dst_w, dst_h;
	if (oglsrc->get_dest_w() > 0) {
		dst_w = oglsrc->get_dest_w();
		dst_h = oglsrc->get_dest_h();
	} else {
		dst_w = srcrc.w;
		dst_h = srcrc.h;
	}

	glBegin(GL_QUADS);
		//set color white, otherwise textures get mixed with color
		glColor3f(1.0,1.0,1.0);
		//top-left 
		glTexCoord2i( srcrc.x,         srcrc.y );
		glVertex2i(   dst.x,           dst.y );
		//top-right
		glTexCoord2i( srcrc.x+srcrc.w, srcrc.y );
		glVertex2f(   dst.x+dst_w,   dst.y );
		//botton-right
		glTexCoord2i( srcrc.x+srcrc.w, srcrc.y+srcrc.h );
		glVertex2f(   dst.x+dst_w,   dst.y+dst_h );
		//bottom-left
		glTexCoord2i( srcrc.x,         srcrc.y + srcrc.h);
		glVertex2f(   dst.x,           dst.y+dst_h );
	glEnd();

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
}

void SurfaceOpenGL::blit(Rect dstrc, Surface * src, Rect srcrc, bool enable_alpha) {
	assert(g_opengl);
	assert(m_surf_type != SURFACE_SCREEN);
	upcast(SurfaceOpenGL, oglsrc, src);
	assert(oglsrc);

	GLuint tex;

	try {
		tex = oglsrc->get_texture();
	} catch (...) {
		log("WARNING: SurfaceOpenGL::blit(): Source surface has no texture\n");
		return;
	}

	/* Set a texture scaling factor. Normaly texture coordiantes 
	* (see glBegin()...glEnd() Block below) are given in the range 0-1
	* to avoid the calculation (and let opengl do it) the texture 
	* space is modified. glMatrixMode select which matrix to manipulate
	* (the texture transformation matrix in this case). glLoadIdentity()
	* resets the (selected) matrix to the identity matrix. And finally 
	* glScalef() calculates the texture matrix.
	*/
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(1.0f/static_cast<GLfloat>(src->get_w()), 1.0f/static_cast<GLfloat>(src->get_h()), 1);

	// Enable Alpha blending 
	if(enable_alpha) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else
		glDisable(GL_BLEND);

	/* select the texture to paint on the screen
		* openGL does not know anything about SDL_Surfaces
		* opengl uses textures to handle images
		* getTexture() returns the texture id of the Surface. It creates
		* the texture from the SDL_Surface if it doesn't exist
		*/
	
	glBindTexture( GL_TEXTURE_2D, tex);
	
	/* This block between blBegin() and glEnd() does the blit.
		* It draws a textured rectangle. glTexCoord2i() set the Texture
		* Texture cooardinates. This is the source rectangle.
		* glVertex2f() sets the screen coordiantes which belong to the 
		* previous texture coordinate. This is the destination rectangle 
		*/
	glBegin(GL_QUADS);
		//set color white, otherwise textures get mixed with color
		glColor3f(1.0,1.0,1.0);
		//top-left 
		glTexCoord2i( srcrc.x,         srcrc.y );
		glVertex2i(   dstrc.x,         dstrc.y );
		//top-right
		glTexCoord2i( srcrc.x + srcrc.w, srcrc.y );
		glVertex2f(   dstrc.x + dstrc.w, dstrc.y );
		//botton-right
		glTexCoord2i( srcrc.x + srcrc.w, srcrc.y + srcrc.h );
		glVertex2f(   dstrc.x + dstrc.w, dstrc.y + dstrc.h );
		//bottom-left
		glTexCoord2i( srcrc.x,           srcrc.y + srcrc.h);
		glVertex2f(   dstrc.x,           dstrc.y + dstrc.h );
	glEnd();

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
}


#endif //USE_OPENGL