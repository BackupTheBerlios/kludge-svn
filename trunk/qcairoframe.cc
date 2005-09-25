/* This code is distributed under GNU GPL version >= 2 *
 *        http://www.gnu.org/licenses/gpl.html         */
#include "qcairoframe"
#include <math.h>

QCairoFrame::CairoBack::CairoBack(QCairoFrame* parent)
{
  my_cairo_frame=parent;  
}

const double QCairoFrame::zoom_factor[13]={0.125, M_SQRT1_2/4.0, 0.25, 
                                           M_SQRT1_2/2.0, 0.5, M_SQRT1_2, 
                                           1.0,
                                           M_SQRT2, 2.0, M_SQRT2*2.0, 
                                           4.0, M_SQRT2*4.0, 8.0};
const unsigned QCairoFrame::zoom_steps=13; 

void
QCairoFrame::paintEvent(QPaintEvent* pe)
{
  QMutexLocker c_target_is_safe(&c_target_lock);
  if(!c_target) return;
  else c_target->update();
  
}

void
QCairoFrame::cairoDraw(cairo_t* v_buf)
{
  tmp_c=v_buf;
  c_draw_lock.lock();
  CairoItem *cur;
  for(cur=drw_list.first();cur;cur=drw_list.next()) cur->cairoDraw();
  c_draw_lock.unlock();
  tmp_c=0;
}

void 
QCairoFrame::resizeEvent(QResizeEvent* re)
{
  QMutexLocker c_target_is_safe(&c_target_lock);
  if(!c_target) return;
  c_target->resize(re->size().width(),re->size().height());
}

void 
QCairoFrame::mousePressEvent(QMouseEvent* me)
{
  if(me->button()==LeftButton && me->state()==0) 
  {
    /* pan scene: LeftButton + NoModifier */
    active_state=Pan;
    saved_cursor=cursor();
    //setCursor(KCursor::handCursor());
    setCursor(KCursor::sizeAllCursor());
    c_target_lock.lock();
    pan_start_point=me->pos();
    c_target_lock.unlock();
  }
}

void 
QCairoFrame::mouseMoveEvent(QMouseEvent* me)
{  
  switch(active_state)
  {
    case Pan:
      if(rect().contains(me->pos()))
      {
        c_target_lock.lock();
        current_offset=me->pos()-pan_start_point;
        pan_start_point=me->pos();                
        c_target->translateAbs((double)current_offset.x(),
                               (double)current_offset.y());
        c_target_lock.unlock();
        update();
      }
      break;
    default:
      break;
  }
}
  
void 
QCairoFrame::mouseReleaseEvent(QMouseEvent* me)
{
  switch(active_state)
  {
    case Pan:
      setCursor(saved_cursor);
      active_state=Idle;
      break;
    default:
      break;
  }
}

void 
QCairoFrame::keyPressEvent(QKeyEvent* ke)
{  
  unsigned zxo, zyo; double da;
  switch(ke->key())
  {
    case Key_Home:      
      c_target_lock.lock();
      c_target->setDefault();      
      zx=4; zy=4;
      ra=0.0;
      c_target_lock.unlock();
      update();
      break;
    case Key_PageDown:
      c_target_lock.lock();
      zxo=zx; zyo=zy;
      zx=(zx==0)?0:zx-1; zy=(zy==0)?0:zy-1;             
      c_target->scale(zoom_factor[zoom_steps-zxo-1]*zoom_factor[zx],
                      zoom_factor[zoom_steps-zyo-1]*zoom_factor[zy]);
      c_target_lock.unlock();
      update();            
      break;
    case Key_PageUp:
      c_target_lock.lock();
      zxo=zx; zyo=zy;
      zx=(zx==8)?8:zx+1; zy=(zy==8)?8:zy+1; 
      c_target->scale(zoom_factor[zoom_steps-zxo-1]*zoom_factor[zx],
                      zoom_factor[zoom_steps-zyo-1]*zoom_factor[zy]);       
      c_target_lock.unlock();      
      update();
      break;
    case Key_Less:
      c_target_lock.lock();
      da=M_PI/18.0; ra+=da;
      c_target->rotate(da);      
      c_target_lock.unlock();
      update();
      break;
    case Key_Greater:
      c_target_lock.lock();
      da=-M_PI/18.0; ra+=da;
      c_target->rotate(da); 
      c_target_lock.unlock();
      update();
      break;
    default:
      ke->ignore();  
      break;
  }  
}

QCairoFrame::QCairoFrame(QWidget* parent, const char* name, WFlags f) 
            :QWidget(parent,name,f)
{
  c_target=new CairoGlitz(this,"Glitz Area");
  zx=4; zy=4; ra=0.0; tmp_c=0;
  setFocusPolicy(StrongFocus); 
}

QCairoFrame::~QCairoFrame()
{
}

cairo_t*
QCairoFrame::cairoTarget()
{
  return tmp_c;
}

cairo_surface_t*
QCairoFrame::createImage(const QImage& qimg)
{
  QMutexLocker c_target_is_safe(&c_target_lock);  
  return c_target->createImage(qimg);  
}

void
QCairoFrame::destroySurface(cairo_surface_t* s)
{
  c_target_lock.lock();
  c_target->destroySurface(s);
  c_target_lock.unlock();
}
  
void 
QCairoFrame::appendItem(CairoItem* ci)
{
  c_draw_lock.lock();
  if(ci) drw_list.append(ci);
  c_draw_lock.unlock();
}

void 
QCairoFrame::removeItem(CairoItem* ci)
{
  c_draw_lock.lock();
  if(ci) drw_list.remove(ci);
  c_draw_lock.unlock();
}
  
#include "qcairoframe.moc"
