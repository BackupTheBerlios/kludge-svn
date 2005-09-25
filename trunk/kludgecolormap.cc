/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include <math.h>
#include <string.h>

#include "kludgecolormap"

double (*KludgeColorMap::colormap_tbl[5])(double,ColorName)={jet,autumn,cool,gray,hot};

const char KludgeColorMap::map_names[5][7]={"JET", "AUTUMN", "COOL", "GRAY", 
                                            "HOT"};
double
KludgeColorMap::saturate(double val)
{
  return (val<=0.0)?0.0:((val>=1.0)?1.0:val);
}

double
KludgeColorMap::jet(double val, ColorName cn)
{
  const double ev=1.0/16.0;
 
  switch(cn)
  {
    case RED: return (val<6.0*ev)?0.0:
      ((val<10.0*ev)?(val-6.0*ev)*4.0:
      ((val<14.0*ev)?1.0:(14.0*ev-val)/4.0/ev+1.0));
    case GREEN: return (val<2.0*ev)?0.0:
      ((val<6.0*ev)?(val-2.0*ev)*4.0:
      ((val<10.0*ev)?1.0:
      ((val<14.0*ev)?(10.0*ev-val)/4.0/ev+1.0:0.0)));          
    case BLUE: return (val<1.0*ev)?val*4.0+12.0*ev:
      ((val<5.0*ev)?1.0:
      ((val<9.0*ev)?(5.0*ev-val)/4.0/ev+1.0:0.0));    
  }
}

double
KludgeColorMap::autumn(double val, ColorName cn)
{
  switch(cn)
  {
    case RED:   return 1.0;
    case GREEN: return val;
    case BLUE:  return 0.0;
  }
}

double
KludgeColorMap::cool(double val, ColorName cn)
{
  switch(cn)
  {
    case RED:   return val;
    case GREEN: return 1.0-val;
    case BLUE:  return 1.0;
  }
}

double
KludgeColorMap::gray(double val, ColorName cn)
{
  return val;
}

double
KludgeColorMap::hot(double val, ColorName cn)
{
  const double ev=3.0/8.0;  
  
  switch(cn)
  {
    case RED:   return (val<ev)?val/ev:1.0;
    case GREEN: return (val<ev)?0.0:((val<2.0*ev)?(val-ev)/ev:1.0);
    case BLUE:  return (val<2.0*ev)?0.0:(val-2.0*ev)/(1.0-2.0*ev);
  }
}

double
KludgeColorMap::r(double val, ColorMapId cid)
{
  return (colormap_tbl[cid])(saturate(val),RED);
}

double
KludgeColorMap::g(double val, ColorMapId cid)
{
  return (colormap_tbl[cid])(saturate(val),GREEN);
}

double
KludgeColorMap::b(double val, ColorMapId cid)
{
  return (colormap_tbl[cid])(saturate(val),BLUE);
}

const char* 
KludgeColorMap::map2name(ColorMapId map)
{
  return map_names[map];
}

KludgeColorMap::ColorMapId 
KludgeColorMap::name2map(const char* mn)
{
  int cnt;
  for(cnt=0;cnt<5;cnt++)
    if(0==strcmp(map_names[cnt],mn)) return (ColorMapId)cnt;
  
  return JET; //default choice?
}
