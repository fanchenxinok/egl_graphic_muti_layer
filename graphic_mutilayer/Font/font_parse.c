#include <stdio.h>
#include <ft2build.h>
#include <freetype/fttypes.h> //FT_TYPES_H
#include <freetype/ftglyph.h>  //FT_GLYPH_H
#include <freetype/tttables.h> //FT_TRUETYPE_TABLES_H
#include <freetype/ftbitmap.h> //FT_BITMAP_H
#include <freetype/ftwinfnt.h> //FT_WINFONTS_H
#include <freetype/fterrors.h> // FT_ERRORS_H

#include "linux_win_api.h"
#include "font_parse.h"
#include "list.h"

/* xMin           xMax  
|	| ____width____|__ yMax|
|	|   +*****++**+|       |                                                           
|	| +**+ +*****+ |       |                                                          
|	|  **+   +** + |       |                                                           
|	|  **     **   |       |                                                           
|	|  **     **   |       |                                                           
|	|  **+   +**   |       |                                                           
|	|  +**+ +**+   |       |                                                           
|	|   +*****+    |height |                                                                 
|	|  +*+         |       |                                                           
|	|  +***+++     |       |                                                           
|---|- ++******++--|-------|-----------                                                                  
|O	| +**++++****+ |       |O                                                           
|	| **+     ++** |       |                                                           
|	| **       +** |       |                                                           
|	| **+     +**+ |       |                                                           
|	| +********+   |       |
|   ------------------yMin | 
|________advance___________|

bearingX为原点O到xMin的距离
bearingY为原点O到yMax的距离
*/


#define FT_ID_BASE	(0xaa00)

typedef struct
{
	int    width;
	int    height;
	int    horiBearingX;
	int    horiBearingY;
	int    horiAdvance;
	//int    vertBearingX;
	//int    vertBearingY;
	//int    vertAdvance;
}stFTGlyphMetrics;

typedef struct
{
	int ft_char_code;
	stFTGlyphMetrics ft_char_glyphmetrics;
}stFTCharInfo;

typedef struct
{
	FT_UShort   ft_font_id;
	FT_UShort 	ft_font_pixel;
	FT_Face 	ft_font_face;
	FT_Byte*	ft_font_rgba; // unsigned char*
	stList*		ft_char_list;
}stFTFontInfo;

typedef struct
{
	FT_Library  	ft_library;
	stList*			ft_font_list;
	LW_MUTEX		ft_lock;
}stFTFontMng;

static stFTFontMng s_ft_mng = {NULL};

static void* ft_find_font(int font_id)
{
	stListNode *pCur = s_ft_mng.ft_font_list->pHead->pNext;
	while(pCur) {
		stFTFontInfo *pFTFontInfo = (stFTFontInfo*)pCur->data;
		if(pFTFontInfo && pFTFontInfo->ft_font_id == font_id){
			printf("find font ID: 0x%x\n", font_id);
			return (void*)pFTFontInfo;
		}
		pCur = pCur->pNext;
	}
	return NULL;
}

// bitmap.width  位图宽度
// bitmap.rows   位图行数（高度）
// bitmap.pitch  位图一行占用的字节数

//MONO模式每1个像素仅用1bit保存，只有黑和白。
//1个byte可以保存8个像素，1个int可以保存8*4个像素。
static void ft_convert_mono2rgba(FT_Bitmap *source, FT_Byte* rgba)
{
    FT_Byte* s = source->buffer;
    FT_Byte* t = rgba;

    for (FT_UInt y = source->rows; y > 0; y--) {
        FT_Byte* ss = s;
        FT_Byte* tt = t;

        for (FT_UInt x = source->width >> 3; x > 0; x--) {
            FT_UInt val = *ss;

            for (FT_UInt i = 8; i > 0; i--) {
                tt[0] = tt[1] = tt[2] = tt[3] = ( val & (1<<(i-1)) ) ? 0xFF : 0x00;
                tt += 4;
            }

            ss += 1;
        }

        FT_UInt rem = source->width & 7;

        if (rem > 0) {
            FT_UInt val = *ss;

            for (FT_UInt x = rem; x > 0; x--) {
                tt[0] = tt[1] = tt[2] = tt[3] = ( val & 0x80 ) ? 0xFF : 0x00;
                tt += 4;
                val <<= 1;
            }
        }

        s += source->pitch;
        t += source->width * 4;    //pitch
    }
}

//GRAY模式1个像素用1个字节保存。
static void ft_convert_gray2rgba(FT_Bitmap *source, FT_Byte *rgba)
{
    for (FT_UInt y = 0; y < source->rows; y++) {
        for (FT_UInt x = 0; x < source->width; x++) {
            FT_Byte *s = &source->buffer[(y * source->pitch) + x];
            FT_Byte *t = &rgba[((y * source->pitch) + x) * 4];

            t[0] = t[1] = t[2] = 0xFF;
            t[3] = *s;
        }
    }
}


