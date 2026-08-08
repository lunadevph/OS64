#ifndef OS64_IMAGE_VIEWER_H
#define OS64_IMAGE_VIEWER_H
#include <stddef.h>
typedef enum image_format{IMAGE_NONE,IMAGE_PPM,IMAGE_BMP}image_format_t;
typedef struct image_document{unsigned char*data;size_t size,offset,stride;unsigned width,height,max_value,bpp;int top_down;image_format_t format;char status[80];}image_document_t;
void image_document_init(image_document_t*image);
int image_open(image_document_t*image,const char*path);
void image_close(image_document_t*image);
void image_render(const image_document_t*image,int x,int y,int width,int height);
#endif
