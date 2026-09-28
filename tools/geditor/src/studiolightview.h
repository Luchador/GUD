#ifndef GEDITOR_STUDIOLIGHTVIEW_H
#define GEDITOR_STUDIOLIGHTVIEW_H
#include "studiogizmo.h"

#define STUDIO_LIGHT_ICON_SIZE 32
/* Context-owned textures; selection and projection also work without GL. */
typedef struct StudioLightIcons { unsigned int texture[2]; } StudioLightIcons;
BOOL StudioLightIconsLoad(StudioLightIcons *icons,HINSTANCE instance);
void StudioLightIconsFree(StudioLightIcons *icons);
BOOL StudioLightIconRect(const StudioLight *light,const OrbitCamera *camera,int width,int height,double rect[4],double *depth);
int StudioLightIconPick(const StudioScene *scene,const OrbitCamera *camera,int width,int height,double x,double y);
void StudioLightIconsDraw(const StudioLightIcons *icons,const StudioScene *scene,const OrbitCamera *camera,int width,int height,int selected);
#endif