static int ft_load_char(stFTFontInfo *pFTFontInfo, int code, stFTGlyphMetrics *metrics, FT_BOOL bold, FT_BOOL need_antialias)
{
    FT_Error err;

	// 将字符编码转换为一个字形(glyph)索引(Freetype默认是utf-16编码类型)
	// 如果使用其它字符编码,则通过FT_Select_Charmap()来获取
    FT_UInt glyph_index = FT_Get_Char_Index(pFTFontInfo->ft_font_face, (FT_ULong)code);

    if (glyph_index > 0) {
		// 根据字形索引,来将字形图像存储到字形槽(glyph slot)中.
        if ((err = FT_Load_Glyph(pFTFontInfo->ft_font_face, glyph_index, FT_LOAD_DEFAULT)) == FT_Err_Ok) {
            FT_GlyphSlot glyph = pFTFontInfo->ft_font_face->glyph;
			// FT_RENDER_MODE_NORMAL：表示生成位图每个像素是RGB888的
			// FT_RENDER_MODE_MONO :表示生成位图每个像素是1位的(黑白图)
            FT_Render_Mode render_mode = need_antialias ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO;

            if (need_antialias && bold) {
                if ((err = FT_Outline_Embolden(&glyph->outline, 60)) != FT_Err_Ok) {
                    printf("FT_Outline_Embolden() Error %d\n", err);
                }
            }

            if ((err = FT_Render_Glyph(glyph, render_mode)) == FT_Err_Ok) {
                FT_Bitmap *bitmap = &glyph->bitmap;
				//像素模式，FT_PIXEL_MODE_MONO 指单色的，FT_PIXEL_MODE_GRAY 表示反走样灰度值
				printf("bitmap->pixel_mode = 0x%x\n", bitmap->pixel_mode);
                switch (bitmap->pixel_mode)
                {
                	case FT_PIXEL_MODE_MONO:
                    {
                        if (!need_antialias && bold) {
                            if ((err = FT_Bitmap_Embolden(s_ft_mng.ft_library, bitmap, 60, 0)) != FT_Err_Ok) {
                                printf("FT_Bitmap_Embolden() Error %d\n", err);
                            }
                        }
                        ft_convert_mono2rgba(bitmap, pFTFontInfo->ft_font_rgba);
                        break;
                    }
                	case FT_PIXEL_MODE_GRAY:
                    {
                        ft_convert_gray2rgba(bitmap, pFTFontInfo->ft_font_rgba);
                        break;
                    }
                	default:
                    {
                        memset(pFTFontInfo->ft_font_rgba, 0xFF, pFTFontInfo->ft_font_pixel * 2 * pFTFontInfo->ft_font_pixel * 4);
                        break;
                    }
                }

                metrics->width = bitmap->width;	 //该位图总宽度,有多少列像素点	
                metrics->height = bitmap->rows;  //该位图总高度,有多少行                
                metrics->horiBearingX = glyph->bitmap_left;
                metrics->horiBearingY = glyph->bitmap_top;
                metrics->horiAdvance = glyph->advance.x >> 6; // == advance.x / 64

                return 0;
            }
            else {
                printf("FT_Render_Glyph() Error %d\n", err);
            }
        }
        else {
            printf("FT_Load_Glyph() Error %d\n", err);
        }
    }

    memset(metrics, 0, sizeof(stFTGlyphMetrics));

    return -1;
}

// return font_id
int ft_font_create(const char* font_file, int font_pixel)
{
	FT_Error err;
	static FT_UShort id = 0x00;
	stFTFontInfo *pFTFontInfo = (stFTFontInfo*)malloc(sizeof(stFTFontInfo));
	//打开一个字体文件，
	if ((err = FT_New_Face(s_ft_mng.ft_library, font_file, 0, &pFTFontInfo->ft_font_face)) != FT_Err_Ok) {
        printf("FT_New_Face() Error %d\n", err);
        return -1;
    }
	else {
		// 设置字符像素 0 表示与另一个尺寸相同
		if ((err = FT_Set_Pixel_Sizes(pFTFontInfo->ft_font_face, 0, font_pixel)) != FT_Err_Ok) {
	        printf("FT_Set_Pixel_Sizes() Error %d\n", err);
	        return -1;
	    }
	}

	lw_lock(&s_ft_mng.ft_lock);
	pFTFontInfo->ft_char_list = list_create();
	pFTFontInfo->ft_font_id = FT_ID_BASE + id++;
	pFTFontInfo->ft_font_pixel = font_pixel;
	pFTFontInfo->ft_font_rgba = (FT_Byte*)malloc(font_pixel * 2 * font_pixel * 4); // 用来存储rgba数据
	list_insert_last(s_ft_mng.ft_font_list, (void**)&pFTFontInfo);
	lw_unlock(&s_ft_mng.ft_lock);
	return pFTFontInfo->ft_font_id;
}


