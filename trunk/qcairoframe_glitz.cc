/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "qcairoframe"

QCairoFrame::CairoGlitz::CairoGlitz(QCairoFrame* parent, const char* name)
                        :QCairoFrame::CairoBack(parent), QWidget(parent,name)
{
  g_drw_fmt_templ.types.window=1;  
  g_drw_fmt_templ.doublebuffer=1;
  g_drw_fmt_templ.depth_size=24;
  g_drw_fmt_templ.color.red_size=8;
  g_drw_fmt_templ.color.green_size=8;
  g_drw_fmt_templ.color.blue_size=8;
  g_drw_fmt_templ.color.alpha_size=8;  
  g_drw_fmt_mask=GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
                 GLITZ_FORMAT_BLUE_SIZE_MASK | GLITZ_FORMAT_ALPHA_SIZE_MASK |
                 GLITZ_FORMAT_DOUBLEBUFFER_MASK | GLITZ_FORMAT_WINDOW_MASK;
    
  s_f_templ.color.red_size=8;
  s_f_templ.color.green_size=8;
  s_f_templ.color.blue_size=8;
  s_f_templ.color.alpha_size=8;    
  s_f_templ.type=GLITZ_FORMAT_TYPE_COLOR;
  s_f_mask=GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
           GLITZ_FORMAT_BLUE_SIZE_MASK | GLITZ_FORMAT_ALPHA_SIZE_MASK |
           GLITZ_FORMAT_TYPE_MASK;
      
  g_disp=x11Display();
  Window p_win=RootWindow(g_disp,x11Screen());
  if(parentWidget()) p_win=parentWidget()->winId();
  glitz_glx_init(0);
  
  g_drw_fmt=glitz_glx_find_drawable_format(g_disp,
                                           x11Screen(),
                                           g_drw_fmt_mask,&g_drw_fmt_templ,0);
  if(g_drw_fmt==0) throw("glitz_glx_find_drawable_format");  
  
  XVisualInfo *vi=glitz_glx_get_visual_info_from_format(g_disp,x11Screen(),
                                                        g_drw_fmt);
  xswa.colormap=XCreateColormap(g_disp,p_win,vi->visual,AllocNone);
  g_win=XCreateWindow(g_disp,p_win,x(),y(),width(),height(),0,vi->depth,
                      InputOutput,vi->visual,CWColormap,&xswa);
  create(g_win);
  XFree(vi);
  
  g_drw=glitz_glx_create_drawable_for_window(g_disp,x11Screen(),g_drw_fmt,
                                             g_win,width(),height());        
  if(g_drw==0) throw("glitz_glx_drawable");      
  
  s_fmt=glitz_find_format(g_drw,s_f_mask,&s_f_templ,0);    
  s=glitz_surface_create(g_drw,s_fmt,width(),height(),0,0);    
  glitz_surface_attach(s,g_drw,GLITZ_DRAWABLE_BUFFER_BACK_COLOR,0,0);
  
  cs=cairo_glitz_surface_create(s);
  c=cairo_create(cs);
  
  back_color.red=0xffff; back_color.green=0xffff; back_color.blue=0xffff; 
  back_color.alpha=0xffff;
  setFocusPolicy(StrongFocus); 
  surf_list.setAutoDelete(true);
}

QCairoFrame::CairoGlitz::~CairoGlitz()
{
  surf_list.clear();
  cairo_destroy(c);
  cairo_surface_destroy(cs);
  
  glitz_surface_detach(s);
  glitz_surface_destroy(s);  
  glitz_drawable_destroy(g_drw);
  XFreeColormap(g_disp,xswa.colormap);  
  glitz_glx_fini();
}

void
QCairoFrame::CairoGlitz::update()
{
  QWidget::update();
}

void 
QCairoFrame::CairoGlitz::resize(int w, int h)
{
   QWidget::resize(w,h);
}
  
void 
QCairoFrame::CairoGlitz::setBackgroundColor(double red, double green, double blue)
{
  back_color.red=(unsigned short)(red*0xffff);
  back_color.green=(unsigned short)(green*0xffff);
  back_color.blue=(unsigned short)(blue*0xffff);
}

void 
QCairoFrame::CairoGlitz::setDefault()
{
  cairo_identity_matrix(c);
}

void
QCairoFrame::CairoGlitz::resizeEvent(QResizeEvent* re)
{
  cairo_destroy(c);
  cairo_surface_destroy(cs);
  glitz_surface_detach(s);
  glitz_surface_destroy(s);
  glitz_drawable_update_size(g_drw,re->size().width(),re->size().height());
  s=glitz_surface_create(g_drw,s_fmt,re->size().width(),re->size().height(),0,0);
  glitz_surface_attach(s,g_drw,GLITZ_DRAWABLE_BUFFER_BACK_COLOR,0,0);
  
  cs=cairo_glitz_surface_create(s);
  c=cairo_create(cs);
}


void
QCairoFrame::CairoGlitz::paintEvent(QPaintEvent* pe)
{
  glitz_set_rectangle(s,&back_color,0,0,width(),height());

  my_cairo_frame->cairoDraw(c);  
  glitz_drawable_swap_buffers(g_drw);
}

void 
QCairoFrame::CairoGlitz::translateAbs(double tx, double ty)
{  
  cairo_device_to_user_distance(c,&tx,&ty);
  cairo_translate(c,tx,ty);
}

void 
QCairoFrame::CairoGlitz::translate(double tx, double ty)
{
  cairo_translate(c,tx,ty);
}

void 
QCairoFrame::CairoGlitz::scale(double sx, double sy)
{
  cairo_scale(c,sx,sy);
}

void 
QCairoFrame::CairoGlitz::rotate(double angle)
{
  cairo_rotate(c,angle);
}

QCairoFrame::CairoGlitz::SurfListItem::~SurfListItem()
{
  cairo_surface_destroy(c_surf);
  glitz_surface_destroy(g_surf);
  glitz_buffer_destroy(g_buf);
}

cairo_surface_t* 
QCairoFrame::CairoGlitz::createImage(const QImage& qimg)
{
  qimg.convertDepth(32);
  glitz_pixel_format_t p_fmt;
  p_fmt.masks.bpp=32;
  p_fmt.masks.alpha_mask=0xff000000;
  p_fmt.masks.red_mask=0x00ff0000;
  p_fmt.masks.green_mask=0x0000ff00;
  p_fmt.masks.blue_mask=0x000000ff;
  p_fmt.xoffset=0;
  p_fmt.skip_lines=0;
  p_fmt.bytes_per_line=qimg.bytesPerLine();
  p_fmt.scanline_order=GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN;

  SurfListItem *ts=new SurfListItem();  
  ts->g_buf=glitz_pixel_buffer_create(g_drw,qimg.bits(),qimg.numBytes(),
                                      GLITZ_BUFFER_HINT_STATIC_READ);
  ts->g_surf=glitz_surface_create(g_drw,s_fmt,qimg.width(),qimg.height(),0,0);
  glitz_set_pixels(ts->g_surf,0,0,qimg.width(),qimg.height(),&p_fmt,ts->g_buf);  
  glitz_surface_set_filter(ts->g_surf,GLITZ_FILTER_BILINEAR,0,0);  
  ts->c_surf=cairo_glitz_surface_create(ts->g_surf);
  surf_list.insert((void*)(ts->c_surf),ts);  
  return ts->c_surf;  
}
    
void 
QCairoFrame::CairoGlitz::destroySurface(cairo_surface_t* surf)
{
  surf_list.remove((void*)surf);
}
