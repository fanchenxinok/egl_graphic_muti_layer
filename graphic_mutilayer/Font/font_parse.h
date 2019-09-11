#ifndef __FONT_PARSE_H__
#define __FONT_PARSE_H__
#include <wchar.h>

#define FT_DRAW_TEXT_EGL	(1) // 直接将文字位图画在texture上

#if FT_DRAW_TEXT_EGL
#include <GLES3/gl3.h>
typedef unsigned int FT_BUFF_HANDLE;
#else
typedef unsigned char* FT_BUFF_HANDLE;
#endif

typedef enum
{
	FT_FALSE = 0,
	FT_TRUE = 1
}FT_BOOL;

typedef struct
{	int canvas_x;				// 文字在画布中的起始位置而不是屏幕的位置
	int canvas_y;
	int canvas_w;		// 文字显示画布的高宽
	int canvas_h;
	wchar_t* draw_string; //文字信息 例如：L"中国hello world!"
	FT_BOOL need_antialias; //需要平滑处理
	FT_BOOL bold; //黑体字
	FT_BOOL italic; // 斜体字
	int line_distance; // 行间距 默认2个像素 最大10个像素
	FT_BUFF_HANDLE buffer_hdl; //得到argb buffer or Texture ID
}stFTFontDrawInfo;

int ft_init(void);
void ft_deinit(void);
int ft_font_create(const char* font_file, int font_pixel);
void ft_font_delete(int font_id);
int ft_draw_text(int font_id, stFTFontDrawInfo *pDrawInfo);

#endif