void ft_font_delete(int font_id)
{
    FT_Error err;

	lw_lock(&s_ft_mng.ft_lock);
	stFTFontInfo *pFTFontInfo = (stFTFontInfo*)ft_find_font(font_id);
	if(pFTFontInfo) {
		if (pFTFontInfo->ft_font_face) {
	        if ((err = FT_Done_Face(pFTFontInfo->ft_font_face)) != FT_Err_Ok) {
	            printf("FT_Done_Face() Error %d\n", err);
	        }
	        pFTFontInfo->ft_font_face = NULL;
			free(pFTFontInfo->ft_font_rgba);
	    }
		list_delete_node(s_ft_mng.ft_font_list, (void*)&pFTFontInfo);
	}	
	lw_unlock(&s_ft_mng.ft_lock);
}

#if FT_DRAW_TEXT_EGL
int ft_draw_text(int font_id, stFTFontDrawInfo *pDrawInfo)
{
	stFTFontInfo *pFTFontInfo = (stFTFontInfo*)ft_find_font(font_id);
	if(pFTFontInfo && pDrawInfo) {
		//注意： wchar_t在windows占2byte,在linux占4bytes.
		wchar_t *string = pDrawInfo->draw_string;

	    //创建纹理buffer
		glGenTextures(1, &pDrawInfo->buffer_hdl);
		GLint format = GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, pDrawInfo->buffer_hdl);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (format == GL_RGBA) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (format == GL_RGBA) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, format, pDrawInfo->canvas_w, pDrawInfo->canvas_h, 0, format, GL_UNSIGNED_BYTE, NULL);
		
		if(0 == pDrawInfo->buffer_hdl) {
			printf("ft_draw_text create texture fail.\n");
			return -1;
		}
		
		if (pDrawInfo->italic) { // 斜体字
	        FT_Matrix matrix;
	        matrix.xx = 0x10000L;
			matrix.xy = 0.5f * 0x10000L;
			matrix.yx = 0;
			matrix.yy = 0x10000L;
	        FT_Set_Transform(pFTFontInfo->ft_font_face, &matrix, NULL);
	    }

		if(pDrawInfo->line_distance <= 0) pDrawInfo->line_distance = 2;
		if(pDrawInfo->line_distance > 10) pDrawInfo->line_distance = 10;

		glBindTexture(GL_TEXTURE_2D, pDrawInfo->buffer_hdl);
		int total = wcslen(string), n = 0;
		int x_start = 0, y_start = pFTFontInfo->ft_font_pixel;
		for(n = 0; n < total; n++) {
			if (string[n] == L'\n') {
	            x_start = 0;
	            y_start += pFTFontInfo->ft_font_pixel + pDrawInfo->line_distance;    //row spacing
	            continue;
	        }

			stFTGlyphMetrics metrics;
			if(ft_load_char(pFTFontInfo, string[n], &metrics, pDrawInfo->bold, pDrawInfo->need_antialias) != -1) {
				int line = 0;
				if((x_start + metrics.width) > pDrawInfo->canvas_w) {
					metrics.width = pDrawInfo->canvas_w - x_start;
				}
				if((y_start + metrics.height) > pDrawInfo->canvas_h) {
					metrics.height = pDrawInfo->canvas_h - y_start;
				}
				int dst_stride = pDrawInfo->canvas_w * 4, src_stride = metrics.width * 4;
				x_start += metrics.horiBearingX;
				x_start = (x_start < 0) ? 0 : x_start;
				int y_adjust = y_start - metrics.horiBearingY;
				printf("[GlyphMetrics] w:%d, h:%d, horiBearingX:%d, horiBearingY:%d, horiAdvance:%d, x_start:%d\n",
					metrics.width,metrics.height,metrics.horiBearingX,metrics.horiBearingY,metrics.horiAdvance,x_start);
			
				glTexSubImage2D(GL_TEXTURE_2D, 0, x_start, y_adjust, metrics.width, metrics.height, GL_RGBA, GL_UNSIGNED_BYTE, pFTFontInfo->ft_font_rgba);
				x_start += metrics.horiAdvance;
				if (pDrawInfo->italic) {
					x_start += 2;
				}
			}
			else {
				printf("ft_draw_text load char: %d fail\n", n);
			}
		}
	}
	else {
		printf("This font is not create: %d\n", font_id);
		return -1;
	}
	return 0;
}

#else
int ft_draw_text(int font_id, stFTFontDrawInfo *pDrawInfo)
{
	stFTFontInfo *pFTFontInfo = (stFTFontInfo*)ft_find_font(font_id);
	if(pFTFontInfo && pDrawInfo) {
		//注意： wchar_t在windows占2byte,在linux占4bytes.
		wchar_t *string = pDrawInfo->draw_string;
		pDrawInfo->buffer_hdl = (FT_Bytes)malloc(pDrawInfo->canvas_w * pDrawInfo->canvas_h * 4);
		
		if(NULL == pDrawInfo->buffer_hdl) {
			printf("ft_draw_text malloc fail.\n");
			return -1;
		}

		memset(pDrawInfo->buffer_hdl, 0x0, pDrawInfo->canvas_w * pDrawInfo->canvas_h * 4);
		if (pDrawInfo->italic) { // 斜体字
	        FT_Matrix matrix;
	        matrix.xx = 0x10000L;
			matrix.xy = 0.5f * 0x10000L;
			matrix.yx = 0;
			matrix.yy = 0x10000L;
	        FT_Set_Transform(pFTFontInfo->ft_font_face, &matrix, NULL);
	    }

		if(pDrawInfo->line_distance <= 0) pDrawInfo->line_distance = 2;
		if(pDrawInfo->line_distance > 10) pDrawInfo->line_distance = 10;
		
		int total = wcslen(string), n = 0;
		int x_start = 0, y_start = pFTFontInfo->ft_font_pixel;
		for(n = 0; n < total; n++) {
			if (string[n] == L'\n') {
	            x_start = 0;
	            y_start += pFTFontInfo->ft_font_pixel + pDrawInfo->line_distance;    //row spacing
	            continue;
	        }

			stFTGlyphMetrics metrics;
			if(ft_load_char(pFTFontInfo, string[n], &metrics, pDrawInfo->bold, pDrawInfo->need_antialias) != -1) {
				int line = 0;
				if((x_start + metrics.width) > pDrawInfo->canvas_w) {
					metrics.width = pDrawInfo->canvas_w - x_start;
				}
				if((y_start + metrics.height) > pDrawInfo->canvas_h) {
					metrics.height = pDrawInfo->canvas_h - y_start;
				}
				int dst_stride = pDrawInfo->canvas_w * 4, src_stride = metrics.width * 4;
				x_start += metrics.horiBearingX;
				x_start = (x_start < 0) ? 0 : x_start;
				int y_adjust = y_start - metrics.horiBearingY;
				printf("[GlyphMetrics] w:%d, h:%d, horiBearingX:%d, horiBearingY:%d, horiAdvance:%d, x_start:%d\n",
					metrics.width,metrics.height,metrics.horiBearingX,metrics.horiBearingY,metrics.horiAdvance,x_start);
				FT_Bytes dst = pDrawInfo->buffer_hdl + y_adjust * dst_stride + x_start * 4;
				FT_Bytes src = pFTFontInfo->ft_font_rgba;
				for(line = 0; line < metrics.height; line++) {
					memcpy(dst, src, src_stride);
					dst += dst_stride;
					src += src_stride;
				}
				x_start += metrics.horiAdvance;
				if (pDrawInfo->italic) {
					x_start += 2;
				}
			}
			else {
				printf("ft_draw_text load char: %d fail\n", n);
			}
		}
	}
	else {
		printf("This font is not create: %d\n", font_id);
		return -1;
	}
	return 0;
}
#endif

int ft_init(void)
{
	FT_Error err;
	if ((err = FT_Init_FreeType(&s_ft_mng.ft_library)) != FT_Err_Ok) {
        printf("FT_Init_FreeType() Error %d\n", err);
        return -1;
    }
	else {
		lw_lock_init(&s_ft_mng.ft_lock);
		s_ft_mng.ft_font_list = list_create();
	}
	return 0;
}

void ft_deinit(void)
{
	FT_Error err;

	stListNode *pCur = s_ft_mng.ft_font_list->pHead->pNext;
	while(pCur) {
		stFTFontInfo *pFTFontInfo = (stFTFontInfo*)pCur->data;
		if(pFTFontInfo){
			ft_font_delete(pFTFontInfo->ft_font_id);
		}
		pCur = pCur->pNext;
	}

	list_destory(s_ft_mng.ft_font_list);

	if ((err = FT_Done_FreeType(s_ft_mng.ft_library)) != FT_Err_Ok) {
        printf("FT_Done_FreeType() Error %d\n");
    }
}

